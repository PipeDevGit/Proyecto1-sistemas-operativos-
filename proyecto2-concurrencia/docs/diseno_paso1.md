# Paso 1 — Diseño del buffer compartido y del detector de corrupción

> Proyecto 2 — Simulador de Concurrencia (Productor-Consumidor)
> TIIT2007 Sistemas Operativos · Universidad Invenio · Isaac Felipe Morún Moreira
> Documento de diseño previo a la implementación. Cada decisión lleva su justificación,
> porque es lo que se pregunta en la Defensa Técnica.

---

## 1. El ítem que circula por el buffer

```cpp
struct Item {
    int  productor_id;   // qué hilo lo produjo
    long secuencia;      // n-ésimo ítem de ESE productor
    long id_unico;       // productor_id * 1000000 + secuencia
    long verificacion;   // suma de control calculada al producir
};
```

**Por qué un struct de varios campos y no un simple `int`.** En x86-64 una escritura alineada
de 8 bytes es atómica a nivel de hardware: si el ítem fuera un solo `long`, nunca se vería un
valor "a medias", solo ítems perdidos o sobrescritos. Con cuatro campos, dos productores pueden
entrelazar sus escrituras **campo por campo**: el `productor_id` de uno queda junto a la
`secuencia` del otro. El resultado es un ítem *que nunca existió*.

**Por qué el campo `verificacion`.** Se calcula al producir a partir de los otros campos.
Si al consumir el recálculo no coincide, ese ítem es una combinación fabricada por el
entrelazado — no lo produjo nadie. Es la evidencia más contundente de "datos corruptos" en el
sentido literal del enunciado, y es imposible de confundir con un error de conteo.

---

## 2. El buffer

```cpp
class BufferCompartido {
    Item datos[CAPACIDAD];
    int  cabeza;   // dónde escribe el productor
    int  cola;     // dónde lee el consumidor
    int  cuenta;   // cuántos ítems hay  <-- la variable más frágil
};
```

**`cuenta` es el corazón de la sección crítica.** `cuenta++` parece una instrucción pero son
tres: leer, sumar, escribir. Es exactamente el caso que el profesor describió en clase
(*"parece que solamente hay un paso"*) y el de la lámina 3 de la Unidad III, donde dos hilos
leen 0, ambos escriben 1, y se pierde un incremento.

**Indexación con `% CAPACIDAD`.** Con los índices corruptos, indexar sin módulo produce
escritura fuera de rango y el programa muere con segfault — una demo que crashea en vez de
mostrar la corrupción. El módulo es aritmética estándar de buffer circular, no un parche
defensivo. *Hay que poder decir exactamente eso si lo preguntan.*

**Capacidad pequeña (8 por defecto).** Menos espacio = más contención = la carrera se
manifiesta más. Configurable por línea de comandos para poder reportar cómo varía.

---

## 3. El detector — y por qué no puede ser él mismo racy

Este es el punto que decide si la evidencia vale o no. Si los consumidores registraran lo que
consumen en una estructura compartida, ese registro también sufriría carreras y los números del
informe no probarían nada. El profesor lo va a notar.

**Regla de diseño: durante la corrida nadie comparte nada para medir.**

- Cada **productor** acumula los `id_unico` que generó en un vector **privado suyo**.
- Cada **consumidor** acumula los ítems que consumió en un vector **privado suyo**.
- La reconciliación se hace **después de todos los `join()`**, en un solo hilo.

Así el fenómeno medido es no determinista, pero **la medición es determinista**.

### Métricas que salen de la reconciliación

| Métrica | Cómo se obtiene (post-`join`, un solo hilo) |
|---|---|
| Producidos | suma de los tamaños de los vectores de productores |
| Consumidos | suma de los tamaños de los vectores de consumidores |
| **Perdidos** | ids producidos que no aparecen en ningún consumidor |
| **Duplicados** | ids consumidos más de una vez |
| **Fantasma** | ids consumidos que nunca fueron producidos |
| **Corruptos** | ítems cuya `verificacion` no cuadra al recalcularla |
| **Violación de capacidad** | máximo de `cuenta` observado > CAPACIDAD |

La violación de capacidad sí se registra durante la corrida, pero cada hilo guarda su propio
máximo local y se fusionan al final. La lectura de `cuenta` es racy — y ese es justamente el
punto que se quiere mostrar.

---

## 4. Terminación: el problema que puede colgar la versión A

Si los consumidores esperaran a que un contador compartido llegue a cero, con `cuenta`
corrupta podrían esperar para siempre. La versión A se colgaría en vez de terminar.

**Solución:** cada consumidor tiene una **cuota fija** (`total_items / n_consumidores`), decidida
antes de arrancar. No depende de ningún estado compartido, así que la terminación nunca depende
de datos corrompidos.

**Más un perro guardián:** un tiempo límite por corrida. Si se agota, la corrida se marca como
`TIMEOUT` y se reporta como tal. Hay que documentarlo explícitamente en el IEEE: **el límite no
crea ni evita la corrupción, solo impide que el proceso quede colgado**. Presentarlo como
manejo de errores (cuenta para "Fiabilidad" en ISO 25010), nunca esconderlo.

---

## 5. Las cuatro variantes

| Var | Sincronización | Tipo de espera | Qué demuestra |
|---|---|---|---|
| **A** | ninguna | activa | la condición de carrera (requisito 3) |
| **B** | `std::mutex` | activa | **overhead puro del mutex** (requisito 4) |
| **C** | `std::mutex` + `std::condition_variable` | duerme | eliminación de la espera activa |
| **D** | semáforos (`vacios`, `llenos`, `mutex`) | duerme | la solución clásica de Dijkstra |

### Por qué cuatro y no dos

**El overhead medido entre A y C sería un número falso.** Dos razones:

1. La versión A **pierde ítems**, así que hace menos trabajo y puede terminar antes. El tiempo
   total no compara lo mismo.
2. A quema CPU en espera activa mientras C duerme. C puede salir *más rápida* que A, dando un
   "overhead negativo" imposible de explicar.

Con las cuatro, cada comparación aísla una sola cosa:

- **B vs. A** → costo del mutex, con la *misma* estrategia de espera. Este es el overhead real.
- **C vs. B** → efecto de dejar de quemar CPU. Responde directamente a la pregunta de la
  lámina 9 de la Unidad III: *"¿por qué el kernel usa espera activa en lugar de bloquear el hilo?"*
  — la variante B **es** el análogo en espacio de usuario del `spinlock` de xv6.
- **D vs. C** → dos mecanismos correctos, distinta arquitectura.

Y se reporta **tiempo por ítem transferido correctamente**, no tiempo total, para que la
comparación sea justa aunque A pierda datos.

### Sobre el semáforo de la variante D

Se implementa **a mano** sobre `mutex` + `condition_variable` (unas 15 líneas), en vez de usar
`std::counting_semaphore`. Tres razones:

1. Mantiene todo el proyecto en **C++17**, igual que el P1, sin arriesgar la compilación en
   MinGW y MSVC (`std::counting_semaphore` es C++20 — verificado: compila en la VM con
   `-std=c++20`, falla con `-std=c++17`).
2. Construir un semáforo desde cero demuestra que se entiende **qué es**, no solo cómo llamarlo.
3. Permite nombrar las operaciones `wait`/`signal` (P/V) como las nombró Dijkstra, a quien el
   profesor atribuyó el concepto en clase.

Se deja constancia en el IEEE de que `std::counting_semaphore` existe desde C++20 y de por qué
no se usó.

---

## 6. Estructura de archivos

```
proyecto2-concurrencia/
├── src/
│   ├── Item.h                 # struct + suma de control
│   ├── Semaforo.h             # semáforo hecho a mano (C++17)
│   ├── BufferA.h              # sin sincronización      <-- ESTUDIANTE
│   ├── BufferB.h              # mutex + espera activa   <-- ESTUDIANTE
│   ├── BufferC.h              # mutex + condition_variable <-- ESTUDIANTE
│   ├── BufferD.h              # semáforos               <-- ESTUDIANTE
│   ├── Detector.h/.cpp        # reconciliación post-join
│   ├── Reporte.h/.cpp         # tablas ANSI + salida CSV
│   └── main.cpp               # CLI
├── scripts/experimentos.sh    # 5+ corridas x 4 variantes -> CSV
├── results/                   # CSV + logs crudos + reporte de TSan
└── Makefile
```

**Sin herencia ni funciones virtuales.** Una clase base con métodos virtuales metería una
llamada indirecta en el camino caliente y contaminaría la medición de tiempos. Las cuatro
variantes son clases independientes y `main` las selecciona con una función `template`. La
decisión es deliberada y es justo el tipo de cosa que se pregunta como "justificá tu diseño".

---

## 7. Interfaz de línea de comandos

```
./simulador --variante A|B|C|D --productores 4 --consumidores 4 \
            --capacidad 8 --items 50000 --timeout 10 --csv results/corridas.csv [--sin-color]
```

`--sin-color` existe porque la consola legacy de Windows (`conhost`) puede no interpretar los
códigos ANSI. Cuenta para "Usabilidad" en ISO 25010.

---

## 8. Cobertura de la Unidad III — qué entra, qué no, y por qué

En el Proyecto 1 el profesor preguntó por qué no se incluyó algo que sí se había visto en clase
(SJF y planificación por prioridades, que el enunciado no pedía). Esta tabla existe para que esa
pregunta ya tenga respuesta escrita antes de que la haga.

| Visto en clase (Unidad III) | ¿En el proyecto? | Justificación |
|---|---|---|
| Condiciones de carrera (lám. 3) | **Sí** — variante A | requisito 3 del enunciado |
| Sección crítica / exclusión mutua (lám. 4) | **Sí** — comentarios en A–D | requisito no funcional explícito |
| Mutex (lám. 5) | **Sí** — variantes B y C | requisito 4 |
| **Semáforos (lám. 5)** | **Sí** — variante D | *No lo pide el enunciado.* Se incluye para cubrir la lámina completa y contrastar la propiedad del mutex contra la no-propiedad del semáforo |
| Productor-Consumidor (lám. 6) | **Sí** | es el proyecto |
| Lectores-Escritores (lám. 6) | **No** | patrón distinto (varios lectores simultáneos). El enunciado fija Productor-Consumidor. Se menciona en "trabajo futuro" del IEEE |
| Filósofos Comensales (lám. 6) | **Parcial** — demo de deadlock | no se construye el simulador completo, pero la demo de dos mutex en orden inverso reproduce la **espera circular**, que es lo que el caso ilustra |
| Condiciones de Coffman (lám. 7) | **Sí** — IEEE + demo | las cuatro se identifican sobre la demo de deadlock |
| Prevención por ordenamiento (lám. 8) | **Sí** — corrección de la demo | se ordena la adquisición y el deadlock desaparece |
| Algoritmo del banquero (lám. 8) | **No** | es evitación con asignación de múltiples recursos: corresponde al **Proyecto 4** (administrador de recursos), no a un buffer de un solo tipo |
| xv6 `spinlock` (lám. 9) | **Sí** — IEEE | la variante B es el análogo en espacio de usuario; se compara `acquire()`/`release()` contra `std::mutex` |
| Laboratorio Bash/PowerShell (lám. 10) | **Sí** — anexo | cubre el componente "Profesional" de la Semana 6 (`flock` en Linux, Mutex de .NET en Windows) |

---

## 9. Evidencia con ThreadSanitizer

`g++ -fsanitize=thread` detecta data races en tiempo de ejecución e imprime archivo, línea y los
dos accesos que colisionan. Verificado que funciona en la VM.

- Variante A → reporte de carrera señalando la sección crítica desprotegida.
- Variantes B, C, D → sin hallazgos.

Es la respuesta a *"¿cómo sé que tu race condition es real y no la fabricaste?"*: **no la afirma
el autor, la certifica una herramienta externa.**

⚠️ TSan hace el programa entre 5 y 15 veces más lento. Las corridas con TSan son **solo para
evidencia, nunca para medir tiempos**. Los tiempos salen de binarios compilados sin sanitizers,
con los mismos flags en las cuatro variantes.

---

## 10. Nota sobre comportamiento indefinido

En C++ un *data race* es **comportamiento indefinido formal**, no simplemente "código
desprotegido". Con `-O2` el compilador puede sacar una lectura fuera del bucle y el programa
puede colgarse en vez de corromper datos.

Decisión: **compilar las cuatro variantes con exactamente los mismos flags** y documentar en el
IEEE que lo observado es la manifestación concreta de ese comportamiento indefinido en este
compilador (g++ 15.2.0) y esta arquitectura (x86-64) — lo cual, de paso, explica por qué se
espera que Windows produzca patrones de corrupción distintos.

Distinguir *data race* (dos accesos sin orden, uno de escritura: definición del lenguaje) de
*race condition* (el resultado depende del orden: definición del sistema operativo) es
exactamente el nivel de precisión que separa un 3 de un 4 en "Dominio conceptual".

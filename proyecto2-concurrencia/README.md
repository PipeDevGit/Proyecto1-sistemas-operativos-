# Proyecto 2 — Simulador de Concurrencia (Productor-Consumidor)

TIIT2007 – Sistemas Operativos · Universidad Invenio
**Isaac Felipe Morún Moreira**

Dos versiones de un simulador Productor-Consumidor sobre un buffer acotado, con
el mismo diseño lógico, los mismos parámetros y la misma instrumentación. La
única diferencia entre ellas es la presencia o ausencia de sincronización.

| Versión | Carpeta | Mecanismo |
|---|---|---|
| **A** | `version_a/` | ninguno: acceso directo y desprotegido |
| **B** | `version_b/` | `std::mutex` + `std::condition_variable` |

---

## Compilación

Un solo comando compila todo (RNF-6):

```bash
make
```

Cada versión compila además **por separado**, sin depender de la otra:

```bash
make -C version_a      # produce version_a/version_a
make -C version_b      # produce version_b/version_b
```

Banderas usadas en todos los binarios:

```
-std=c++17 -Wall -Wextra -pthread -O2
```

No hay dependencias externas: solo la biblioteca estándar de C++17.
Compila sin advertencias.

---

## Ejecución

Ambas versiones aceptan **exactamente los mismos parámetros**, de modo que la
comparación entre ellas sea válida.

```bash
./version_a/version_a [opciones]
./version_b/version_b [opciones]
```

| Opción | Por defecto | Descripción |
|---|---|---|
| `--capacidad N` | 10 | tamaño del buffer (N) |
| `--productores N` | 2 | hilos productores, mínimo 2 |
| `--consumidores N` | 2 | hilos consumidores, mínimo 2 |
| `--items N` | 5000 | ítems por productor |
| `--csv archivo` | — | anexa una fila con los resultados |
| `--log archivo` | — | vuelca el log de eventos |
| `--sin-color` | — | desactiva los códigos ANSI |
| `--ayuda` | — | muestra la ayuda |

El total a producir es `productores × items` y **se imprime al inicio de la
corrida**, antes de lanzar los hilos.

**Código de salida:** `0` si la verificación de invariantes pasó, `2` si falló.
Permite encadenar corridas en scripts sin analizar la salida de texto.

### Ejemplo

```bash
./version_b/version_b --capacidad 10 --productores 4 --consumidores 4 --items 5000
```

---

## Verificación de invariantes

Al final de cada corrida el programa reporta automáticamente:

- ítems producidos y consumidos
- **suma de los valores producidos y consumidos** (invariante fuerte)
- ítems perdidos, duplicados, fantasma y corruptos
- ocupación máxima observada y si excedió N

La corrida se marca `INVARIANTE: OK` solo si producidos = consumidos, las sumas
coinciden, la ocupación nunca excedió N y no hubo inconsistencias.

Cada ítem lleva un `valor` entero cuya suma se compara entre producción y
consumo, como pide el enunciado. El proyecto añade además un identificador único
por ítem y una suma de verificación derivada de sus campos: la suma sola no
distingue *cuáles* ítems se perdieron ni detecta el caso en que una pérdida y
una duplicación se cancelen entre sí.

---

## Protocolo experimental

```bash
make experimentos
```

Ejecuta 5 corridas por versión en cada una de estas cuatro configuraciones, con
el **total de ítems fijo en 20 000** como variable controlada:

| N | Productores | Consumidores | Ítems por productor |
|---|---|---|---|
| 5 | 2 | 2 | 10 000 |
| 5 | 4 | 4 | 5 000 |
| 50 | 2 | 2 | 10 000 |
| 50 | 4 | 4 | 5 000 |

Deja los resultados en `evidencia/resultados.csv` y la salida de cada corrida en
`evidencia/corrida_*.txt`. Después imprime las dos tablas obligatorias y el
análisis de overhead.

El generador de reportes se ejecuta también por separado sobre cualquier CSV:

```bash
./herramientas/reporte evidencia/resultados.csv [--ascii] [--sin-color]
```

`--ascii` sustituye los caracteres de recuadro Unicode; combinado con
`--sin-color` la salida no contiene bytes fuera del rango ASCII.

---

## Evidencia de la condición de carrera

```bash
make evidencia
```

Corre la versión A con parámetros pequeños (N=5, 2P/2C, 25 ítems) para que el
log sea legible, y produce:

| Archivo | Contenido |
|---|---|
| `evidencia/log_version_a.txt` | log de eventos: qué hilo hizo qué, cuándo, en qué posición y cómo cambió la ocupación |
| `evidencia/carrera_explicada.txt` | dos casos concretos localizados en ese log, con la explicación de por qué ese intercalado produce el error |
| `evidencia/salida_carrera.txt` | salida completa de la corrida |

El log se recoge en estructuras **privadas de cada hilo** y se ordena por marca
de tiempo después de unirlos: de otro modo el propio registro sufriría
condiciones de carrera y la evidencia no probaría nada. El orden resultante es
aproximado, ya que dos eventos separados por nanosegundos pueden aparecer
invertidos.

### ThreadSanitizer (opcional)

```bash
make tsan
```

Compila binarios instrumentados y contrasta lo que reporta el detector en cada
versión. Los binarios con ThreadSanitizer son entre 5 y 15 veces más lentos, así
que **ningún tiempo del informe proviene de ellos**.

---

## Extras (no exigidos por el enunciado)

```bash
./extras/extras --mecanismo espera-activa   # mutex, girando en vez de dormirse
./extras/extras --mecanismo semaforos       # tres semáforos, solución de Dijkstra
```

Ambos mecanismos son **correctos**: no exhiben condiciones de carrera. Existen
para sostener con datos propios dos puntos del documento: el costo medido de la
espera activa, que es lo que RNF-5 exige para admitirla, y el contraste
experimental entre mutex y semáforo.

> **`extras/demo_deadlock` se bloquea a propósito** en su modo por defecto. Es
> una demostración independiente del temario de interbloqueos y **no tiene
> relación con la versión B**, que no puede interbloquearse. Se incluye porque
> la unidad cubre las condiciones de Coffman y las estrategias de manejo.

```bash
./extras/demo_deadlock --modo deadlock   # se traba (detectado y reportado)
./extras/demo_deadlock --modo orden      # previene ordenando la adquisición
./extras/demo_deadlock --modo scoped     # previene con std::scoped_lock
```

---

## Estructura

```
proyecto2-concurrencia/
├── common/         código compartido por ambas versiones
│   ├── Item.h          ítem, valor, suma de verificación
│   ├── Registro.h/.cpp instrumentación, reconciliación y log
│   └── Ejecutor.h      orquestación, argumentos y salida
├── version_a/      buffer SIN sincronización  + Makefile propio
├── version_b/      buffer CON sincronización  + Makefile propio
├── extras/         mecanismos adicionales y demostración de interbloqueo
├── herramientas/   generador de reportes
├── scripts/        protocolo experimental y análisis del log
├── evidencia/      CSV, logs y salidas de las corridas
├── docs/           documento IEEE
├── Makefile
└── README.md
```

---

## Entorno de prueba

| | Entorno 1 | Entorno 2 |
|---|---|---|
| Sistema | Ubuntu 26.04 LTS sobre VirtualBox | Windows 11 Pro |
| Compilador | g++ 15.2.0 | g++ 16.2.0 (MinGW-w64 UCRT) |
| Núcleos lógicos | 6 | 16 |
| Banderas | `-std=c++17 -Wall -Wextra -pthread -O2` | idénticas |

En Linux `-pthread` es necesario. En MinGW-w64 con modelo de hilos POSIX es
inocuo —se comprobó comparando los binarios y sus dependencias— pero se conserva
por portabilidad.

**Nota sobre las mediciones.** Los tiempos absolutos derivan entre sesiones en
entornos virtualizados. Las corridas de cada configuración se ejecutan de forma
consecutiva y se reportan media, desviación estándar, mínimo y máximo, nunca un
valor aislado.

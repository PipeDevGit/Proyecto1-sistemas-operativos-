# Proyecto 3 — Simulador de Gestión de Memoria

TIIT2007 – Sistemas Operativos · Universidad Invenio
**Isaac Felipe Morún Moreira**

Simulador de reemplazo de páginas que compara **FIFO** y **LRU** sobre la misma
cadena de referencias, con el número de marcos configurable. Incluye además el
algoritmo **ÓPTIMO** como cota inferior teórica (no exigido por el enunciado).

---

## Compilación

```bash
make
```

Banderas: `-std=c++17 -Wall -Wextra -O2`. Sin dependencias externas, solo la
biblioteca estándar. Compila sin advertencias.

---

## Ejecución

```bash
./simulador [opciones]
```

| Opción | Por defecto | Descripción |
|---|---|---|
| `--cadena archivo` | `data/corta.txt` | archivo con la cadena de referencias |
| `--marcos N` | 3 | número de marcos de memoria física |
| `--algoritmo A` | `todos` | `fifo`, `lru`, `optimo` o `todos` |
| `--traza` | — | imprime el estado de los marcos paso a paso |
| `--csv archivo` | — | anexa una fila por algoritmo con los resultados |
| `--sin-color` | — | desactiva los códigos ANSI |
| `--ayuda` | — | muestra la ayuda |

Ningún parámetro requiere recompilar.

**Validaciones.** `--marcos` debe ser al menos 1 y no puede superar el número
de referencias de la cadena: más marcos que referencias no puede cambiar el
resultado, porque cada referencia carga a lo sumo una página. Si a una opción le
falta su valor, el mensaje lo dice explícitamente en lugar de tratarla como
opción desconocida.

**Códigos de salida.** `0` correcto · `1` error de argumentos o de lectura ·
`2` incoherencia interna en las métricas · `3` memoria insuficiente.

### Ejemplos

```bash
# Traza paso a paso de la cadena canónica
./simulador --cadena data/corta.txt --marcos 3 --traza

# Solo FIFO, con 5 marcos
./simulador --cadena data/larga_localidad.txt --marcos 5 --algoritmo fifo
```

---

## Formato de la cadena de referencias

Números de página separados por espacios, comas o saltos de línea. Las líneas
que empiezan con `#` son comentarios, de modo que cada archivo documenta su
propio origen.

Una entrada inválida **no aborta la lectura**: se reporta por `stderr`
indicando el número de línea y se descarta, así un archivo parcialmente
corrupto sigue siendo utilizable y el usuario se entera de qué se ignoró.

---

## Cadenas incluidas

| Archivo | Referencias | Para qué |
|---|---|---|
| `data/corta.txt` | 20 | Traza manual. Es la cadena canónica de Silberschatz, la misma de la lámina 7 del curso |
| `data/belady.txt` | 12 | Demuestra la **anomalía de Belady** con FIFO |
| `data/localidad_corta.txt` | 24 | Localidad marcada, para inspección a ojo |
| `data/larga_localidad.txt` | 1200 | Conjunto de trabajo que se desplaza — donde LRU debería ganar |
| `data/larga_secuencial.txt` | 1200 | Barrido lineal repetido — el peor caso |
| `data/larga_aleatoria.txt` | 1200 | Sin localidad — donde no debería haber ventaja clara |

Las tres cadenas largas se generan con semilla fija y son reproducibles:

```bash
./herramientas/generar localidad 1200 20 42 > data/larga_localidad.txt
```

Cada archivo lleva en su cabecera el comando exacto que lo regenera.

---

## Protocolo experimental

```bash
./scripts/experimentos.sh
./herramientas/reporte evidencia/resultados.csv
```

Ejecuta las 6 cadenas × hasta 6 tamaños de marco × 3 algoritmos, dejando los
resultados en `evidencia/resultados.csv`. El reporte produce:

1. **Tabla de fallos** por algoritmo y tamaño de marco, marcando el mejor
2. **Gráfico de barras** de fallos frente al tamaño de marco
3. **Detección automática de la anomalía de Belady**
4. **Tiempos de simulación** por combinación

`--ascii` sustituye los caracteres de recuadro Unicode; combinado con
`--sin-color` la salida no contiene bytes fuera del rango ASCII.

---

## Validación

Los tres algoritmos se contrastan contra los valores publicados en la
literatura para la cadena canónica `7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1`
con 3 marcos:

| Algoritmo | Fallos esperados | Fallos obtenidos |
|---|---|---|
| FIFO | 15 | **15** |
| LRU | 12 | **12** |
| ÓPTIMO | 9 | **9** |

Es una verificación externa: los números no dependen de que la implementación
sea correcta "según su autor", sino de coincidir con una referencia publicada.

El simulador comprueba además en cada corrida que `aciertos + fallos =
referencias`, y devuelve código de salida 2 si esa identidad no se cumple.

---

## Estructura

```
proyecto3-memoria/
├── common/
│   ├── Cadena.h        lectura y validación de la cadena de referencias
│   └── Simulacion.h    tipos comunes, métricas y motor de simulación
├── src/
│   ├── FIFO.h          reemplazo por antigüedad de carga
│   ├── LRU.h           reemplazo por antigüedad de uso
│   ├── Optimo.h        cota inferior teórica (extra)
│   └── main.cpp        línea de comandos, traza y salida
├── herramientas/
│   ├── generar.cpp     generador de cadenas sintéticas
│   └── reporte.cpp     tablas y gráficos desde los CSV
├── scripts/            protocolo experimental
├── data/               cadenas de referencia
├── evidencia/          CSV y salidas de las corridas
├── docs/               documento IEEE
├── Makefile
└── README.md
```

---

## Entorno de prueba

| | Entorno 1 | Entorno 2 |
|---|---|---|
| Sistema | Ubuntu 26.04 LTS (VirtualBox) | Windows 11 Pro |
| Compilador | g++ 15.2.0 | g++ 16.2.0 (MinGW-w64 UCRT) |
| Núcleos lógicos | 6 | 16 |
| Banderas | `-std=c++17 -Wall -Wextra -O2` | idénticas |

El protocolo completo se ejecutó en ambos entornos y las **72 combinaciones
produjeron fallos y aciertos idénticos**, sin una sola discrepancia. Como el
simulador es determinista y de un solo hilo, esa coincidencia exacta es la
verificación de compatibilidad más exigente posible.

**Nota sobre los tiempos.** La simulación es de un solo hilo y sin
entrada/salida, así que los tiempos son estables entre corridas. Aun así se
reportan por combinación y no como un valor único, porque el costo depende del
algoritmo: FIFO y LRU son O(1) por referencia, mientras que ÓPTIMO es
O(marcos) por fallo al buscar la víctima.

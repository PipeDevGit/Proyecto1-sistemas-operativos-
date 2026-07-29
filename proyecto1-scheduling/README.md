# Proyecto 1 — Simulador de Scheduling

Simulador de los algoritmos de planificación de CPU **FCFS** (First Come, First Served) y
**Round Robin** con quantum configurable, desarrollado en C++17.

## Requisitos

- Compilador con soporte C++17 (probado con `g++ 15.2.0` en Ubuntu 26.04 LTS).
- No requiere bibliotecas externas ni dependencias de plataforma — solo biblioteca estándar de C++.

## Compilación

```bash
make
```

Esto genera el ejecutable `simulador` a partir de los fuentes en `src/`. Para limpiar los binarios:

```bash
make clean
```

Compilación manual equivalente (sin `make`):

```bash
g++ -std=c++17 -Wall -Isrc -o simulador src/*.cpp
```

## Ejecución

```bash
./simulador <archivo_de_procesos> <quantum_round_robin>
```

Ejemplos:

```bash
./simulador data/procesos.txt 2      # dataset de 10 procesos, quantum=2
./simulador data/procesos.txt 4      # mismo dataset, quantum=4
./simulador data/procesos50.txt 2    # dataset extendido de 50 procesos
```

Atajos de conveniencia (compilan si hace falta y ejecutan por vos):

```bash
make run     # corre data/procesos.txt con quantum=2
make demo    # corre las 4 combinaciones usadas en el documento IEEE (10 y 50 procesos, q=2 y q=4)
```

El programa corre FCFS y Round Robin sobre el mismo dataset y quantum indicados, e imprime para cada
algoritmo: diagrama de Gantt ASCII, tiempos de espera/retorno por proceso, promedios y % de
utilización de CPU.

## Datasets (`data/`)

| Archivo | Descripción |
|---|---|
| `procesos.txt` | 10 procesos sintéticos (requisito mínimo del enunciado) |
| `procesos50.txt` | 50 procesos sintéticos, generados con semilla fija `20260729` (ampliación voluntaria, no exigida por el enunciado) |
| `procesos_errores.txt` | Dataset con líneas deliberadamente malformadas, usado para verificar el manejo de errores del lector |

## Estructura del código (`src/`)

| Archivo | Responsabilidad |
|---|---|
| `Proceso.h` | Estructura de datos de un proceso |
| `Lector.h/.cpp` | Lectura y validación del archivo de entrada |
| `FCFS.h/.cpp` | Algoritmo FCFS |
| `RoundRobin.h/.cpp` | Algoritmo Round Robin |
| `Simulacion.h` | Tipos compartidos entre algoritmos (`Segmento`, `ResultadoSimulacion`) |
| `Metricas.h/.cpp` | Cálculo de promedios y utilización de CPU |
| `Gantt.h/.cpp` | Generación del diagrama de Gantt ASCII |
| `main.cpp` | Punto de entrada: orquesta lectura, ambos algoritmos e impresión de resultados |

## Resultados (`results/`)

- `simulacion_completa.txt` — salida completa para el dataset de 10 procesos (q=2 y q=4).
- `simulacion_50procesos.txt` — salida completa para el dataset de 50 procesos (q=2 y q=4).
- `comparativa.txt` — tabla resumen de promedios y utilización de CPU entre FCFS y Round Robin.

## Documento IEEE (`docs/`)

Ver `docs/documento_ieee.pdf`.

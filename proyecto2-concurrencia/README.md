# Proyecto 2 — Simulador de Concurrencia (Productor-Consumidor)

TIIT2007 – Sistemas Operativos · Universidad Invenio
Isaac Felipe Morún Moreira

Implementación en C++17 del problema Productor-Consumidor con hilos reales
(`std::thread`), en cuatro variantes que comparten la misma estructura de datos y
difieren únicamente en su mecanismo de sincronización.

## Requisitos

- Compilador con soporte de C++17 (`g++` 15 o superior; verificado también con `g++` 16.2.0 en MinGW-w64 UCRT)
- `make`
- Opcional, solo para la evidencia de carreras: `g++` con ThreadSanitizer

En Linux hace falta `-pthread`. En MinGW-w64 con modelo de hilos POSIX la bandera
es inocua (se verificó comparando binarios y dependencias), pero se conserva por
portabilidad.

## Compilación

```bash
make            # simulador, demo_deadlock y reporte
make tsan       # binario aparte instrumentado con ThreadSanitizer
make clean
```

Se compila con `-Wall -Wextra` y no produce advertencias.

## Las cuatro variantes

| Var | Mecanismo | Tipo de espera | Qué demuestra |
|-----|-----------|----------------|---------------|
| A | ninguno | activa | la condición de carrera |
| B | `std::mutex` | activa | costo del mutex (B contra A) |
| C | `mutex` + `condition_variable` | bloqueante | efecto de no girar (C contra B) |
| D | tres semáforos | bloqueante | arquitectura del mecanismo (D contra C) |

La sección crítica está delimitada explícitamente en comentarios dentro de cada
`BufferX.h`.

## Ejecución

```bash
./simulador --variante A|B|C|D [opciones]
```

| Opción | Por defecto | Descripción |
|--------|-------------|-------------|
| `--productores N` | 4 | hilos productores (mínimo 2) |
| `--consumidores N` | 4 | hilos consumidores (mínimo 2) |
| `--capacidad N` | 8 | posiciones del buffer |
| `--items N` | 50000 | ítems totales a producir |
| `--timeout N` | 10 | watchdog en segundos |
| `--idle-ms N` | 500 | espera sin progreso antes de rendirse |
| `--csv archivo` | — | anexa una fila con los resultados |
| `--sin-color` | — | desactiva los códigos ANSI |

## Atajos

```bash
make experimentos   # 5 corridas x 4 variantes y muestra el reporte
make race           # evidencia de condición de carrera con ThreadSanitizer
make deadlock       # los tres modos de la demostración de interbloqueo
```

## Reportes

El generador de reportes lee los CSV y arma las tablas y gráficos. Se ejecuta por
separado del simulador, de modo que las tablas del documento se regeneran sin
volver a correr los experimentos.

```bash
./reporte results/verificacion.csv
./reporte results/barrido_final.csv --ascii --sin-color
```

`--ascii` sustituye los recuadros Unicode por `+ - |`; combinado con `--sin-color`
la salida no contiene ningún byte fuera del rango ASCII, para consolas sin soporte
de codificación extendida.

## Demostración de interbloqueo

```bash
./demo_deadlock --modo deadlock   # orden inverso de adquisición: se traba
./demo_deadlock --modo orden      # mismo orden en ambos hilos: previene
./demo_deadlock --modo scoped     # std::scoped_lock: previene
```

El interbloqueo es real: no hay esperas artificiales, simplemente se repite la
operación hasta que el planificador produce la intercalación que lo provoca. Como
un hilo interbloqueado no puede unirse nunca, los hilos se desacoplan y se vigilan
desde afuera observando la falta de progreso.

## Estructura

```
proyecto2-concurrencia/
├── src/
│   ├── Item.h              estructura del ítem y suma de verificación
│   ├── BufferA.h           sin sincronización
│   ├── BufferB.h           mutex con espera activa
│   ├── BufferC.h           mutex + variable de condición
│   ├── BufferD.h           tres semáforos
│   ├── Semaforo.h          semáforo contador construido a mano
│   ├── Registro.h/.cpp     instrumentación y reconciliación
│   ├── main.cpp            orquestación y línea de comandos
│   ├── reporte.cpp         generación de tablas y gráficos
│   └── deadlock.cpp        demostración de interbloqueo
├── results/                CSV crudos y registros de ThreadSanitizer
├── docs/                   documento IEEE y diseño
└── Makefile
```

## Nota sobre la medición

Los tiempos absolutos derivan considerablemente entre sesiones en entornos
virtualizados, porque la espera activa es muy sensible a la disponibilidad real de
procesador. Todas las comparaciones se hacen intercalando las variantes dentro de
una misma tanda y se reportan medianas y cocientes, nunca tiempos absolutos
aislados.

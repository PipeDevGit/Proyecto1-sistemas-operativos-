# TIIT2007 — Sistemas Operativos

**Isaac Felipe Morún Moreira** — Universidad Invenio
Licenciatura en Tecnologías de Información y Comunicación Empresarial

Repositorio individual de los proyectos del curso TIIT2007 (Sistemas Operativos).

## Proyectos

| Proyecto | Carpeta | Estado |
|---|---|---|
| Proyecto 1 — Simulador de Scheduling | [`proyecto1-scheduling/`](proyecto1-scheduling/) | Entregado |
| Proyecto 2 — Simulador de Concurrencia | [`proyecto2-concurrencia/`](proyecto2-concurrencia/) | Entregado |
| Proyecto 3 — Gestión de Memoria | [`proyecto3-memoria/`](proyecto3-memoria/) | Entregado |
| Proyecto 4 — Administrador de Recursos | [`proyecto4-administrador-recursos/`](proyecto4-administrador-recursos/) | Pendiente |

Cada proyecto tiene su propio `README.md` con instrucciones de compilación, parámetros aceptados
y entorno de prueba.

## Entorno de compilación

- Lenguaje: C++17, solo biblioteca estándar, sin dependencias externas.
- Entorno principal: Ubuntu 26.04 LTS sobre VirtualBox (VM institucional del curso), `g++ 15.2.0`.
- Entorno secundario: Windows 11, `g++ 16.2.0` (MinGW-w64 UCRT vía MSYS2).

| Proyecto | Verificación multiplataforma |
|---|---|
| Proyecto 1 | Solo Linux. La compatibilidad con Windows se argumenta por ausencia de dependencias de plataforma, **no** por verificación directa. |
| Proyecto 2 | Compilado y ejecutado en ambos entornos. Evidencia en `proyecto2-concurrencia/evidencia/resultados_windows.csv`. |
| Proyecto 3 | Compilado y ejecutado en ambos entornos. Las **72 combinaciones produjeron resultados idénticos**; evidencia en `proyecto3-memoria/evidencia/resultados_windows.csv`. |

Ninguno de los proyectos produce advertencias bajo `-Wall -Wextra`.

## Declaración de uso de Inteligencia Artificial

Se utilizó Claude (Anthropic) como herramienta de apoyo, conforme a la Política de Uso de
Inteligencia Artificial del syllabus (sección 7), que la permite bajo la condición de que toda
solución entregada sea comprendida, explicada y defendida por el estudiante.

**Proyecto 1.** Explicación de conceptos de C++ y de los algoritmos de planificación, asistencia en
la configuración del entorno de desarrollo (VM, SSH, VS Code Remote), redacción asistida de parte
del código (estructura de datos, lectura de archivos, algoritmos FCFS/Round Robin, métricas,
generación de Gantt ASCII) y del primer borrador del documento IEEE.

**Proyecto 2.** El alcance de la asistencia fue mayor y se declara de forma explícita: se redactaron
con apoyo de la herramienta las versiones del buffer compartido, el esquema de instrumentación y
reconciliación, el generador de reportes, la demostración de interbloqueo y el documento IEEE. El
estudiante definió el alcance, revisó cada decisión de diseño con su justificación, ejecutó y validó
los experimentos, y detectó junto a la herramienta varias afirmaciones que los datos no sostenían,
las cuales fueron corregidas antes de la entrega.

**Proyecto 3.** Alcance equivalente al del Proyecto 2: se redactaron con apoyo de la herramienta las
implementaciones de FIFO, LRU y el algoritmo óptimo, el generador de cadenas sintéticas, el
generador de reportes y el documento IEEE. Se incorporó además una verificación externa —el
simulador reproduce los valores publicados en la literatura para la cadena canónica— de modo que la
corrección no depende del criterio del propio autor.

En los tres casos, todo el contenido fue revisado y comprendido por el estudiante, y puede ser
explicado y defendido en la Defensa Técnica Individual.

## Estructura del repositorio

```
TIIT2007-sistemas-operativos/
├── README.md
├── proyecto1-scheduling/
│   ├── src/            FCFS, Round Robin, metricas y Gantt
│   ├── data/           cargas de procesos sinteticas
│   ├── results/        salidas de las corridas
│   ├── docs/           documento IEEE
│   └── README.md
├── proyecto2-concurrencia/
│   ├── common/         codigo compartido por ambas versiones
│   ├── version_a/      buffer SIN sincronizacion
│   ├── version_b/      buffer CON sincronizacion
│   ├── extras/         mecanismos adicionales y demo de interbloqueo
│   ├── herramientas/   generador de reportes
│   ├── evidencia/      CSV, logs y salidas de las corridas
│   ├── docs/           documento IEEE
│   ├── Makefile
│   └── README.md
├── proyecto3-memoria/
│   ├── common/         lectura de cadenas y motor de simulacion
│   ├── src/            FIFO, LRU y algoritmo optimo
│   ├── herramientas/   generador de cadenas y de reportes
│   ├── data/           cadenas de referencia
│   ├── evidencia/      CSV, trazas y reportes
│   ├── docs/           documento IEEE
│   ├── Makefile
│   └── README.md
├── proyecto4-administrador-recursos/
├── invenio-fest/
└── portafolio/
```

# TIIT2007 — Sistemas Operativos

**Isaac Felipe Morún Moreira** — Universidad Invenio
Licenciatura en Tecnologías de Información y Comunicación Empresarial

Repositorio individual de los proyectos del curso TIIT2007 (Sistemas Operativos).

## Proyectos

| Proyecto | Carpeta | Estado |
|---|---|---|
| Proyecto 1 — Simulador de Scheduling | [`proyecto1-scheduling/`](proyecto1-scheduling/) | Entregado |
| Proyecto 2 — Simulador de Concurrencia | [`proyecto2-concurrencia/`](proyecto2-concurrencia/) | Entregado |
| Proyecto 3 — Gestión de Memoria | [`proyecto3-memoria/`](proyecto3-memoria/) | Pendiente |
| Proyecto 4 — Administrador de Recursos | [`proyecto4-administrador-recursos/`](proyecto4-administrador-recursos/) | Pendiente |

## Entorno de compilación

- Lenguaje: C++17, solo biblioteca estándar, sin dependencias externas.
- Entorno principal: Ubuntu 26.04 LTS sobre VirtualBox (VM institucional del curso), `g++ 15.2.0`.
- Entorno secundario: Windows 11, `g++ 16.2.0` (MinGW-w64 UCRT vía MSYS2).

El Proyecto 2 **se compiló y ejecutó en ambos entornos**, sin modificaciones y sin advertencias
bajo `-Wall -Wextra`; los resultados de esa verificación están en
`proyecto2-concurrencia/results/windows*.csv`. El Proyecto 1 se desarrolló y probó únicamente en
Linux, y su compatibilidad con Windows se argumenta por ausencia de dependencias de plataforma,
no por verificación directa.

## Declaración de uso de Inteligencia Artificial

Se utilizó Claude (Anthropic) como herramienta de apoyo, conforme a la Política de Uso de
Inteligencia Artificial del syllabus (sección 7), que la permite bajo la condición de que toda
solución entregada sea comprendida, explicada y defendida por el estudiante.

**Proyecto 1.** Explicación de conceptos de C++ y de los algoritmos de planificación, asistencia en
la configuración del entorno de desarrollo (VM, SSH, VS Code Remote), redacción asistida de parte
del código (estructura de datos, lectura de archivos, algoritmos FCFS/Round Robin, métricas,
generación de Gantt ASCII) y del primer borrador del documento IEEE.

**Proyecto 2.** El alcance de la asistencia fue mayor y se declara de forma explícita: se redactaron
con apoyo de la herramienta las cuatro variantes del buffer compartido, el esquema de
instrumentación y reconciliación, el generador de reportes, la demostración de interbloqueo y el
documento IEEE. El estudiante definió el alcance, revisó cada decisión de diseño con su
justificación, ejecutó y validó los experimentos, y detectó junto a la herramienta varias
afirmaciones que los datos no sostenían, las cuales fueron corregidas antes de la entrega.

En ambos casos, todo el contenido fue revisado y comprendido por el estudiante, y puede ser
explicado y defendido en la Defensa Técnica Individual.

## Estructura del repositorio

```
TIIT2007-sistemas-operativos/
├── README.md
├── proyecto1-scheduling/
│   ├── src/
│   ├── data/
│   ├── results/
│   ├── docs/
│   └── README.md
├── proyecto2-concurrencia/
│   ├── src/            cuatro variantes de sincronizacion + instrumentacion
│   ├── results/        CSV crudos y registros de ThreadSanitizer
│   ├── docs/           documento IEEE y documento de diseño
│   ├── Makefile
│   └── README.md
├── proyecto3-memoria/
├── proyecto4-administrador-recursos/
├── invenio-fest/
└── portafolio/
```

# TIIT2007 — Sistemas Operativos

**Isaac Felipe Morún Moreira** — Universidad Invenio
Licenciatura en Tecnologías de Información y Comunicación Empresarial

Repositorio individual de los proyectos del curso TIIT2007 (Sistemas Operativos).

## Proyectos

| Proyecto | Carpeta | Estado |
|---|---|---|
| Proyecto 1 — Simulador de Scheduling | [`proyecto1-scheduling/`](proyecto1-scheduling/) | Entregado |
| Proyecto 2 — Simulador de Concurrencia | [`proyecto2-concurrencia/`](proyecto2-concurrencia/) | Pendiente |
| Proyecto 3 — Gestión de Memoria | [`proyecto3-memoria/`](proyecto3-memoria/) | Pendiente |
| Proyecto 4 — Administrador de Recursos | [`proyecto4-administrador-recursos/`](proyecto4-administrador-recursos/) | Pendiente |

## Entorno de compilación

- Lenguaje: C++17
- Compilador de prueba: `g++ 15.2.0`
- Sistema operativo de prueba: Ubuntu 26.04 LTS (VM institucional del curso, VirtualBox)
- El código no usa dependencias específicas de plataforma; se espera que compile igual en
  Windows (MinGW/MSVC) sin modificaciones.

## Declaración de uso de Inteligencia Artificial

Se utilizó Claude (Anthropic) como herramienta de apoyo durante el desarrollo del Proyecto 1,
conforme a la Política de Uso de Inteligencia Artificial del syllabus (sección 7). Uso concreto:
explicación de conceptos de C++ y de los algoritmos de planificación, asistencia en la configuración
del entorno de desarrollo (VM, SSH, VS Code Remote), redacción asistida de parte del código
(estructura de datos, lectura de archivos, algoritmos FCFS/Round Robin, métricas, generación de
Gantt ASCII) y del primer borrador del documento IEEE. Todo el contenido fue revisado, comprendido
y puede ser explicado y defendido por el estudiante en la Defensa Técnica Individual.

## Estructura del repositorio

```
TIIT2007-sistemas-operativos/
├── README.md
├── proyecto1-scheduling/
│   ├── src/
│   ├── data/
│   ├── results/
│   ├── docs/
│   │   └── documento_ieee.pdf
│   └── README.md
├── proyecto2-concurrencia/
├── proyecto3-memoria/
├── proyecto4-administrador-recursos/
├── invenio-fest/
└── portafolio/
```

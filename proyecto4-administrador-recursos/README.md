# Proyecto 4 — Administrador Simplificado de Recursos

TIIT2007 – Sistemas Operativos · Universidad Invenio
**Isaac Felipe Morún Moreira**

Herramienta de consola en C++17 que integra gestión de archivos dentro de un
directorio de trabajo controlado con monitoreo de procesos y memoria del sistema
operativo anfitrión. Consolida las Unidades V y VI.

---

## Por dónde empezar a leer

Si abrís el repositorio por primera vez, este es el orden que tiene sentido:

| # | Archivo | Por qué empezar por ahí |
|---|---|---|
| 1 | `src/Sandbox.h` | Es el núcleo conceptual: lo que hace que «directorio de trabajo controlado» sea verdad y no una promesa. 170 líneas |
| 2 | `src/GestorArchivos.h` | El requisito funcional 1 completo. Todo pasa antes por `Sandbox::resolver` |
| 3 | `src/MonitorProcesos.h` y `src/MonitorMemoria.h` | Los **contratos** de los requisitos 2 y 3. Sin una sola línea de implementación |
| 4 | `src/SistemaInfo.cpp` | El **único** archivo del proyecto con `#ifdef`. Las seis implementaciones reales |
| 5 | `tests/test_recursos.cpp` | Cada prueba del sandbox es un ataque concreto, con su explicación al lado |
| 6 | `src/main.cpp` | Solo orquesta: menú, línea de comandos y despacho |

---

## Compilación

```bash
make
```

Banderas: `-std=c++17 -Wall -Wextra -O2`. Sin dependencias externas, solo la
biblioteca estándar. **Compila sin advertencias** en los dos entornos.

| Objetivo | Qué produce |
|---|---|
| `make` | la herramienta y las tres de medición |
| `make tests` | compila y corre las 54 pruebas |
| `make experimentos` | protocolo experimental completo, deja todo en `results/` |
| `make reporte` | regenera las tablas desde el CSV ya medido |
| `make clean` | borra los binarios |

### Linux

```bash
make && ./administrador
```

### Windows

El `Makefile` detecta la plataforma solo. Desde MSYS2/UCRT64:

```bash
make && ./administrador.exe
```

Sin `make`, a mano:

```bash
g++ -std=c++17 -Wall -Wextra -O2 -Isrc -o administrador.exe \
    src/main.cpp src/SistemaInfo.cpp -lpsapi
```

`-lpsapi` hace falta para `GetProcessMemoryInfo`, que mide el consumo propio de
la herramienta. En Linux no hay biblioteca extra que enlazar: todo sale de
`/proc`, que se lee con `ifstream` normal.

---

## Ejecución

```bash
./administrador [opciones]
```

| Opción | Por defecto | Descripción |
|---|---|---|
| `--base RUTA` | `data/workspace` | directorio de trabajo controlado |
| `--procesos N` | 15 | cuántos procesos listar |
| `--fuente NOMBRE` | — | forzar la fuente de procesos (`/proc`, `ps`, …) |
| `--sin-color` | — | desactiva los códigos ANSI |
| `--ascii` | — | sustituye los caracteres de dibujo UTF-8 |
| `--ayuda` | — | muestra la ayuda |

Con `--sin-color` y `--ascii` juntos, la salida no contiene un solo byte fuera
de ASCII. Ningún parámetro requiere recompilar.

**Códigos de salida:** `0` correcto · `1` error de argumentos o de entorno.

---

## Soporte de plataforma

El enunciado exige documentar esto explícitamente.

| | Linux | Windows |
|---|---|---|
| Gestión de archivos | `std::filesystem` (C++17) | idéntico, sin código propio |
| Procesos, ruta primaria | `/proc/[pid]/stat` | `CreateToolhelp32Snapshot` |
| Procesos, ruta de respaldo | `ps` | `tasklist` |
| Memoria, ruta primaria | `/proc/meminfo` | `GlobalMemoryStatusEx` |
| Memoria, ruta de respaldo | `free -b` | — (la API siempre está) |
| Consumo propio | `/proc/self/status` y `/proc/self/stat` | `GetProcessMemoryInfo`, `GetProcessTimes` |

**Todo el código dependiente de plataforma vive en `src/SistemaInfo.cpp`.**
Verificable de un vistazo:

```bash
for f in src/*.h src/*.cpp tests/*.cpp; do
  printf '%-26s %s\n' "$(basename $f)" "$(grep -c '_WIN32\|__linux__\|__APPLE__' $f)"
done
```

Los once archivos dan cero salvo `SistemaInfo.cpp`.

### Limitación conocida: los permisos en Windows

`std::filesystem::permissions()` da una vista unificada de dos modelos que **no
son equivalentes**. Linux usa el modelo Unix real de 9 bits, y lo que reporta la
herramienta coincide exactamente con `ls -l`. Windows usa ACL —listas donde cada
entrada da permisos a un usuario o grupo concreto—, y eso no cabe en 9 bits: la
biblioteca estándar lo aproxima a partir del atributo de solo lectura. **El
octal que se ve en Windows es una traducción con pérdida, no el permiso real.**
Se declara en vez de disimularse.

---

## El directorio de trabajo controlado

Es lo más importante del proyecto. Ninguna operación destructiva toca el disco
sin pasar por `Sandbox::resolver`, que:

1. Resuelve la ruta con **`weakly_canonical`** —y no `canonical`, que exige que
   la ruta entera exista y haría imposible validar la creación de un archivo
   nuevo—. Eso colapsa los `..` y resuelve los enlaces simbólicos del tramo que
   sí existe.
2. Comprueba la contención **comparando componentes de ruta**, no prefijos de
   texto. Con comparación de texto, un directorio hermano llamado
   `workspace_malo` empezaría por `workspace` y pasaría el filtro.

Se puede probar en vivo desde el menú: **1 → 5**.

| Se escribe | Resultado |
|---|---|
| `../../etc/passwd` | rechazada |
| `/etc/passwd` | rechazada |
| un enlace simbólico que apunta afuera | rechazada |
| `../workspace_malo/nomina.txt` | rechazada |
| `sub/../notas.txt` | aceptada (el `..` no sale) |

---

## Pruebas

```bash
make tests
```

54 pruebas. Devuelve 0 si todas pasan y 1 si alguna falla. Las que crean enlaces
simbólicos se informan como **omitidas** —no como pasadas— en plataformas donde
crear enlaces exige privilegios.

**Se comprobó que la suite puede fallar.** Se rompió el código a propósito tres
veces y se anotó cuántas veces la suite se dio cuenta: dos de tres. La tercera
—cambiar `rfind` por `find` al analizar `/proc/[pid]/stat`— no la detectaba
nadie, porque el análisis vivía dentro de la función que abre el archivo y solo
podía ejercitarse contra procesos reales, que casi nunca tienen paréntesis en el
nombre. Se corrigió separándolo a `src/AnalisisProcFS.h`, una función pura, y
agregando las pruebas 48–54 con líneas sintéticas.

---

## Protocolo experimental

```bash
make experimentos          # o: ./scripts/experimentos.sh [repeticiones]
```

Corre las dos condiciones que pide el enunciado y deja la evidencia en
`results/`:

| Archivo | Qué contiene |
|---|---|
| `results/benchmark.csv` | todas las mediciones crudas, una fila por corrida |
| `results/comparativa_carga.txt` | estado del sistema antes, durante y después de la carga |
| `results/reporte.txt` | las tablas del documento IEEE |

### Metodología, y por qué cada decisión

- **Tamaños alternados, no agrupados.** Se corre 10, 100, 1000 y recién entonces
  la repetición siguiente. Agrupando, cualquier cosa que pase en la máquina
  durante ese rato caería entera sobre un solo tamaño y se leería como si ese
  tamaño fuera lento.
- **Se reporta la mediana, no el promedio.** Una corrida contaminada por otro
  proceso arrastra el promedio pero no mueve la mediana. En el Proyecto 2 una
  medición única dio una diferencia de factor 10 por ruido de la VM. El rango
  mínimo–máximo se informa igual: esconder la dispersión sería presentar los
  datos como más limpios de lo que son.
- **La primera repetición se marca, no se descarta.** Sale en el CSV con
  `repeticion = 0` y `cache_fria = 1`. Mide la caché fría del sistema de
  archivos, que es información legítima.
- **El listado se mide en dos niveles.** `recorrer` solo enumera el directorio;
  `listar` enumera **y** lee los metadatos de cada entrada, que es lo que hace
  la herramienta de verdad. La diferencia dice cuánto cuesta el `stat` por
  archivo frente al costo fijo de abrir el directorio.
- **Se mide el código que se entrega.** El benchmark llama a `gestor::crear`,
  `gestor::listar` y `gestor::eliminar` —las mismas funciones que usa el menú—,
  incluido el costo de validar cada ruta contra el directorio controlado.

### La carga

`herramientas/carga` reserva memoria, **la toca** (reservar no basta: Linux
entrega páginas de forma perezosa, y sin escribir en ellas la carga sería
ficticia) y quema CPU con varios hilos. El script lanza varias instancias en
segundo plano, que es lo que además sube el número de procesos del sistema.

No se usa `stress` ni `stress-ng`: no están instalados en la VM del curso y
agregarlos obligaría a un `sudo apt install` antes de poder reproducir los
experimentos.

---

## Estructura

```
proyecto4-administrador-recursos/
├── src/
│   ├── Sandbox.h           directorio de trabajo controlado
│   ├── GestorArchivos.h    crear, listar, eliminar, metadatos
│   ├── Permisos.h          octal, rwx y auditoría de riesgo
│   ├── MonitorProcesos.h   contrato del requisito 2
│   ├── MonitorMemoria.h    contrato del requisito 3
│   ├── AnalisisProcFS.h    análisis puro de /proc/[pid]/stat
│   ├── SistemaInfo.cpp     ← el único archivo con #ifdef
│   ├── Consola.h           colores, recuadros, tablas, barras
│   ├── VistasSistema.h     presentación del monitoreo
│   └── main.cpp            menú y despacho
├── herramientas/
│   ├── benchmark.cpp       mide las operaciones de archivo
│   ├── carga.cpp           genera carga de memoria y CPU
│   ├── reporte.cpp         tablas y gráficos desde los CSV
│   └── ansi_a_html.py      convierte la salida real a HTML para la bitácora
├── tests/test_recursos.cpp 54 pruebas
├── scripts/experimentos.sh protocolo completo
├── data/workspace/         directorio de trabajo (no se versiona su contenido)
├── results/                mediciones y evidencia
├── docs/                   documento IEEE
├── Makefile
└── README.md
```

`results/` es el nombre que usa el documento oficial de estructura del curso
(`17_notebooklm_y_repositorio.md`); los Proyectos 2 y 3 habían usado
`evidencia/`.

---

## Entorno de prueba

| | Entorno 1 | Entorno 2 |
|---|---|---|
| Sistema | Ubuntu 26.04 LTS (VirtualBox) | Windows 11 Pro |
| Compilador | g++ 15.2.0 | g++ 16.2.0 (MinGW-w64 UCRT) |
| Núcleos lógicos | 6 | 16 |
| Banderas | `-std=c++17 -Wall -Wextra -O2` | idénticas |

La VM **no tiene área de intercambio configurada** (`Swap: 0B`), así que los
experimentos de carga no pueden mostrar actividad de swap ni thrashing. Se
declara porque el Laboratorio 4 sí observa esas columnas: aquí la presión de
memoria se manifiesta en el tiempo de las operaciones, no en `si`/`so`.

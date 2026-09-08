# ---------------------------------------------------------------------------
# Makefile - Proyecto 4, Administrador Simplificado de Recursos
# TIIT2007 Sistemas Operativos - Universidad Invenio
# Isaac Felipe Morun Moreira
#
#   make               compila la herramienta y las herramientas de medicion
#   make tests         compila y corre la suite de pruebas
#   make experimentos  corre el protocolo completo y deja la evidencia
#   make reporte       regenera las tablas desde el CSV ya medido
#   make clean         borra los binarios
#
# Funciona igual en Linux (g++ 15.2.0) y en Windows bajo MSYS2/UCRT64
# (g++ 16.2.0). La unica diferencia real esta en las dos variables de la
# deteccion de plataforma de mas abajo.
# ---------------------------------------------------------------------------
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isrc

# --- Deteccion de plataforma ------------------------------------------------
# En Windows hay que enlazar psapi para GetProcessMemoryInfo (consumo propio de
# la herramienta) y el ejecutable lleva extension .exe. En Linux no hace falta
# ninguna biblioteca extra: todo sale de /proc, que se lee con ifstream normal.
# El generador de carga usa hilos, y en Linux eso pide -pthread.
ifeq ($(OS),Windows_NT)
  BIN   = administrador.exe
  LIBS  = -lpsapi
  HILOS =
  # -static enlaza libstdc++, libgcc y libwinpthread dentro del ejecutable.
  # Sin esto el .exe solo arranca si el directorio de las DLL de MSYS2 esta en
  # el PATH; en cualquier otra maquina falla con 0xC0000139, "punto de entrada
  # no encontrado", que no le dice nada util a quien lo recibe. Enlazado asi,
  # el binario corre en cualquier Windows sin instalar nada.
  EXTRA = -static
  # mingw32-make ejecuta las recetas con cmd.exe, que no tiene 'rm'. Y 'del' no
  # acepta barras normales: las interpreta como modificadores. Por eso hay que
  # traducirlas con $(subst) antes de pasarle la lista.
  BORRAR = del /F /Q
  ABARRA = $(subst /,\,$(1))
else
  BIN   = administrador
  LIBS  =
  HILOS = -pthread
  EXTRA =
  BORRAR = rm -f
  ABARRA = $(1)
endif

# --- Fuentes ----------------------------------------------------------------
# HDR incluye TODOS los headers y se declara como dependencia de cada objeto.
# Sin esto, make no recompila al modificar un .h y se terminan midiendo
# binarios viejos: paso en el Proyecto 2 y un analisis con ThreadSanitizer
# llego a reportar cero condiciones de carrera por esa razon.
HDR = $(wildcard src/*.h)
SRC = src/main.cpp src/SistemaInfo.cpp
OBJ = $(SRC:.cpp=.o)

PRUEBAS   = tests/test_recursos
BENCHMARK = herramientas/benchmark
CARGA     = herramientas/carga
REPORTE   = herramientas/reporte

all: $(BIN) $(BENCHMARK) $(CARGA) $(REPORTE)

# Lleva -pthread porque la demostracion de E/S de la Unidad VI usa
# std::thread para simular el dispositivo que avisa cuando termina.
$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LIBS) $(HILOS) $(EXTRA)

%.o: %.cpp $(HDR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# El benchmark enlaza contra SistemaInfo.o y llama a las mismas funciones de
# GestorArchivos que usa el menu: mide el codigo que se entrega, no una version
# simplificada escrita para el experimento.
# Lleva -pthread porque genera su propia carga de CPU con std::thread.
$(BENCHMARK): herramientas/benchmark.cpp src/SistemaInfo.o $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ herramientas/benchmark.cpp src/SistemaInfo.o $(LIBS) $(HILOS) $(EXTRA)

# La carga usa std::thread, de ahi -pthread en Linux.
$(CARGA): herramientas/carga.cpp
	$(CXX) $(CXXFLAGS) -o $@ herramientas/carga.cpp $(HILOS) $(EXTRA)

# El reporte solo lee CSV: no depende de nada del proyecto.
$(REPORTE): herramientas/reporte.cpp
	$(CXX) $(CXXFLAGS) -o $@ herramientas/reporte.cpp $(EXTRA)

# La suite enlaza contra SistemaInfo.o, el mismo objeto que usa la herramienta:
# se prueba el codigo que se entrega, no una copia compilada aparte.
$(PRUEBAS): tests/test_recursos.cpp src/SistemaInfo.o $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ tests/test_recursos.cpp src/SistemaInfo.o $(LIBS) $(HILOS) $(EXTRA)

tests: $(PRUEBAS)
	./$(PRUEBAS)

experimentos: all
	./scripts/experimentos.sh

reporte: $(REPORTE)
	./$(REPORTE) results/benchmark.csv

# Lista unica de lo que se borra, para no repetirla en cada plataforma.
BASURA = $(OBJ) administrador administrador.exe \
         $(PRUEBAS) $(PRUEBAS).exe \
         $(BENCHMARK) $(BENCHMARK).exe \
         $(CARGA) $(CARGA).exe \
         $(REPORTE) $(REPORTE).exe

# El guion inicial le dice a make que ignore el codigo de salida: los archivos
# que no existen no son un error de limpieza, y 'del' se queja de cada uno.
clean:
	-$(BORRAR) $(call ABARRA,$(BASURA))

.PHONY: all tests experimentos reporte clean

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
else
  BIN   = administrador
  LIBS  =
  HILOS = -pthread
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

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LIBS)

%.o: %.cpp $(HDR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# El benchmark enlaza contra SistemaInfo.o y llama a las mismas funciones de
# GestorArchivos que usa el menu: mide el codigo que se entrega, no una version
# simplificada escrita para el experimento.
# Lleva -pthread porque genera su propia carga de CPU con std::thread.
$(BENCHMARK): herramientas/benchmark.cpp src/SistemaInfo.o $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ herramientas/benchmark.cpp src/SistemaInfo.o $(LIBS) $(HILOS)

# La carga usa std::thread, de ahi -pthread en Linux.
$(CARGA): herramientas/carga.cpp
	$(CXX) $(CXXFLAGS) -o $@ herramientas/carga.cpp $(HILOS)

# El reporte solo lee CSV: no depende de nada del proyecto.
$(REPORTE): herramientas/reporte.cpp
	$(CXX) $(CXXFLAGS) -o $@ herramientas/reporte.cpp

# La suite enlaza contra SistemaInfo.o, el mismo objeto que usa la herramienta:
# se prueba el codigo que se entrega, no una copia compilada aparte.
$(PRUEBAS): tests/test_recursos.cpp src/SistemaInfo.o $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ tests/test_recursos.cpp src/SistemaInfo.o $(LIBS)

tests: $(PRUEBAS)
	./$(PRUEBAS)

experimentos: all
	./scripts/experimentos.sh

reporte: $(REPORTE)
	./$(REPORTE) results/benchmark.csv

clean:
	rm -f $(OBJ) administrador administrador.exe \
	      $(PRUEBAS) $(PRUEBAS).exe \
	      $(BENCHMARK) $(BENCHMARK).exe \
	      $(CARGA) $(CARGA).exe \
	      $(REPORTE) $(REPORTE).exe

.PHONY: all tests experimentos reporte clean

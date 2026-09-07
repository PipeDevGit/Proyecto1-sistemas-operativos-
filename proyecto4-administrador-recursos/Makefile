# ---------------------------------------------------------------------------
# Makefile - Proyecto 4, Administrador Simplificado de Recursos
# TIIT2007 Sistemas Operativos - Universidad Invenio
# Isaac Felipe Morun Moreira
#
#   make            compila la herramienta
#   make clean      borra los binarios
#
# Funciona igual en Linux (g++ 15.2.0) y en Windows bajo MSYS2/UCRT64
# (g++ 16.2.0). La unica diferencia real esta en las dos variables de la
# deteccion de plataforma de mas abajo.
# ---------------------------------------------------------------------------
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isrc

# --- Deteccion de plataforma ------------------------------------------------
# En Windows hay que enlazar psapi para GetProcessMemoryInfo (consumo propio de
# la herramienta, fase 3) y el ejecutable lleva extension .exe. En Linux no
# hace falta ninguna biblioteca extra: todo sale de /proc, que se lee con
# ifstream normal.
ifeq ($(OS),Windows_NT)
  BIN  = administrador.exe
  LIBS = -lpsapi
else
  BIN  = administrador
  LIBS =
endif

# --- Fuentes ----------------------------------------------------------------
# HDR incluye TODOS los headers y se declara como dependencia de cada objeto.
# Sin esto, make no recompila al modificar un .h y se terminan midiendo
# binarios viejos: paso en el Proyecto 2 y un analisis con ThreadSanitizer
# llego a reportar cero condiciones de carrera por esa razon.
HDR = $(wildcard src/*.h)
SRC = src/main.cpp src/SistemaInfo.cpp
OBJ = $(SRC:.cpp=.o)

all: $(BIN)

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ) $(LIBS)

%.o: %.cpp $(HDR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) administrador administrador.exe

.PHONY: all clean

# ---------------------------------------------------------------------------
# Makefile - Proyecto 3, Simulador de Gestion de Memoria
# TIIT2007 Sistemas Operativos - Universidad Invenio
#
#   make               compila el simulador y las herramientas
#   make traza         traza paso a paso de la cadena canonica
#   make belady        demuestra la anomalia de Belady
#   make cadenas       regenera las cadenas largas sinteticas
#   make experimentos  corre el protocolo completo y muestra el reporte
#   make validar       contrasta contra los valores publicados en la literatura
#   make clean
# ---------------------------------------------------------------------------
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Icommon -Isrc

SIM      = simulador
GENERAR  = herramientas/generar
REPORTE  = herramientas/reporte
HDR      = $(wildcard common/*.h) $(wildcard src/*.h)

all: $(SIM) $(GENERAR) $(REPORTE)

$(SIM): src/main.cpp $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ src/main.cpp

$(GENERAR): herramientas/generar.cpp
	$(CXX) -std=c++17 -Wall -Wextra -O2 -o $@ herramientas/generar.cpp

$(REPORTE): herramientas/reporte.cpp
	$(CXX) -std=c++17 -Wall -Wextra -O2 -o $@ herramientas/reporte.cpp

traza: $(SIM)
	./$(SIM) --cadena data/corta.txt --marcos 3 --traza

belady: $(SIM)
	@echo "=== Anomalia de Belady: FIFO con 3 y con 4 marcos ==="
	./$(SIM) --cadena data/belady.txt --marcos 3 --algoritmo fifo
	./$(SIM) --cadena data/belady.txt --marcos 4 --algoritmo fifo

cadenas: $(GENERAR)
	./$(GENERAR) localidad  1200 20 42 > data/larga_localidad.txt
	./$(GENERAR) secuencial 1200 20 42 > data/larga_secuencial.txt
	./$(GENERAR) aleatoria  1200 20 42 > data/larga_aleatoria.txt
	@echo "Cadenas regeneradas en data/"

experimentos: $(SIM) $(REPORTE)
	./scripts/experimentos.sh
	./$(REPORTE) evidencia/resultados.csv

# Contrasta los tres algoritmos contra los valores publicados para la cadena
# canonica con 3 marcos: FIFO 15, LRU 12, OPTIMO 9.
validar: $(SIM)
	@./scripts/validar.sh

clean:
	rm -f $(SIM) $(GENERAR) $(REPORTE)

.PHONY: all traza belady cadenas experimentos validar clean

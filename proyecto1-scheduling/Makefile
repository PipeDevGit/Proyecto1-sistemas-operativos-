CXX = g++
CXXFLAGS = -std=c++17 -Wall -Isrc

SRC = src/main.cpp src/Lector.cpp src/FCFS.cpp src/RoundRobin.cpp src/Metricas.cpp src/Gantt.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = simulador

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJ)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Compila (si hace falta) y corre una sola combinacion por defecto: dataset de 10 procesos, quantum=2.
run: $(TARGET)
	./$(TARGET) data/procesos.txt 2

# Compila (si hace falta) y corre las 4 combinaciones obligatorias/usadas en el documento IEEE,
# una detras de otra, sin tener que escribir cada comando a mano.
demo: $(TARGET)
	@echo "=== procesos.txt (10 procesos) - quantum=2 ==="
	@./$(TARGET) data/procesos.txt 2
	@echo
	@echo "=== procesos.txt (10 procesos) - quantum=4 ==="
	@./$(TARGET) data/procesos.txt 4
	@echo
	@echo "=== procesos50.txt (50 procesos) - quantum=2 ==="
	@./$(TARGET) data/procesos50.txt 2
	@echo
	@echo "=== procesos50.txt (50 procesos) - quantum=4 ==="
	@./$(TARGET) data/procesos50.txt 4

clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: all run demo clean

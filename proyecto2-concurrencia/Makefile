CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2 -Isrc

SRC    = src/main.cpp src/Registro.cpp
# Los headers van como dependencia explicita: sin esto, cambiar BufferA.h no
# dispara la recompilacion y se terminan midiendo binarios viejos.
HDR    = $(wildcard src/*.h)
TARGET = simulador

# Binario aparte con ThreadSanitizer. Es SOLO para evidencia de carreras:
# TSan hace el programa 5-15 veces mas lento, asi que los tiempos del
# documento NUNCA salen de este binario.
TSAN_FLAGS  = -std=c++17 -Wall -Wextra -pthread -g -fsanitize=thread -Isrc
TSAN_TARGET = simulador_tsan

DEADLOCK = demo_deadlock
REPORTE  = reporte

all: $(TARGET) $(DEADLOCK) $(REPORTE)

$(TARGET): $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

# scoped_lock necesita C++17, que ya es el estandar del proyecto.
$(DEADLOCK): src/deadlock.cpp
	$(CXX) $(CXXFLAGS) -o $@ src/deadlock.cpp

$(REPORTE): src/reporte.cpp
	$(CXX) $(CXXFLAGS) -o $@ src/reporte.cpp

tsan: $(SRC) $(HDR)
	$(CXX) $(TSAN_FLAGS) -o $(TSAN_TARGET) $(SRC)

# Corrida rapida de la version A
run: $(TARGET)
	./$(TARGET) --variante A

# Evidencia de condicion de carrera con ThreadSanitizer
race: tsan
	./$(TSAN_TARGET) --variante A --items 2000

# Demostracion de interbloqueo: primero se traba, despues las dos prevenciones
deadlock: $(DEADLOCK)
	-./$(DEADLOCK) --modo deadlock
	./$(DEADLOCK) --modo orden
	./$(DEADLOCK) --modo scoped

# Regenera todos los resultados desde cero y muestra el reporte.
experimentos: $(TARGET) $(REPORTE)
	@rm -f results/experimentos.csv
	@for i in 1 2 3 4 5; do \
		for v in A B C D; do \
			./$(TARGET) --variante $$v --sin-color --csv results/experimentos.csv > /dev/null; \
		done; \
	done
	./$(REPORTE) results/experimentos.csv

clean:
	rm -f $(TARGET) $(TSAN_TARGET) $(DEADLOCK) $(REPORTE)

.PHONY: all tsan run race deadlock clean

# Makefile de los EXTRAS. Se compila por separado y no es necesario para
# evaluar los entregables exigidos (version A y version B).
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2 -I../common -I.

all: extras demo_deadlock

extras: main.cpp BufferEsperaActiva.h BufferSemaforos.h ../common/Ejecutor.h ../common/Item.h ../common/Registro.h
	$(CXX) $(CXXFLAGS) -o extras main.cpp ../common/Registro.cpp

# ATENCION: demo_deadlock SE BLOQUEA A PROPOSITO en su modo por defecto.
# Es una demostracion independiente del temario de interbloqueos; NO es la
# version B ni tiene relacion con ella.
demo_deadlock: deadlock.cpp
	$(CXX) $(CXXFLAGS) -o demo_deadlock deadlock.cpp

clean:
	rm -f extras demo_deadlock

.PHONY: all clean

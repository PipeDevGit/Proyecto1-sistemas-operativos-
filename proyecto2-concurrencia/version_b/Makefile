# Makefile de la VERSION B. Compila por separado, sin depender de la otra
# version (entregable 1 del enunciado).
CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2 -I../common -I.

TARGET   = version_b
SRC      = main.cpp ../common/Registro.cpp
HDR      = BufferB.h ../common/Item.h ../common/Registro.h ../common/Ejecutor.h

all: $(TARGET)

# Los encabezados van como dependencia explicita: sin esto, cambiar BufferB.h
# no dispara la recompilacion y se terminan midiendo binarios viejos.
$(TARGET): $(SRC) $(HDR)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

# Binario aparte instrumentado con ThreadSanitizer. Es SOLO para evidencia de
# carreras: hace el programa entre 5 y 15 veces mas lento, asi que ningun
# tiempo del informe sale de aqui.
tsan: $(SRC) $(HDR)
	$(CXX) -std=c++17 -Wall -Wextra -pthread -g -fsanitize=thread -I../common -I. 	       -o $(TARGET)_tsan $(SRC)

clean:
	rm -f $(TARGET) $(TARGET)_tsan

.PHONY: all tsan clean

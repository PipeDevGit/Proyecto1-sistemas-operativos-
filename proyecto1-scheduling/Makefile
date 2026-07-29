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

clean:
	rm -f $(OBJ) $(TARGET)

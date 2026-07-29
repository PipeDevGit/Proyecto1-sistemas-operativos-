#ifndef SIMULACION_H
#define SIMULACION_H

#include <vector>
#include "Proceso.h"

// Representa un tramo de tiempo de CPU: que proceso corrio (o -1 si CPU ociosa) entre inicio y fin.
struct Segmento {
    int idProceso; // -1 = CPU ociosa
    int inicio;
    int fin;
};

// Resultado comun de cualquier algoritmo de planificacion: los procesos con sus tiempos
// calculados, y la traza de ejecucion (segmentos) usada para el Gantt y la utilizacion de CPU.
struct ResultadoSimulacion {
    std::vector<Proceso> procesos;
    std::vector<Segmento> segmentos;
};

#endif

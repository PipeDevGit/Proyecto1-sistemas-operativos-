#ifndef GANTT_H
#define GANTT_H

#include <string>
#include "Simulacion.h"

// Genera una representacion ASCII del diagrama de Gantt a partir de los segmentos de ejecucion.
std::string generarGanttASCII(const ResultadoSimulacion& resultado);

#endif

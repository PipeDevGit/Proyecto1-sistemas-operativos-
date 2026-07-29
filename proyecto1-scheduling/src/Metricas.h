#ifndef METRICAS_H
#define METRICAS_H

#include <string>
#include "Simulacion.h"

// Imprime la tabla de tiempos de espera/retorno por proceso, sus promedios,
// y el % de utilizacion de CPU para un resultado de simulacion ya calculado.
void imprimirMetricas(const ResultadoSimulacion& resultado, const std::string& etiquetaAlgoritmo);

// Calcula el % de utilizacion de CPU (tiempo no ocioso / tiempo total) a partir de los segmentos.
double calcularUtilizacionCPU(const ResultadoSimulacion& resultado);

#endif

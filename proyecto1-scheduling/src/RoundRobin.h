#ifndef ROUNDROBIN_H
#define ROUNDROBIN_H

#include <vector>
#include "Proceso.h"
#include "Simulacion.h"

// Simula Round Robin con quantum fijo configurable. Cada proceso listo recibe como
// maximo 'quantum' unidades de CPU por turno; si no termina, vuelve al final de la cola.
ResultadoSimulacion simularRoundRobin(std::vector<Proceso> procesos, int quantum);

#endif

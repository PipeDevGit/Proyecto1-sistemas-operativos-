#ifndef FCFS_H
#define FCFS_H

#include <vector>
#include "Proceso.h"
#include "Simulacion.h"

// Simula FCFS (First Come, First Served): ejecuta los procesos en orden de llegada,
// sin apropiacion (una vez que un proceso empieza, corre hasta terminar su rafaga completa).
ResultadoSimulacion simularFCFS(std::vector<Proceso> procesos);

#endif

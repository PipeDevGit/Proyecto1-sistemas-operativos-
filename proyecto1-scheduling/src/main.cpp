#include <iostream>
#include "Proceso.h"
#include "Lector.h"
#include "FCFS.h"
#include "RoundRobin.h"
#include "Metricas.h"
#include "Gantt.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Uso: " << argv[0] << " <archivo_procesos> <quantum_round_robin>\n";
        return 1;
    }

    std::vector<Proceso> procesos;
    try {
        procesos = leerProcesos(argv[1]);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    if (procesos.empty()) {
        std::cerr << "No se pudo cargar ningun proceso valido desde " << argv[1] << "\n";
        return 1;
    }

    int quantum = std::stoi(argv[2]);

    ResultadoSimulacion resultadoFCFS = simularFCFS(procesos);
    ResultadoSimulacion resultadoRR = simularRoundRobin(procesos, quantum);

    std::cout << "=== FCFS ===\n";
    std::cout << generarGanttASCII(resultadoFCFS) << "\n";
    imprimirMetricas(resultadoFCFS, "FCFS");

    std::cout << "\n=== Round Robin (quantum=" << quantum << ") ===\n";
    std::cout << generarGanttASCII(resultadoRR) << "\n";
    imprimirMetricas(resultadoRR, "Round Robin");

    return 0;
}

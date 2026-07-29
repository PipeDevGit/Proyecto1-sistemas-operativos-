#include "Metricas.h"
#include <iostream>
#include <iomanip>

double calcularUtilizacionCPU(const ResultadoSimulacion& resultado) {
    int tiempoTotal = 0;
    int tiempoOcioso = 0;

    for (const auto& seg : resultado.segmentos) {
        int duracion = seg.fin - seg.inicio;
        tiempoTotal += duracion;
        if (seg.idProceso == -1) {
            tiempoOcioso += duracion;
        }
    }

    if (tiempoTotal == 0) return 0.0;
    return 100.0 * (tiempoTotal - tiempoOcioso) / tiempoTotal;
}

void imprimirMetricas(const ResultadoSimulacion& resultado, const std::string& etiquetaAlgoritmo) {
    std::cout << "\n--- " << etiquetaAlgoritmo << ": tiempos por proceso ---\n";
    std::cout << std::left << std::setw(6) << "PID" << std::setw(12) << "Espera" << "Retorno\n";

    double sumaEspera = 0.0;
    double sumaRetorno = 0.0;

    for (const auto& p : resultado.procesos) {
        std::cout << std::left << std::setw(6) << ("P" + std::to_string(p.id))
                  << std::setw(12) << p.tiempoEspera << p.tiempoRetorno << "\n";
        sumaEspera += p.tiempoEspera;
        sumaRetorno += p.tiempoRetorno;
    }

    size_t n = resultado.procesos.size();
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Promedio espera:  " << (sumaEspera / static_cast<double>(n)) << "\n";
    std::cout << "Promedio retorno: " << (sumaRetorno / static_cast<double>(n)) << "\n";
    std::cout << "Utilizacion CPU:  " << calcularUtilizacionCPU(resultado) << "%\n";
}

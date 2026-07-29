#include "FCFS.h"
#include <algorithm>

ResultadoSimulacion simularFCFS(std::vector<Proceso> procesos) {
    // FCFS ordena estrictamente por tiempo de llegada; en empate, por id (orden determinista)
    std::sort(procesos.begin(), procesos.end(), [](const Proceso& a, const Proceso& b) {
        if (a.llegada != b.llegada) return a.llegada < b.llegada;
        return a.id < b.id;
    });

    ResultadoSimulacion resultado;
    int tiempoActual = 0;

    for (auto& proceso : procesos) {
        if (tiempoActual < proceso.llegada) {
            // CPU ociosa hasta que llegue el siguiente proceso
            resultado.segmentos.push_back({-1, tiempoActual, proceso.llegada});
            tiempoActual = proceso.llegada;
        }

        int inicio = tiempoActual;
        proceso.finalizacion = inicio + proceso.rafaga;
        proceso.tiempoRetorno = proceso.finalizacion - proceso.llegada;
        proceso.tiempoEspera = proceso.tiempoRetorno - proceso.rafaga;

        resultado.segmentos.push_back({proceso.id, inicio, proceso.finalizacion});
        tiempoActual = proceso.finalizacion;

        resultado.procesos.push_back(proceso);
    }

    return resultado;
}

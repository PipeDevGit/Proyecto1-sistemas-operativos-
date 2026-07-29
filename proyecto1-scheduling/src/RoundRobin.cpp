#include "RoundRobin.h"
#include <algorithm>
#include <queue>

ResultadoSimulacion simularRoundRobin(std::vector<Proceso> procesos, int quantum) {
    std::sort(procesos.begin(), procesos.end(), [](const Proceso& a, const Proceso& b) {
        if (a.llegada != b.llegada) return a.llegada < b.llegada;
        return a.id < b.id;
    });

    ResultadoSimulacion resultado;
    std::queue<int> colaListos;
    size_t siguienteLlegada = 0;
    int tiempoActual = 0;
    int pendientes = static_cast<int>(procesos.size());

    auto encolarLlegadasHasta = [&](int t) {
        while (siguienteLlegada < procesos.size() && procesos[siguienteLlegada].llegada <= t) {
            colaListos.push(static_cast<int>(siguienteLlegada));
            siguienteLlegada++;
        }
    };

    encolarLlegadasHasta(tiempoActual);

    while (pendientes > 0) {
        if (colaListos.empty()) {
            // Nadie listo todavia: CPU ociosa hasta la proxima llegada
            resultado.segmentos.push_back({-1, tiempoActual, procesos[siguienteLlegada].llegada});
            tiempoActual = procesos[siguienteLlegada].llegada;
            encolarLlegadasHasta(tiempoActual);
        }

        int idx = colaListos.front();
        colaListos.pop();
        Proceso& proceso = procesos[idx];

        int ejecutar = std::min(quantum, proceso.restante);
        int inicio = tiempoActual;
        tiempoActual += ejecutar;
        proceso.restante -= ejecutar;

        resultado.segmentos.push_back({proceso.id, inicio, tiempoActual});

        // Los que llegaron durante este quantum se encolan ANTES de reencolar al proceso
        // actual (si no termino), para respetar el orden real de llegada.
        encolarLlegadasHasta(tiempoActual);

        if (proceso.restante > 0) {
            colaListos.push(idx);
        } else {
            proceso.finalizacion = tiempoActual;
            proceso.tiempoRetorno = proceso.finalizacion - proceso.llegada;
            proceso.tiempoEspera = proceso.tiempoRetorno - proceso.rafaga;
            pendientes--;
        }
    }

    resultado.procesos = procesos;
    std::sort(resultado.procesos.begin(), resultado.procesos.end(), [](const Proceso& a, const Proceso& b) {
        return a.id < b.id;
    });

    return resultado;
}

// ---------------------------------------------------------------------------
// Simulador Productor-Consumidor - VERSION A
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// Toda la orquestacion, la instrumentacion y el formato de salida viven en
// ../common y son IDENTICOS para las dos versiones: la unica diferencia entre
// ellas es el buffer que se instancia aqui abajo.
// ---------------------------------------------------------------------------
#include "Ejecutor.h"
#include "BufferA.h"

int main(int argc, char** argv) {
    Config cfg;
    if (!parsearArgumentos(argc, argv, cfg, "A")) return 1;

    imprimirCabecera(cfg, "A", "sin sincronizacion");

    const bool conLog = !cfg.log.empty();
    Metricas m = ejecutar<BufferA>(cfg, conLog);

    imprimirResultado(cfg, m);
    anexarCsv(cfg, m, "A");

    if (conLog) printf("  Log de eventos escrito en %s\n\n", cfg.log.c_str());

    // Codigo de salida: 0 si el invariante se cumplio, 2 si se violo. Permite
    // encadenar corridas en scripts sin analizar la salida de texto.
    return m.invarianteOk() ? 0 : 2;
}

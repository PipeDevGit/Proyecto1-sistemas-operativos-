// ---------------------------------------------------------------------------
// EXTRAS - mecanismos adicionales de sincronizacion
//
// NO forma parte de los entregables exigidos por el enunciado, que pide
// unicamente la version A y la version B. Se incluye para sostener con datos
// propios dos puntos del documento:
//
//   espera-activa  respalda lo que RNF-5 admite: usar espera activa siempre
//                  que se documente como decision de diseno CON SU COSTO
//                  MEDIDO. Comparada contra la version B aisla el efecto de
//                  girar en vez de dormirse.
//
//   semaforos      permite contrastar experimentalmente mutex contra semaforo.
//
// Ambos mecanismos son CORRECTOS: no exhiben condiciones de carrera.
// ---------------------------------------------------------------------------
#include <cstring>
#include "Ejecutor.h"
#include "BufferEsperaActiva.h"
#include "BufferSemaforos.h"

int main(int argc, char** argv) {
    std::string mecanismo = "espera-activa";

    // Se extrae --mecanismo y se deja el resto de argumentos para el parser
    // comun, de modo que las opciones sean identicas a las de A y B.
    std::vector<char*> resto;
    resto.push_back(argv[0]);
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--mecanismo") == 0 && i + 1 < argc) {
            mecanismo = argv[++i];
        } else {
            resto.push_back(argv[i]);
        }
    }
    if (mecanismo != "espera-activa" && mecanismo != "semaforos") {
        std::cerr << "Error: --mecanismo debe ser espera-activa o semaforos.\n";
        return 1;
    }

    Config cfg;
    if (!parsearArgumentos(static_cast<int>(resto.size()), resto.data(), cfg,
                           mecanismo.c_str())) return 1;

    const bool esEsperaActiva = (mecanismo == "espera-activa");
    imprimirCabecera(cfg, esEsperaActiva ? "EXTRA/espera-activa" : "EXTRA/semaforos",
                     esEsperaActiva ? "mutex + espera activa" : "tres semaforos");

    const bool conLog = !cfg.log.empty();
    Metricas m = esEsperaActiva ? ejecutar<BufferEsperaActiva>(cfg, conLog)
                                : ejecutar<BufferSemaforos>(cfg, conLog);

    imprimirResultado(cfg, m);
    anexarCsv(cfg, m, esEsperaActiva ? "EA" : "SEM");
    return m.invarianteOk() ? 0 : 2;
}

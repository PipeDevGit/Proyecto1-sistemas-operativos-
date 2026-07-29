#include "Gantt.h"
#include <sstream>

std::string generarGanttASCII(const ResultadoSimulacion& resultado) {
    std::ostringstream barra;
    std::ostringstream regla;

    barra << "|";
    regla << "0";

    for (const auto& seg : resultado.segmentos) {
        std::string etiqueta = (seg.idProceso == -1) ? "ocioso" : ("P" + std::to_string(seg.idProceso));
        barra << " " << etiqueta << " |";

        int ancho = static_cast<int>(etiqueta.size()) + 3;
        std::string marca = std::to_string(seg.fin);
        int relleno = ancho - static_cast<int>(marca.size());
        if (relleno < 1) relleno = 1;
        regla << std::string(static_cast<size_t>(relleno), ' ');
        regla << marca;
    }

    return barra.str() + "\n" + regla.str();
}

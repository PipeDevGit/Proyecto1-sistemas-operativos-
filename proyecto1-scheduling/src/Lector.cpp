#include "Lector.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>

std::vector<Proceso> leerProcesos(const std::string& rutaArchivo) {
    std::ifstream archivo(rutaArchivo);
    if (!archivo.is_open()) {
        throw std::runtime_error("No se pudo abrir el archivo: " + rutaArchivo);
    }

    std::vector<Proceso> procesos;
    std::string linea;
    int numeroLinea = 0;

    while (std::getline(archivo, linea)) {
        numeroLinea++;

        if (linea.empty() || linea[0] == '#') {
            continue;
        }

        std::stringstream ss(linea);
        std::string campoId, campoLlegada, campoRafaga;

        if (!std::getline(ss, campoId, ',') ||
            !std::getline(ss, campoLlegada, ',') ||
            !std::getline(ss, campoRafaga, ',')) {
            std::cerr << "Aviso: linea " << numeroLinea
                      << " malformada (se esperaban 3 campos), se ignora: \"" << linea << "\"\n";
            continue;
        }

        try {
            int id = std::stoi(campoId);
            int llegada = std::stoi(campoLlegada);
            int rafaga = std::stoi(campoRafaga);

            if (llegada < 0 || rafaga <= 0) {
                std::cerr << "Aviso: linea " << numeroLinea
                          << " con valores invalidos (llegada >= 0, rafaga > 0), se ignora: \"" << linea << "\"\n";
                continue;
            }

            procesos.emplace_back(id, llegada, rafaga);
        } catch (const std::exception&) {
            std::cerr << "Aviso: linea " << numeroLinea
                      << " no numerica, se ignora: \"" << linea << "\"\n";
            continue;
        }
    }

    return procesos;
}

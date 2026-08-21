#ifndef CADENA_H
#define CADENA_H

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// Reloj monotono para medir duraciones: no salta si cambia la hora del sistema.
typedef std::chrono::steady_clock Reloj;

// ---------------------------------------------------------------------------
// Cadena de referencias de paginas (RF-1).
//
// Formato del archivo: numeros de pagina separados por espacios, comas o
// saltos de linea. Las lineas que empiezan con '#' son comentarios, lo que
// permite documentar cada cadena dentro del propio archivo.
//
// Politica de errores: una entrada invalida NO aborta la lectura. Se reporta
// por stderr indicando la linea y se descarta, de modo que un archivo
// parcialmente corrupto sigue siendo utilizable y el usuario se entera de
// exactamente que se ignoro.
// ---------------------------------------------------------------------------
inline std::vector<int> leerCadena(const std::string& ruta) {
    std::ifstream archivo(ruta.c_str());
    if (!archivo.is_open()) {
        throw std::runtime_error("No se pudo abrir el archivo de cadena: " + ruta);
    }

    std::vector<int> cadena;
    std::string linea;
    int numeroLinea = 0;

    while (std::getline(archivo, linea)) {
        numeroLinea++;

        const size_t comentario = linea.find('#');
        if (comentario != std::string::npos) linea = linea.substr(0, comentario);

        // Las comas se tratan como separadores, igual que los espacios.
        for (size_t i = 0; i < linea.size(); i++) {
            if (linea[i] == ',' || linea[i] == ';' || linea[i] == '\t') linea[i] = ' ';
        }

        std::istringstream ss(linea);
        std::string token;
        while (ss >> token) {
            try {
                const int pagina = std::stoi(token);
                if (pagina < 0) {
                    std::cerr << "Aviso: linea " << numeroLinea
                              << ": numero de pagina negativo (" << token
                              << "), se ignora.\n";
                    continue;
                }
                cadena.push_back(pagina);
            } catch (const std::exception&) {
                std::cerr << "Aviso: linea " << numeroLinea
                          << ": token no numerico (\"" << token << "\"), se ignora.\n";
            }
        }
    }

    if (cadena.empty()) {
        throw std::runtime_error("La cadena de referencias quedo vacia: " + ruta);
    }
    return cadena;
}

// Cuantas paginas distintas aparecen en la cadena. Es el dato que determina
// a partir de que numero de marcos deja de haber reemplazos.
inline int paginasDistintas(const std::vector<int>& cadena) {
    if (cadena.empty()) return 0;
    int maximo = 0;
    for (size_t i = 0; i < cadena.size(); i++)
        if (cadena[i] > maximo) maximo = cadena[i];
    std::vector<bool> visto(static_cast<size_t>(maximo) + 1, false);
    int n = 0;
    for (size_t i = 0; i < cadena.size(); i++) {
        if (!visto[static_cast<size_t>(cadena[i])]) {
            visto[static_cast<size_t>(cadena[i])] = true;
            n++;
        }
    }
    return n;
}

#endif

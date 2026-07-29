#ifndef LECTOR_H
#define LECTOR_H

#include <vector>
#include <string>
#include "Proceso.h"

// Lee un archivo de procesos (formato: id,llegada,rafaga por linea).
// Lanza std::runtime_error si el archivo no se puede abrir.
// Lineas malformadas se reportan por stderr y se ignoran (no abortan la lectura).
std::vector<Proceso> leerProcesos(const std::string& rutaArchivo);

#endif

// ---------------------------------------------------------------------------
// AnalisisProcFS.h - Analisis de una linea de /proc/[pid]/stat
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Separa el ANALISIS DE TEXTO de la linea de /proc/[pid]/stat del acto de
//   leer el archivo. Es solo manipulacion de cadenas: no abre nada, no depende
//   del sistema operativo y compila igual en Linux y en Windows.
//
// POR QUE ESTA SEPARADO (y no dentro de SistemaInfo.cpp)
//   Porque asi se puede PROBAR. Estando embebido en la funcion que abre el
//   archivo, la unica forma de ejercitarlo era correrlo contra los procesos
//   reales de la maquina, y los procesos reales casi nunca tienen nombres
//   raros: la suite pasaba entera aunque el analisis estuviera roto.
//
//   Se descubrio con una prueba de mutacion: al cambiar rfind por find -es
//   decir, al introducir a proposito el error clasico de este formato- las 47
//   pruebas seguian pasando. Un error que ningun test detecta es un error que
//   va a llegar a la entrega. Sacando el analisis a una funcion pura, se le
//   pueden dar lineas sinteticas con los casos dificiles y el hueco se cierra.
//
// EL FORMATO (proc(5))
//   Campo 1  pid
//   Campo 2  nombre del ejecutable ENTRE PARENTESIS  <- la trampa
//   Campo 3  estado (R, S, D, Z, T)
//   Campo 14 utime   tics de reloj en modo usuario
//   Campo 15 stime   tics de reloj dentro del kernel
//   Campo 22 starttime  tics desde el arranque del sistema
//   Campo 24 rss     paginas residentes (NO bytes)
//
//   El campo 2 puede contener espacios y parentesis, porque es el nombre del
//   binario tal cual. Un proceso llamado "mi (prog) raro" produce:
//       1234 (mi (prog) raro) S 1 ...
//   Partir por espacios da nombre "(mi", estado "(prog)" y TODOS los campos
//   numericos corridos dos lugares, sin ningun sintoma visible: la memoria y
//   los tiempos saldrian mal pero con pinta de numeros validos.
//
//   La solucion, la misma que usa el codigo del kernel, es buscar el ULTIMO
//   parentesis de cierre de la linea.
// ---------------------------------------------------------------------------
#ifndef ANALISISPROCFS_H
#define ANALISISPROCFS_H

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

namespace procfs {

// Los campos crudos, tal como vienen: sin convertir tics a segundos ni paginas
// a bytes. Esa conversion necesita sysconf(), que si es de plataforma, y por
// eso se hace fuera.
struct CamposStat {
    bool               ok = false;
    long               pid = 0;
    std::string        nombre;
    char               estado = '?';
    unsigned long long utime = 0;       // tics
    unsigned long long stime = 0;       // tics
    unsigned long long starttime = 0;   // tics desde el arranque del sistema
    unsigned long long paginasResidentes = 0;
};

// Analiza una linea completa de /proc/[pid]/stat.
// Devuelve ok == false ante cualquier linea que no encaje, en vez de lanzar:
// el contenido lo genera el kernel, pero un proceso puede morir a mitad de la
// lectura y dejar una linea truncada.
inline CamposStat analizarLineaStat(const std::string& linea) {
    CamposStat c;

    const std::size_t abre   = linea.find('(');
    const std::size_t cierra = linea.rfind(')');   // el ULTIMO, no el primero
    if (abre == std::string::npos || cierra == std::string::npos || cierra < abre)
        return c;

    // Campo 1: el pid, antes del parentesis.
    const std::string textoPid = linea.substr(0, abre);
    char* fin = nullptr;
    const long pid = std::strtol(textoPid.c_str(), &fin, 10);
    if (fin == textoPid.c_str() || pid <= 0) return c;
    c.pid = pid;

    // Campo 2: el nombre, entre el primer '(' y el ultimo ')'.
    c.nombre = linea.substr(abre + 1, cierra - abre - 1);
    if (c.nombre.empty()) return c;

    // A partir de aca los campos ya no tienen sorpresas y se parten por
    // espacios. v[0] es el campo 3, asi que el campo N esta en v[N - 3].
    std::vector<std::string> v;
    {
        std::istringstream ss(linea.substr(cierra + 1));
        std::string x;
        while (ss >> x) v.push_back(x);
    }
    if (v.size() < 22) return c;   // hasta el campo 24 hacen falta 22 elementos

    c.estado = v[0].empty() ? '?' : v[0][0];

    auto numero = [](const std::string& s) -> unsigned long long {
        return std::strtoull(s.c_str(), nullptr, 10);
    };
    c.utime             = numero(v[11]);   // campo 14
    c.stime             = numero(v[12]);   // campo 15
    c.starttime         = numero(v[19]);   // campo 22
    c.paginasResidentes = numero(v[21]);   // campo 24

    c.ok = true;
    return c;
}

} // namespace procfs

#endif // ANALISISPROCFS_H

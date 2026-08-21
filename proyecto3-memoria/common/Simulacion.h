#ifndef SIMULACION_H
#define SIMULACION_H

#include <string>
#include <vector>
#include "Cadena.h"

const int MARCO_LIBRE = -1;   // marco todavia no ocupado
const int SIN_VICTIMA = -1;   // el fallo se resolvio sin desalojar a nadie

// ---------------------------------------------------------------------------
// Un paso de la simulacion (RF-5: traza paso a paso).
//
// Guarda todo lo necesario para reconstruir que paso en cada referencia sin
// tener que volver a ejecutar: que pagina se pidio, si fue acierto o fallo,
// a quien se desalojo, y como quedaron los marcos despues.
// ---------------------------------------------------------------------------
struct Paso {
    int              indice = 0;              // posicion dentro de la cadena
    int              pagina = 0;              // pagina solicitada
    bool             fallo = false;           // true = page fault
    int              victima = SIN_VICTIMA;   // pagina desalojada, si hubo
    int              marcoUsado = -1;         // marco donde quedo la pagina
    std::vector<int> marcos;                  // estado despues de la operacion
};

// ---------------------------------------------------------------------------
// Resultado agregado de una corrida (RF-4).
//
// Se reportan aciertos ademas de fallos porque el docente mostro en clase un
// ejemplo de salida con ambas columnas, aunque el enunciado escrito solo pida
// contabilizar los fallos.
// ---------------------------------------------------------------------------
struct Metricas {
    std::string algoritmo;
    std::string cadena;          // nombre del archivo de entrada
    int         marcos = 0;
    long        referencias = 0;
    long        fallos = 0;
    long        aciertos = 0;
    double      microsegundos = 0.0;

    double tasaFallos() const {
        return referencias > 0 ? 100.0 * static_cast<double>(fallos)
                                       / static_cast<double>(referencias) : 0.0;
    }
    double tasaAciertos() const { return 100.0 - tasaFallos(); }

    // Control de consistencia interna: todo acceso es acierto o fallo, nunca
    // ambos ni ninguno. Se verifica en cada corrida.
    bool coherente() const { return aciertos + fallos == referencias; }
};

// ---------------------------------------------------------------------------
// Ejecuta un algoritmo sobre una cadena completa.
//
// Es plantilla y no herencia con metodos virtuales a proposito: una llamada
// virtual por referencia, sobre cadenas de miles de accesos, agregaria una
// indireccion al camino caliente y contaminaria la medicion de tiempos, que
// es justamente una de las magnitudes que el proyecto compara.
//
// Si 'traza' no es nulo, se registra el estado paso a paso. La traza se
// desactiva en las corridas de medicion para que el costo de guardarla no se
// sume al tiempo del algoritmo.
// ---------------------------------------------------------------------------
template <typename TAlgoritmo>
Metricas simular(const std::vector<int>& cadena, int marcos,
                 const std::string& nombreAlgoritmo, const std::string& nombreCadena,
                 std::vector<Paso>* traza = 0) {
    TAlgoritmo algoritmo(marcos, cadena);

    Metricas m;
    m.algoritmo   = nombreAlgoritmo;
    m.cadena      = nombreCadena;
    m.marcos      = marcos;
    m.referencias = static_cast<long>(cadena.size());

    if (traza) traza->reserve(cadena.size());

    const Reloj::time_point t0 = Reloj::now();
    for (size_t i = 0; i < cadena.size(); i++) {
        Paso paso;
        const bool fallo = algoritmo.acceder(i, paso);
        if (fallo) m.fallos++; else m.aciertos++;
        if (traza) traza->push_back(paso);
    }
    const Reloj::time_point t1 = Reloj::now();

    m.microsegundos = std::chrono::duration<double, std::micro>(t1 - t0).count();
    return m;
}

#endif

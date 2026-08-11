#ifndef REGISTRO_H
#define REGISTRO_H

#include <string>
#include <vector>
#include "Item.h"

// ---------------------------------------------------------------------------
// Instrumentacion (RF-4, RF-9).
//
// REGLA DE DISENO: durante la ejecucion NINGUN hilo comparte estructura de
// medicion con otro. Cada productor y cada consumidor escribe solo en SU
// propio registro; la reconciliacion se hace despues de todos los join(), en
// un unico hilo.
//
// Motivo: si los hilos anotaran lo producido y lo consumido en una estructura
// compartida, ese registro sufriria a su vez condiciones de carrera y las
// cifras del informe no probarian nada. El fenomeno medido es no determinista,
// pero la medicion tiene que ser determinista y auditable.
// ---------------------------------------------------------------------------

// Un evento del log (RF-9): permite reconstruir que hilo hizo que y cuando.
struct Evento {
    Reloj::time_point instante;
    char   tipo;            // 'P' produccion, 'C' consumo
    int    hilo;            // indice del hilo dentro de su grupo
    long   id_unico;
    long   valor;
    int    posicion;
    int    cuenta_antes;
    int    cuenta_despues;
    bool   integro;         // solo aplica al consumo
};

struct RegistroProductor {
    std::vector<long>   ids;        // ids entregados al buffer
    long                suma = 0;   // suma de los valores entregados
    int                 max_cuenta = 0;
    std::vector<Evento> eventos;    // vacio si el log esta desactivado
};

struct RegistroConsumidor {
    std::vector<Item>   items;      // items extraidos
    long                suma = 0;   // suma de los valores extraidos
    int                 max_cuenta = 0;
    std::vector<Evento> eventos;
};

struct Metricas {
    long producidos      = 0;
    long consumidos      = 0;
    long suma_producida  = 0;   // invariante obligatorio de RF-4
    long suma_consumida  = 0;
    long perdidos        = 0;   // producidos que nadie consumio
    long duplicados      = 0;   // consumidos mas de una vez
    long fantasma        = 0;   // consumidos que nadie produjo
    long corruptos       = 0;   // verificacion no satisfecha
    long entregados_ok   = 0;   // validos entregados exactamente una vez
    int  max_cuenta      = 0;
    int  capacidad       = 0;
    double milisegundos  = 0.0;

    bool excedioCapacidad() const { return max_cuenta > capacidad; }
    bool sumasCoinciden()   const { return suma_producida == suma_consumida; }
    long inconsistencias()  const { return perdidos + duplicados + fantasma + corruptos; }

    // Criterio de aceptacion de RF-4 / RF-5 / RF-6: la corrida pasa la
    // verificacion de invariantes solo si se cumple TODO lo siguiente.
    bool invarianteOk() const {
        return producidos == consumidos
            && sumasCoinciden()
            && !excedioCapacidad()
            && inconsistencias() == 0;
    }
};

Metricas reconciliar(const std::vector<RegistroProductor>& productores,
                     const std::vector<RegistroConsumidor>& consumidores,
                     int capacidad);

// Vuelca el log de eventos ordenado por instante. Devuelve false si no pudo
// abrir el archivo.
bool volcarLog(const std::string& ruta,
               const std::vector<RegistroProductor>& productores,
               const std::vector<RegistroConsumidor>& consumidores,
               Reloj::time_point origen);

#endif

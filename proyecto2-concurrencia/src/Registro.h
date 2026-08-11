#ifndef REGISTRO_H
#define REGISTRO_H

#include <vector>
#include "Item.h"

// ---------------------------------------------------------------------------
// Instrumentacion.
//
// REGLA DE DISENO: durante la corrida NINGUN hilo comparte estructura de
// medicion con otro. Cada productor y cada consumidor escribe solo en SU
// propio registro. La reconciliacion se hace despues de todos los join(),
// en un solo hilo.
//
// Motivo: si los consumidores anotaran lo consumido en una estructura
// compartida, ese registro tambien sufriria carreras y los numeros del
// informe no probarian nada. El fenomeno medido es no determinista, pero
// la medicion tiene que ser determinista.
// ---------------------------------------------------------------------------

struct RegistroProductor {
    std::vector<long> ids;      // ids que este productor entrego al buffer
    int  max_cuenta = 0;        // maximo de buffer.cuentaActual() que observo
    long timeouts   = 0;        // corto por el watchdog duro
    long drenados   = 0;        // corto por falta de progreso (ver main.cpp)
    Reloj::time_point ultimo{}; // momento de su ultima operacion exitosa
};

struct RegistroConsumidor {
    std::vector<Item> items;    // items que este consumidor extrajo
    int  max_cuenta = 0;
    long timeouts   = 0;
    long drenados   = 0;
    Reloj::time_point ultimo{};
};

struct Metricas {
    long producidos      = 0;
    long consumidos      = 0;
    long perdidos        = 0;   // producidos que nadie consumio
    long duplicados      = 0;   // consumidos mas de una vez
    long fantasma        = 0;   // consumidos que nadie produjo
    long corruptos       = 0;   // suma de control invalida
    long entregados_ok   = 0;   // items validos entregados exactamente una vez
    long timeouts        = 0;
    long drenados        = 0;
    int  max_cuenta      = 0;
    bool violo_capacidad = false;
    double segundos      = 0.0;

    long inconsistencias() const {
        return perdidos + duplicados + fantasma + corruptos;
    }
};

Metricas reconciliar(const std::vector<RegistroProductor>& productores,
                     const std::vector<RegistroConsumidor>& consumidores,
                     int capacidad);

#endif

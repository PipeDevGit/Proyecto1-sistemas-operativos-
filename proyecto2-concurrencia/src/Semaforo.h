#ifndef SEMAFORO_H
#define SEMAFORO_H

#include <condition_variable>
#include <mutex>
#include "Item.h"   // define el tipo Reloj

// ===========================================================================
// SEMAFORO CONTADOR, CONSTRUIDO A MANO
//
// Se implementa en vez de usar std::counting_semaphore por tres razones:
//
//  1. std::counting_semaphore es de C++20. Se verifico en la VM: compila con
//     -std=c++20 y falla con -std=c++17. Todo el proyecto usa C++17, igual
//     que el Proyecto 1, para no arriesgar la compilacion en MinGW y MSVC.
//
//  2. Construir uno desde cero demuestra que se entiende QUE ES un semaforo,
//     no solo como llamarlo.
//
//  3. Permite nombrar las operaciones como las nombro Dijkstra: P (esperar,
//     del neerlandes "proberen", probar) y V (senalar, "verhogen",
//     incrementar).
//
// ---------------------------------------------------------------------------
// LA DIFERENCIA CON UN MUTEX (pregunta muy probable en la Defensa)
//
//  - Un mutex tiene PROPIETARIO: el hilo que hace lock es el unico que puede
//    hacer unlock. Protege un unico recurso.
//
//  - Un semaforo NO tiene propietario: cualquier hilo puede senalarlo, no
//    solo el que espero. Es un CONTADOR de permisos. Sirve para limitar
//    cuantos hilos acceden a la vez, o para que un hilo le avise a otro.
//
//  Esa ausencia de propiedad es justamente lo que permite el patron de abajo:
//  el productor senala el semaforo "llenos", que es el que esperan los
//  consumidores. Un mutex no podria hacer eso.
// ===========================================================================
class Semaforo {
public:
    explicit Semaforo(int inicial) : cuenta_(inicial) {}

    // Operacion P (Dijkstra) / wait / down.
    // Espera a que haya al menos un permiso y consume uno.
    // Devuelve false si se agoto el tiempo limite.
    bool esperar(Reloj::time_point limite) {
        std::unique_lock<std::mutex> cerrojo(mutex_);
        if (!cv_.wait_until(cerrojo, limite, [this] { return cuenta_ > 0; })) {
            return false;
        }
        cuenta_--;
        return true;
    }

    // Operacion V (Dijkstra) / signal / up.
    // Devuelve un permiso y despierta a un hilo que estuviera esperando.
    // No puede fallar ni bloquear.
    void senalar() {
        {
            std::lock_guard<std::mutex> cerrojo(mutex_);
            cuenta_++;
        }
        // Se notifica FUERA del bloqueo: si se hiciera adentro, el hilo
        // despertado encontraria el mutex ocupado y volveria a dormirse.
        cv_.notify_one();
    }

private:
    std::mutex              mutex_;
    std::condition_variable cv_;
    int                     cuenta_;   // permisos disponibles
};

#endif

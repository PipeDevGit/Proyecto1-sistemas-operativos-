#ifndef BUFFER_SEMAFOROS_H
#define BUFFER_SEMAFOROS_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include "Item.h"

// ---------------------------------------------------------------------------
// Semaforo contador, construido a mano.
//
// Se implementa en vez de usar std::counting_semaphore porque esa plantilla es
// de C++20: se comprobo que compila con -std=c++20 y falla con -std=c++17, y
// todo el proyecto usa C++17. Ademas, construirlo desde cero permite nombrar
// las operaciones como las nombro Dijkstra: P (esperar, del neerlandes
// "proberen") y V (senalar, "verhogen").
//
// DIFERENCIA CON UN MUTEX: un mutex tiene propietario, solo el hilo que lo
// adquirio puede liberarlo, y protege un unico recurso. Un semaforo NO tiene
// propietario: cualquier hilo puede senalarlo. Esa ausencia de pertenencia es
// justamente lo que hace posible el diseno de mas abajo, donde el productor
// senala el semaforo que esperan los consumidores.
// ---------------------------------------------------------------------------
class Semaforo {
public:
    explicit Semaforo(int inicial) : cuenta_(inicial) {}

    void esperar() {                                  // P / wait / down
        std::unique_lock<std::mutex> cerrojo(mutex_);
        cv_.wait(cerrojo, [this] { return cuenta_ > 0; });
        cuenta_--;
    }

    void senalar() {                                  // V / signal / up
        {
            std::lock_guard<std::mutex> cerrojo(mutex_);
            cuenta_++;
        }
        cv_.notify_one();
    }

private:
    std::mutex              mutex_;
    std::condition_variable cv_;
    int                     cuenta_;
};

// ===========================================================================
// EXTRA - SOLUCION CLASICA CON TRES SEMAFOROS
//
// No forma parte de los entregables exigidos. Se incluye porque la unidad
// dedica una lamina completa a contrastar mutex y semaforo, y porque permite
// medir el costo de expresar la misma logica con una arquitectura distinta.
//
//   vacios_     espacios libres            (inicia en N)
//   llenos_     items disponibles          (inicia en 0)
//   exclusion_  binario, hace de cerrojo   (inicia en 1)
//
// La condicion de lleno o vacio ya no se evalua con un if: queda CODIFICADA en
// el contador del semaforo.
// ===========================================================================
class BufferSemaforos {
public:
    explicit BufferSemaforos(int capacidad)
        : capacidad_(capacidad),
          datos_(static_cast<size_t>(capacidad), itemVacio()),
          cabeza_(0), cola_(0), cuenta_(0), fin_(false),
          vacios_(capacidad), llenos_(0), exclusion_(1) {}

    void producir(const Item& it, Traza& tz) {
        // ORDEN DE ADQUISICION: primero el semaforo de recurso, despues el de
        // exclusion mutua. Invertirlo produce interbloqueo: un productor se
        // quedaria con el cerrojo esperando espacio que solo un consumidor
        // puede liberar, y ese consumidor no podria entrar. Ordenar la
        // adquisicion es la tecnica de prevencion por eliminacion de la
        // espera circular.
        vacios_.esperar();
        exclusion_.esperar();

        // === INICIO SECCIÓN CRÍTICA ===
        const int pos = cabeza_ % capacidad_;
        tz.cuenta_antes = cuenta_.load();
        tz.posicion     = pos;

        datos_[static_cast<size_t>(pos)] = it;
        cabeza_ = (cabeza_ + 1) % capacidad_;
        cuenta_++;

        tz.cuenta_despues = cuenta_.load();
        // === FIN SECCIÓN CRÍTICA ===

        exclusion_.senalar();
        llenos_.senalar();
    }

    bool consumir(Item& destino, Traza& tz) {
        llenos_.esperar();

        if (fin_.load() && cuenta_.load() <= 0) {
            // Fin de produccion y nada pendiente. Se repropaga el permiso para
            // que el siguiente consumidor dormido tambien despierte y salga
            // (cascada); asi no hace falta conocer cuantos consumidores hay.
            llenos_.senalar();
            return false;
        }

        exclusion_.esperar();

        // === INICIO SECCIÓN CRÍTICA ===
        const int pos = cola_ % capacidad_;
        tz.cuenta_antes = cuenta_.load();
        tz.posicion     = pos;

        destino = datos_[static_cast<size_t>(pos)];
        cola_ = (cola_ + 1) % capacidad_;
        cuenta_--;

        tz.cuenta_despues = cuenta_.load();
        // === FIN SECCIÓN CRÍTICA ===

        exclusion_.senalar();
        vacios_.senalar();
        return true;
    }

    // Un unico permiso alcanza: el primer consumidor que despierte y decida
    // salir lo repropaga, encadenando la salida de todos los demas.
    void marcarFinProduccion() {
        fin_.store(true);
        llenos_.senalar();
    }

    int capacidad() const { return capacidad_; }

private:
    int               capacidad_;
    std::vector<Item> datos_;
    int               cabeza_;
    int               cola_;
    // Atomico solo para poder observarlo en la traza sin introducir una
    // carrera; el control de lleno y vacio lo hacen los semaforos, no este
    // contador.
    std::atomic<int>  cuenta_;
    std::atomic<bool> fin_;

    Semaforo vacios_;
    Semaforo llenos_;
    Semaforo exclusion_;
};

#endif

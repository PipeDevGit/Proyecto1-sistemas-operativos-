#ifndef BUFFER_B_H
#define BUFFER_B_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <vector>
#include "Item.h"

// ===========================================================================
// VERSION B - CON SINCRONIZACION
//
// Mismo diseno logico, mismos parametros y misma instrumentacion que la
// version A. La UNICA diferencia es la presencia de sincronizacion.
//
// Mecanismos:
//   - std::mutex protege la seccion critica (exclusion mutua).
//   - std::condition_variable coordina la espera por buffer lleno o vacio,
//     de modo que NO hay espera activa en ninguna parte (RNF-5).
//   - RAII en todos los casos: unique_lock y lock_guard. No se llama a lock()
//     ni a unlock() manualmente salvo el unlock() previo a notificar, que se
//     explica mas abajo (RNF-4).
// ===========================================================================
class BufferB {
public:
    explicit BufferB(int capacidad)
        : capacidad_(capacidad),
          datos_(static_cast<size_t>(capacidad), itemVacio()),
          cabeza_(0), cola_(0), cuenta_(0), fin_(false) {}

    void producir(const Item& it, Traza& tz) {
        // unique_lock y no lock_guard: la variable de condicion necesita poder
        // soltar y volver a tomar el mutex mientras el hilo duerme.
        std::unique_lock<std::mutex> cerrojo(mutex_);

        // wait con predicado hace tres cosas de una: si la condicion ya se
        // cumple sigue de largo; si no, suelta el mutex y duerme; y al
        // despertar vuelve a tomarlo y REEVALUA el predicado. Ese ultimo paso
        // es imprescindible porque una variable de condicion puede despertar
        // sin que nadie haya notificado (despertar espurio).
        hay_lugar_.wait(cerrojo, [this] { return cuenta_ < capacidad_; });

        // === INICIO SECCIÓN CRÍTICA ===
        //
        // Mismas operaciones que en la version A. La diferencia de fondo es
        // que aqui la comprobacion de que hay lugar y la escritura ocurren
        // dentro del mismo bloqueo, de modo que ningun otro hilo puede
        // colarse entre ambas. Ese hueco era el origen de toda la corrupcion.
        const int pos = cabeza_ % capacidad_;
        tz.cuenta_antes = cuenta_;
        tz.posicion     = pos;

        datos_[static_cast<size_t>(pos)] = it;
        cabeza_ = (cabeza_ + 1) % capacidad_;
        cuenta_ = cuenta_ + 1;

        tz.cuenta_despues = cuenta_;
        // === FIN SECCIÓN CRÍTICA ===

        // Se suelta el mutex ANTES de notificar. Si se notificara con el
        // cerrojo tomado, el hilo despertado lo encontraria ocupado y volveria
        // a bloquearse de inmediato: dos cambios de contexto para nada.
        cerrojo.unlock();
        hay_datos_.notify_one();
    }

    // Devuelve false cuando el buffer esta vacio y la produccion ya termino.
    // No hay temporizadores ni espera activa (RF-7, RNF-5).
    bool consumir(Item& destino, Traza& tz) {
        std::unique_lock<std::mutex> cerrojo(mutex_);

        // Se despierta si hay algo que consumir o si ya no vendra nada mas.
        hay_datos_.wait(cerrojo, [this] { return cuenta_ > 0 || fin_.load(); });

        if (cuenta_ == 0) return false;   // vacio y produccion terminada

        // === INICIO SECCIÓN CRÍTICA ===
        const int pos = cola_ % capacidad_;
        tz.cuenta_antes = cuenta_;
        tz.posicion     = pos;

        destino = datos_[static_cast<size_t>(pos)];
        cola_ = (cola_ + 1) % capacidad_;
        cuenta_ = cuenta_ - 1;

        tz.cuenta_despues = cuenta_;
        // === FIN SECCIÓN CRÍTICA ===

        cerrojo.unlock();
        hay_lugar_.notify_one();
        return true;
    }

    // La bandera se activa bajo el cerrojo y se notifica a TODOS los
    // consumidores: cualquier numero de ellos puede estar dormido esperando
    // datos que ya no llegaran, y todos deben despertar para salir.
    void marcarFinProduccion() {
        {
            std::lock_guard<std::mutex> cerrojo(mutex_);
            fin_.store(true);
        }
        hay_datos_.notify_all();
    }

    int capacidad() const { return capacidad_; }

private:
    int                     capacidad_;
    std::vector<Item>       datos_;
    int                     cabeza_;
    int                     cola_;
    int                     cuenta_;
    std::atomic<bool>       fin_;

    mutable std::mutex      mutex_;      // protege datos_, cabeza_, cola_ y cuenta_

    // DOS variables de condicion, no una. Los productores esperan "hay lugar"
    // y los consumidores "hay datos": son condiciones distintas. Con una sola,
    // cada notificacion tendria probabilidad apreciable de despertar al tipo de
    // hilo equivocado, que reevaluaria su predicado, lo encontraria falso y se
    // volveria a dormir.
    std::condition_variable hay_lugar_;  // esperan los productores
    std::condition_variable hay_datos_;  // esperan los consumidores
};

#endif

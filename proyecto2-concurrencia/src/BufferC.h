#ifndef BUFFER_C_H
#define BUFFER_C_H

#include <condition_variable>
#include <mutex>
#include <vector>
#include "Item.h"   // define Item y el tipo Reloj

// ===========================================================================
// VERSION C - MUTEX + VARIABLE DE CONDICION
//
// Mismo mutex que la version B, misma correccion. Lo unico que cambia es
// COMO se espera: en vez de girar consultando (quemando procesador), el hilo
// se duerme y el sistema operativo lo despierta cuando la condicion cambia.
//
// Que aisla esta variante: comparada contra B mide EL EFECTO DE ELIMINAR LA
// ESPERA ACTIVA, porque la proteccion es identica en las dos. Es la
// respuesta con datos propios a la pregunta de la lamina 9 de la Unidad III:
// "por que el kernel usa espera activa en lugar de bloquear el hilo".
// ===========================================================================
class BufferC {
public:
    explicit BufferC(int capacidad)
        : capacidad_(capacidad),
          datos_(static_cast<size_t>(capacidad), itemVacio()),
          cabeza_(0),
          cola_(0),
          cuenta_(0) {}

    bool producir(const Item& it, Reloj::time_point limite) {
        // unique_lock y no lock_guard: la variable de condicion necesita
        // poder SOLTAR y volver a TOMAR el mutex mientras el hilo duerme,
        // y lock_guard no permite eso.
        std::unique_lock<std::mutex> cerrojo(mutex_);

        // wait_until con predicado hace tres cosas de una:
        //   1. Si el predicado ya es cierto, sigue de largo sin dormirse.
        //   2. Si no, suelta el mutex y duerme hasta que alguien notifique.
        //   3. Al despertar, vuelve a tomar el mutex y REVISA el predicado.
        //
        // El paso 3 es imprescindible: una variable de condicion puede
        // despertar sola, sin que nadie haya notificado (despertar espurio).
        // La forma con predicado protege de eso automaticamente; escrito a
        // mano habria que envolverlo en un while.
        //
        // Devuelve el valor del predicado: false significa que se agoto el
        // tiempo sin que la condicion llegara a cumplirse.
        if (!hay_lugar_.wait_until(cerrojo, limite,
                                   [this] { return cuenta_ < capacidad_; })) {
            return false;
        }

        // #######################################################
        // ###  SECCION CRITICA  -  PROTEGIDA POR mutex_       ###
        // #######################################################
        datos_[cabeza_] = it;
        cabeza_ = (cabeza_ + 1) % capacidad_;
        cuenta_ = cuenta_ + 1;
        // #######################################################

        // Soltar el mutex ANTES de notificar. Si se notificara con el mutex
        // todavia tomado, el hilo despertado se encontraria el cerrojo
        // ocupado y volveria a bloquearse de inmediato: se lo despierta para
        // que se vuelva a dormir, gastando dos cambios de contexto para nada.
        cerrojo.unlock();
        hay_datos_.notify_one();
        return true;
    }

    bool consumir(Item& destino, Reloj::time_point limite) {
        std::unique_lock<std::mutex> cerrojo(mutex_);

        if (!hay_datos_.wait_until(cerrojo, limite,
                                   [this] { return cuenta_ > 0; })) {
            return false;
        }

        // #######################################################
        // ###  SECCION CRITICA  -  PROTEGIDA POR mutex_       ###
        // #######################################################
        destino = datos_[cola_];
        cola_ = (cola_ + 1) % capacidad_;
        cuenta_ = cuenta_ - 1;
        // #######################################################

        cerrojo.unlock();
        hay_lugar_.notify_one();
        return true;
    }

    int cuentaActual() const {
        std::lock_guard<std::mutex> cerrojo(mutex_);
        return cuenta_;
    }

    int capacidad() const { return capacidad_; }

private:
    int                capacidad_;
    std::vector<Item>  datos_;
    int                cabeza_;
    int                cola_;
    int                cuenta_;

    mutable std::mutex mutex_;

    // DOS variables de condicion, no una. Los productores esperan "hay
    // lugar" y los consumidores esperan "hay datos": son condiciones
    // distintas. Con una sola, un notify despertaria igual de probable al
    // tipo de hilo equivocado, que verificaria su predicado, lo encontraria
    // falso y se volveria a dormir. Separarlas hace que cada notificacion
    // despierte a alguien que si puede avanzar.
    std::condition_variable hay_lugar_;   // esperan los productores
    std::condition_variable hay_datos_;   // esperan los consumidores
};

#endif

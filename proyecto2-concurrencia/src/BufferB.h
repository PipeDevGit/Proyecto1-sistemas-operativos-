#ifndef BUFFER_B_H
#define BUFFER_B_H

#include <mutex>
#include <vector>
#include "Item.h"   // define Item y el tipo Reloj

// ===========================================================================
// VERSION B - MUTEX CON ESPERA ACTIVA
//
// Misma estrategia de espera que la version A (girar consultando, sin
// dormirse), pero la seccion critica queda protegida por un mutex.
//
// Que aisla esta variante: comparada contra A mide EL COSTO DEL MUTEX Y NADA
// MAS, porque las dos esperan igual. Si se comparara A contra la version C
// (que duerme), el numero mezclaria dos cosas distintas: el costo de
// bloquear y el efecto de dejar de quemar CPU.
//
// Es tambien el analogo en espacio de usuario del spinlock de xv6: el hilo
// gira en vez de ceder el procesador.
// ===========================================================================
class BufferB {
public:
    explicit BufferB(int capacidad)
        : capacidad_(capacidad),
          datos_(static_cast<size_t>(capacidad), itemVacio()),
          cabeza_(0),
          cola_(0),
          cuenta_(0) {}

    bool producir(const Item& it, Reloj::time_point limite) {
        for (;;) {
            {
                // Bloque interno a proposito: el mutex se toma aca y se
                // suelta al cerrar la llave, ANTES de volver a girar.
                std::lock_guard<std::mutex> cerrojo(mutex_);

                // ###########################################################
                // ###  SECCION CRITICA  -  PROTEGIDA POR mutex_           ###
                // ###########################################################
                //
                // La diferencia de fondo con la version A esta aca: el
                // "verificar" y el "actuar" ocurren DENTRO del mismo bloqueo,
                // asi que ningun otro hilo puede colarse entre los dos. Ese
                // hueco era el origen de toda la corrupcion de A.
                if (cuenta_ < capacidad_) {
                    datos_[cabeza_] = it;
                    cabeza_ = (cabeza_ + 1) % capacidad_;
                    cuenta_ = cuenta_ + 1;
                    return true;   // lock_guard suelta el mutex al salir
                }
                // ###########################################################
                // ###  FIN SECCION CRITICA                                ###
                // ###########################################################
            }

            // IMPORTANTE: el mutex ya esta suelto cuando se llega aca.
            // Si se girara sin soltarlo, este hilo esperaria a que el buffer
            // se vacie mientras impide que cualquier consumidor lo vacie:
            // un interbloqueo con las cuatro condiciones de Coffman.
            if (Reloj::now() > limite) return false;
        }
    }

    bool consumir(Item& destino, Reloj::time_point limite) {
        for (;;) {
            {
                std::lock_guard<std::mutex> cerrojo(mutex_);

                // ###########################################################
                // ###  SECCION CRITICA  -  PROTEGIDA POR mutex_           ###
                // ###########################################################
                if (cuenta_ > 0) {
                    destino = datos_[cola_];
                    cola_ = (cola_ + 1) % capacidad_;
                    cuenta_ = cuenta_ - 1;
                    return true;
                }
                // ###########################################################
            }
            if (Reloj::now() > limite) return false;
        }
    }

    // Lectura protegida: a diferencia de la version A, aca no hay carrera.
    // Es mutable el mutex porque el metodo es const y aun asi necesita
    // bloquear (es la excepcion clasica y legitima a mutable).
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
    mutable std::mutex mutex_;   // protege datos_, cabeza_, cola_ y cuenta_
};

#endif

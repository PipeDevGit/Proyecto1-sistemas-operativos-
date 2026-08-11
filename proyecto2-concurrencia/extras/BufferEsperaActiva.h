#ifndef BUFFER_ESPERA_ACTIVA_H
#define BUFFER_ESPERA_ACTIVA_H

#include <atomic>
#include <mutex>
#include <vector>
#include "Item.h"

// ===========================================================================
// EXTRA - MUTEX CON ESPERA ACTIVA
//
// No forma parte de los entregables exigidos. Existe para responder con datos
// propios a RNF-5, que admite la espera activa "siempre que se documente
// explicitamente como decision de diseno con su costo medido".
//
// Es correcta: protege la seccion critica con el mismo mutex que la version B.
// La unica diferencia con la version B es COMO espera: gira consultando en vez
// de dormirse. Por eso compararla contra la version B aisla exactamente el
// efecto de la espera activa, sin mezclarlo con el costo de la exclusion mutua.
//
// Es tambien el analogo en espacio de usuario del spinlock del kernel de xv6.
// ===========================================================================
class BufferEsperaActiva {
public:
    explicit BufferEsperaActiva(int capacidad)
        : capacidad_(capacidad),
          datos_(static_cast<size_t>(capacidad), itemVacio()),
          cabeza_(0), cola_(0), cuenta_(0), fin_(false) {}

    void producir(const Item& it, Traza& tz) {
        for (;;) {
            {
                // Bloque interno a proposito: el cerrojo se toma aqui y se
                // suelta al cerrar la llave, ANTES de volver a girar. Si se
                // girara reteniendolo, este hilo esperaria a que se libere
                // espacio mientras impide que cualquier consumidor lo libere:
                // interbloqueo con las cuatro condiciones de Coffman.
                std::lock_guard<std::mutex> cerrojo(mutex_);

                // === INICIO SECCIÓN CRÍTICA ===
                if (cuenta_ < capacidad_) {
                    const int pos = cabeza_ % capacidad_;
                    tz.cuenta_antes = cuenta_;
                    tz.posicion     = pos;

                    datos_[static_cast<size_t>(pos)] = it;
                    cabeza_ = (cabeza_ + 1) % capacidad_;
                    cuenta_ = cuenta_ + 1;

                    tz.cuenta_despues = cuenta_;
                    return;
                }
                // === FIN SECCIÓN CRÍTICA ===
            }
        }
    }

    bool consumir(Item& destino, Traza& tz) {
        for (;;) {
            {
                std::lock_guard<std::mutex> cerrojo(mutex_);

                // === INICIO SECCIÓN CRÍTICA ===
                if (cuenta_ > 0) {
                    const int pos = cola_ % capacidad_;
                    tz.cuenta_antes = cuenta_;
                    tz.posicion     = pos;

                    destino = datos_[static_cast<size_t>(pos)];
                    cola_ = (cola_ + 1) % capacidad_;
                    cuenta_ = cuenta_ - 1;

                    tz.cuenta_despues = cuenta_;
                    return true;
                }
                if (fin_.load()) return false;
                // === FIN SECCIÓN CRÍTICA ===
            }
        }
    }

    void marcarFinProduccion() { fin_.store(true); }

    int capacidad() const { return capacidad_; }

private:
    int                capacidad_;
    std::vector<Item>  datos_;
    int                cabeza_;
    int                cola_;
    int                cuenta_;
    std::atomic<bool>  fin_;
    mutable std::mutex mutex_;
};

#endif

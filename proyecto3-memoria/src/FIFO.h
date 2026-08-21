#ifndef FIFO_H
#define FIFO_H

#include <unordered_map>
#include <vector>
#include "Simulacion.h"

// ===========================================================================
// FIFO - First In, First Out (RF-2)
//
// Reemplaza la pagina que lleva mas tiempo en memoria, sin importar si se
// acaba de usar. No mira el patron de acceso en absoluto.
//
// ESTRUCTURA DE DATOS
//   marcos_    contenido de cada marco fisico
//   residente_ pagina -> indice de marco, para responder "esta cargada?" en
//              tiempo constante en vez de recorrer los marcos
//   siguiente_ puntero circular al marco que toca reemplazar
//
// El puntero circular es lo que hace innecesaria una cola explicita: como los
// marcos se llenan en orden y cada reemplazo avanza el puntero una posicion,
// el marco apuntado siempre contiene la pagina mas antigua. Es FIFO exacto
// con O(1) de tiempo y sin memoria adicional.
//
// COSTO: O(1) por referencia, tanto en acierto como en fallo.
// ===========================================================================
class FIFO {
public:
    FIFO(int marcos, const std::vector<int>& cadena)
        : marcos_(static_cast<size_t>(marcos), MARCO_LIBRE), siguiente_(0) {
        (void)cadena;   // FIFO no necesita conocer la cadena por adelantado
        residente_.reserve(static_cast<size_t>(marcos) * 2);
        cadena_ = &cadena;
    }

    // Devuelve true si la referencia produjo un fallo de pagina.
    bool acceder(size_t i, Paso& paso) {
        const int pagina = (*cadena_)[i];
        paso.indice = static_cast<int>(i);
        paso.pagina = pagina;

        std::unordered_map<int, int>::iterator it = residente_.find(pagina);
        if (it != residente_.end()) {
            // ACIERTO: la pagina ya esta en un marco. FIFO no reordena nada:
            // el hecho de haberla usado recien no cambia su antiguedad.
            paso.fallo      = false;
            paso.marcoUsado = it->second;
            paso.marcos     = marcos_;
            return false;
        }

        // FALLO: hay que cargarla, desalojando si el marco elegido esta ocupado.
        const int destino = siguiente_;
        const int ocupante = marcos_[static_cast<size_t>(destino)];
        if (ocupante != MARCO_LIBRE) {
            residente_.erase(ocupante);
            paso.victima = ocupante;
        }

        marcos_[static_cast<size_t>(destino)] = pagina;
        residente_[pagina] = destino;
        siguiente_ = (siguiente_ + 1) % static_cast<int>(marcos_.size());

        paso.fallo      = true;
        paso.marcoUsado = destino;
        paso.marcos     = marcos_;
        return true;
    }

private:
    std::vector<int>            marcos_;
    std::unordered_map<int,int> residente_;
    int                         siguiente_;
    const std::vector<int>*     cadena_;
};

#endif

#ifndef LRU_H
#define LRU_H

#include <list>
#include <unordered_map>
#include <vector>
#include "Simulacion.h"

// ===========================================================================
// LRU - Least Recently Used (RF-3)
//
// Reemplaza la pagina que lleva mas tiempo sin usarse. A diferencia de FIFO,
// mira el patron de acceso reciente: una pagina vieja pero muy usada se queda.
//
// ESTRUCTURA DE DATOS (RNF-1 pide una eficiente y justificarla)
//
//   orden_     lista doblemente enlazada de paginas residentes, de mas
//              reciente (frente) a menos reciente (final). El candidato a
//              desalojar es siempre el ultimo.
//   posicion_  pagina -> iterador a su nodo dentro de orden_
//   marcos_    contenido de cada marco fisico
//   residente_ pagina -> indice de marco
//
// POR QUE ESTA COMBINACION. El problema de LRU es que en cada acierto hay que
// mover un elemento al frente. Con un vector eso costaria O(n) por el
// desplazamiento; con una lista enlazada cuesta O(1), pero encontrar el nodo
// costaria O(n) si hubiera que recorrerla. Guardar el iterador en un mapa
// resuelve las dos mitades: se localiza el nodo en O(1) y se mueve en O(1)
// con splice, que reengancha punteros sin copiar ni invalidar el iterador.
//
// Es exactamente la estructura de un cache LRU clasico.
//
// COSTO: O(1) por referencia, tanto en acierto como en fallo.
// ===========================================================================
class LRU {
public:
    LRU(int marcos, const std::vector<int>& cadena)
        : marcos_(static_cast<size_t>(marcos), MARCO_LIBRE), libres_(marcos) {
        residente_.reserve(static_cast<size_t>(marcos) * 2);
        posicion_.reserve(static_cast<size_t>(marcos) * 2);
        cadena_ = &cadena;
    }

    bool acceder(size_t i, Paso& paso) {
        const int pagina = (*cadena_)[i];
        paso.indice = static_cast<int>(i);
        paso.pagina = pagina;

        std::unordered_map<int,int>::iterator it = residente_.find(pagina);
        if (it != residente_.end()) {
            // ACIERTO: se promueve la pagina al frente. splice mueve el nodo
            // sin copiarlo ni invalidar el iterador guardado en posicion_,
            // por eso no hace falta actualizar el mapa.
            orden_.splice(orden_.begin(), orden_, posicion_[pagina]);
            paso.fallo      = false;
            paso.marcoUsado = it->second;
            paso.marcos     = marcos_;
            return false;
        }

        // FALLO
        int destino;
        if (libres_ > 0) {
            // Todavia queda memoria sin usar: no se desaloja a nadie.
            destino = static_cast<int>(marcos_.size()) - libres_;
            libres_--;
        } else {
            // Memoria llena: la victima es la del final de la lista, es decir
            // la que lleva mas tiempo sin referenciarse.
            const int victima = orden_.back();
            orden_.pop_back();
            posicion_.erase(victima);
            destino = residente_[victima];
            residente_.erase(victima);
            paso.victima = victima;
        }

        marcos_[static_cast<size_t>(destino)] = pagina;
        residente_[pagina] = destino;
        orden_.push_front(pagina);
        posicion_[pagina] = orden_.begin();

        paso.fallo      = true;
        paso.marcoUsado = destino;
        paso.marcos     = marcos_;
        return true;
    }

private:
    std::vector<int>                                  marcos_;
    std::unordered_map<int,int>                       residente_;
    std::list<int>                                    orden_;
    std::unordered_map<int, std::list<int>::iterator> posicion_;
    int                                               libres_;
    const std::vector<int>*                           cadena_;
};

#endif

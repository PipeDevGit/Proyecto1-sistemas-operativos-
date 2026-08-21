#ifndef OPTIMO_H
#define OPTIMO_H

#include <deque>
#include <limits>
#include <unordered_map>
#include <vector>
#include "Simulacion.h"

// ===========================================================================
// OPTIMO (OPT / Belady) - NO EXIGIDO POR EL ENUNCIADO
//
// El enunciado pide comparar FIFO y LRU. Este tercer algoritmo se incluye
// porque sin el no hay forma de responder una pregunta natural: cuando LRU
// obtiene, por ejemplo, 40 fallos, ¿eso es bueno o malo? Sin referencia no se
// sabe si quedaba mucho margen o casi ninguno.
//
// OPT reemplaza la pagina que tardara mas en volver a usarse. Requiere
// conocer el futuro, de modo que NO ES IMPLEMENTABLE en un sistema real: su
// unico valor es teorico. Belady demostro que es optimo, es decir que ningun
// algoritmo puede producir menos fallos sobre la misma cadena. Sirve entonces
// como COTA INFERIOR contra la cual medir cuanto margen de mejora le queda a
// FIFO y a LRU.
//
// ESTRUCTURA DE DATOS
//   ocurrencias_ pagina -> cola con todos los indices en que aparece
//
// En cada referencia se descartan del frente los indices ya pasados, con lo
// que el frente queda siendo siempre "la proxima vez que se usara esta
// pagina". Cada indice entra y sale una sola vez en toda la simulacion, de
// modo que el precalculo es O(n) y el mantenimiento amortizado O(1).
//
// COSTO: O(1) amortizado por acierto; O(marcos) por fallo, al buscar cual de
// las paginas residentes se usara mas tarde.
// ===========================================================================
class Optimo {
public:
    Optimo(int marcos, const std::vector<int>& cadena)
        : marcos_(static_cast<size_t>(marcos), MARCO_LIBRE), libres_(marcos) {
        cadena_ = &cadena;
        residente_.reserve(static_cast<size_t>(marcos) * 2);
        for (size_t i = 0; i < cadena.size(); i++)
            ocurrencias_[cadena[i]].push_back(i);
    }

    bool acceder(size_t i, Paso& paso) {
        const int pagina = (*cadena_)[i];
        paso.indice = static_cast<int>(i);
        paso.pagina = pagina;

        // El indice actual ya no cuenta como uso futuro de esta pagina.
        consumirHasta(pagina, i);

        std::unordered_map<int,int>::iterator it = residente_.find(pagina);
        if (it != residente_.end()) {
            paso.fallo      = false;
            paso.marcoUsado = it->second;
            paso.marcos     = marcos_;
            return false;
        }

        int destino;
        if (libres_ > 0) {
            destino = static_cast<int>(marcos_.size()) - libres_;
            libres_--;
        } else {
            // Victima: la pagina residente cuyo proximo uso este mas lejos.
            // Una pagina que ya no vuelve a aparecer se desaloja de inmediato.
            int victima = MARCO_LIBRE;
            size_t masLejano = 0;
            for (std::unordered_map<int,int>::iterator r = residente_.begin();
                 r != residente_.end(); ++r) {
                const size_t proximo = proximoUso(r->first, i);
                if (victima == MARCO_LIBRE || proximo > masLejano) {
                    masLejano = proximo;
                    victima   = r->first;
                }
            }
            destino = residente_[victima];
            residente_.erase(victima);
            paso.victima = victima;
        }

        marcos_[static_cast<size_t>(destino)] = pagina;
        residente_[pagina] = destino;

        paso.fallo      = true;
        paso.marcoUsado = destino;
        paso.marcos     = marcos_;
        return true;
    }

private:
    // Descarta los indices ya consumidos, dejando al frente el proximo uso.
    void consumirHasta(int pagina, size_t i) {
        std::deque<size_t>& q = ocurrencias_[pagina];
        while (!q.empty() && q.front() <= i) q.pop_front();
    }

    // Proximo indice en que se usara la pagina despues de i, o "infinito" si
    // no vuelve a aparecer nunca.
    size_t proximoUso(int pagina, size_t i) {
        consumirHasta(pagina, i);
        const std::deque<size_t>& q = ocurrencias_[pagina];
        return q.empty() ? std::numeric_limits<size_t>::max() : q.front();
    }

    std::vector<int>                            marcos_;
    std::unordered_map<int,int>                 residente_;
    std::unordered_map<int, std::deque<size_t> > ocurrencias_;
    int                                         libres_;
    const std::vector<int>*                     cadena_;
};

#endif

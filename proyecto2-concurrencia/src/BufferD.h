#ifndef BUFFER_D_H
#define BUFFER_D_H

#include <atomic>
#include <vector>
#include "Item.h"
#include "Semaforo.h"

// ===========================================================================
// VERSION D - SEMAFOROS (solucion clasica de Dijkstra)
//
// Tres semaforos, que es como se resuelve Productor-Consumidor en la
// literatura desde los anos 60:
//
//   vacios_    cuenta cuantos ESPACIOS LIBRES quedan   (inicia en capacidad)
//   llenos_    cuenta cuantos ITEMS DISPONIBLES hay    (inicia en 0)
//   exclusion_ semaforo BINARIO usado como cerrojo     (inicia en 1)
//
// Diferencia conceptual con las versiones B y C: alli el mutex protegia la
// seccion critica y la CONDICION (lleno / vacio) se evaluaba a mano con un
// if o un predicado. Aca la condicion ya no se evalua: esta CODIFICADA en el
// contador del semaforo. Si no hay espacio, vacios_ vale 0 y el productor
// simplemente no pasa.
// ===========================================================================
class BufferD {
public:
    explicit BufferD(int capacidad)
        : capacidad_(capacidad),
          datos_(static_cast<size_t>(capacidad), itemVacio()),
          cabeza_(0),
          cola_(0),
          cuenta_(0),
          vacios_(capacidad),
          llenos_(0),
          exclusion_(1) {}

    bool producir(const Item& it, Reloj::time_point limite) {
        // ORDEN DE ADQUISICION: primero el semaforo de recurso, despues el
        // de exclusion mutua. Si se invirtiera, un productor podria quedarse
        // con el cerrojo esperando espacio que solo un consumidor puede
        // liberar, y ese consumidor no podria entrar porque el cerrojo esta
        // tomado: interbloqueo con las cuatro condiciones de Coffman.
        // Ordenar la adquisicion es la tecnica de PREVENCION de la lamina 8.
        if (!vacios_.esperar(limite)) return false;

        if (!exclusion_.esperar(limite)) {
            // Si se corta aca hay que DEVOLVER el permiso ya tomado. Sin
            // esto se filtra un espacio en cada corte y el buffer se
            // "encoge" hasta trabarse. Es el error clasico al programar con
            // semaforos.
            vacios_.senalar();
            return false;
        }

        // #######################################################
        // ###  SECCION CRITICA  -  protegida por exclusion_   ###
        // #######################################################
        datos_[cabeza_] = it;
        cabeza_ = (cabeza_ + 1) % capacidad_;
        cuenta_++;
        // #######################################################

        exclusion_.senalar();
        llenos_.senalar();   // avisa a los consumidores que hay un item mas
        return true;
    }

    bool consumir(Item& destino, Reloj::time_point limite) {
        if (!llenos_.esperar(limite)) return false;

        if (!exclusion_.esperar(limite)) {
            llenos_.senalar();
            return false;
        }

        // #######################################################
        // ###  SECCION CRITICA  -  protegida por exclusion_   ###
        // #######################################################
        destino = datos_[cola_];
        cola_ = (cola_ + 1) % capacidad_;
        cuenta_--;
        // #######################################################

        exclusion_.senalar();
        vacios_.senalar();   // avisa a los productores que hay un hueco mas
        return true;
    }

    // Solo observacional. A diferencia de las versiones A, B y C, aca
    // cuenta_ NO se usa para decidir nada: el control de lleno/vacio lo
    // hacen los contadores de los semaforos. Se mantiene atomico unicamente
    // para poder reportarlo sin introducir una carrera.
    int cuentaActual() const { return cuenta_.load(); }

    int capacidad() const { return capacidad_; }

private:
    int               capacidad_;
    std::vector<Item> datos_;
    int               cabeza_;
    int               cola_;
    std::atomic<int>  cuenta_;

    Semaforo vacios_;      // espacios libres
    Semaforo llenos_;      // items disponibles
    Semaforo exclusion_;   // binario: hace de cerrojo
};

#endif

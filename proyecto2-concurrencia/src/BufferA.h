#ifndef BUFFER_A_H
#define BUFFER_A_H

#include <vector>
#include "Item.h"   // define Item y el tipo Reloj

// ===========================================================================
// VERSION A - SIN SINCRONIZACION
//
// Esta clase es "deliberada y honestamente vulnerable" (requisito no
// funcional del enunciado). La corrupcion sale del codigo desprotegido, no
// de trucos.
//
// REGLAS DE HONESTIDAD que se respetan en este archivo:
//   1. No hay std::mutex, std::atomic, volatile ni ninguna barrera.
//   2. No hay sleep(), yield() ni esperas artificiales para forzar el fallo.
//   3. No se altero el orden de las operaciones para provocar la carrera.
//
// Consecuencia: si se quitan los productores/consumidores extra y queda un
// solo hilo de cada tipo, este mismo codigo produce resultados perfectos.
// El fallo lo causa la concurrencia, no el codigo.
// ===========================================================================
class BufferA {
public:
    explicit BufferA(int capacidad)
        : capacidad_(capacidad),
          datos_(static_cast<size_t>(capacidad), itemVacio()),
          cabeza_(0),
          cola_(0),
          cuenta_(0) {}

    // -----------------------------------------------------------------------
    // PRODUCIR
    // -----------------------------------------------------------------------
    bool producir(const Item& it, Reloj::time_point limite) {
        // ESPERA ACTIVA ("busy wait"): el hilo gira consultando la condicion
        // en vez de dormirse. Es el analogo en espacio de usuario del
        // spinlock de xv6, y es la misma estrategia de espera que usara la
        // version B: por eso B contra A mide el costo del mutex y nada mas.
        //
        // El chequeo del tiempo va DENTRO del bucle por dos razones:
        //   (a) Termina la corrida si cuenta_ quedo corrupta y este hilo
        //       esperaria para siempre.
        //   (b) Reloj::now() es una funcion externa que el compilador no
        //       puede analizar, asi que lo obliga a releer cuenta_ en cada
        //       vuelta. Sin ella, con -O2 el compilador tiene permitido
        //       asumir que cuenta_ no cambia (un data race es comportamiento
        //       indefinido) y convertir esto en un while(true) literal.
        while (cuenta_ == capacidad_) {
            if (Reloj::now() > limite) return false;
        }

        // ###################################################################
        // ###  SECCION CRITICA  -  DESPROTEGIDA A PROPOSITO               ###
        // ###################################################################
        //
        // Las tres lineas de abajo leen y escriben estado compartido
        // (datos_, cabeza_, cuenta_) sin ninguna exclusion mutua. Dos
        // productores pueden ejecutarlas entrelazadas y producir:
        //
        //   - Items PERDIDOS: dos productores leen el mismo cabeza_, los dos
        //     escriben en la misma casilla, y el segundo pisa al primero.
        //
        //   - Items CORRUPTOS: la asignacion de un Item son 32 bytes, no una
        //     escritura atomica. Un consumidor puede leer la casilla cuando
        //     solo se escribio una parte, y quedarse con campos de dos items
        //     distintos. La suma de control lo detecta.
        //
        //   - VIOLACION DE CAPACIDAD: entre el while de arriba y esta linea,
        //     otro productor pudo llenar el buffer. Ya pasamos el chequeo,
        //     asi que escribimos igual. Ese hueco entre "verificar" y "actuar"
        //     es el corazon de toda condicion de carrera.
        datos_[cabeza_] = it;
        cabeza_ = (cabeza_ + 1) % capacidad_;

        // Escrito expandido a proposito para que se vean los TRES pasos que
        // el procesador ejecuta: leer cuenta_, sumarle 1, escribir el
        // resultado. Dos hilos pueden leer el mismo valor, sumar sobre el
        // mismo valor y escribir el mismo resultado: se pierde un incremento.
        // Es exactamente el caso de la lamina 3 de la Unidad III.
        cuenta_ = cuenta_ + 1;
        // ###################################################################
        // ###  FIN SECCION CRITICA                                        ###
        // ###################################################################

        return true;
    }

    // -----------------------------------------------------------------------
    // CONSUMIR
    // -----------------------------------------------------------------------
    bool consumir(Item& destino, Reloj::time_point limite) {
        // Misma espera activa, condicion inversa: gira mientras este vacio.
        while (cuenta_ == 0) {
            if (Reloj::now() > limite) return false;
        }

        // ###################################################################
        // ###  SECCION CRITICA  -  DESPROTEGIDA A PROPOSITO               ###
        // ###################################################################
        //
        //   - Items DUPLICADOS: dos consumidores leen el mismo cola_ y se
        //     llevan los dos la misma casilla. El item se consume dos veces.
        //
        //   - Items FANTASMA: se paso el chequeo de "no esta vacio", pero
        //     otro consumidor vacio el buffer antes de llegar aca. Se lee una
        //     casilla que nadie escribio todavia, o que ya se consumio.
        //
        //   - cuenta_ puede quedar NEGATIVA por la misma razon que puede
        //     superar la capacidad en producir().
        destino = datos_[cola_];
        cola_ = (cola_ + 1) % capacidad_;
        cuenta_ = cuenta_ - 1;
        // ###################################################################
        // ###  FIN SECCION CRITICA                                        ###
        // ###################################################################

        return true;
    }

    // Lectura observacional de cuenta_, sin proteger a proposito: sirve para
    // registrar si el buffer llego a superar su capacidad, que es uno de los
    // sintomas que el enunciado pide demostrar.
    int cuentaActual() const { return cuenta_; }

    int capacidad() const { return capacidad_; }

private:
    int               capacidad_;
    std::vector<Item> datos_;
    int               cabeza_;   // donde escribe el productor
    int               cola_;     // donde lee el consumidor
    int               cuenta_;   // <-- LA VARIABLE MAS FRAGIL
};

#endif

#ifndef BUFFER_A_H
#define BUFFER_A_H

#include <atomic>
#include <thread>
#include <vector>
#include "Item.h"

// ===========================================================================
// VERSION A - SIN SINCRONIZACION
//
// El acceso al buffer y a sus variables compartidas es directo y desprotegido.
// La vulnerabilidad es genuina (RNF-1): surge del acceso real sin exclusion
// mutua, no de ningun artificio.
//
// LO QUE NO HAY EN ESTE ARCHIVO, Y ES DELIBERADO:
//   - Ningun std::mutex, ningun lock, ninguna barrera de memoria.
//   - Ningun sleep(), yield() ni retardo para "ayudar" a que falle (RNF-2).
//   - Ninguna alteracion de contadores ni simulacion de perdidas.
//
// Si se ejecuta con un solo productor y un solo consumidor, este mismo codigo
// produce resultados perfectos: el fallo lo causa la concurrencia, no el codigo.
//
// ---------------------------------------------------------------------------
// SOBRE volatile
//
// Las variables de estado se declaran volatile. Conviene ser preciso sobre lo
// que eso significa y lo que no:
//
//   volatile NO aporta atomicidad ni ordenamiento de memoria. NO es un
//   mecanismo de sincronizacion. La carrera permanece intacta: cuenta_ sigue
//   siendo un contador leido-modificado-escrito en tres pasos, y la asignacion
//   de un Item sigue sin ser atomica.
//
//   Su unico efecto es obligar al compilador a releer la variable en cada
//   iteracion. Sin volatile, como una carrera de datos es comportamiento
//   indefinido, el optimizador esta autorizado a suponer que cuenta_ no cambia
//   y a sacar la lectura fuera del bucle de espera, convirtiendolo en un ciclo
//   infinito. Se comprobo que eso ocurre con -O2.
//
// Es decir: volatile es lo que permite que el programa TERMINE, no lo que
// permite que falle. La version B no lo necesita porque el mutex ya impone el
// ordenamiento.
// ===========================================================================
class BufferA {
public:
    explicit BufferA(int capacidad)
        : capacidad_(capacidad),
          datos_(static_cast<size_t>(capacidad), itemVacio()),
          cabeza_(0), cola_(0), cuenta_(0), fin_(false) {}

    void producir(const Item& it, Traza& tz) {
        // Espera hasta que haya lugar. Es espera activa, propia de esta
        // version: sin primitivas de sincronizacion no hay forma de dormirse.
        //
        // Sobre el yield(): ver la nota al final del archivo. No es un retardo
        // ni sincronizacion; solo cede el turno cuando este hilo no puede
        // avanzar. La carrera de la seccion critica permanece intacta.
        while (cuenta_ >= capacidad_) {
            std::this_thread::yield();
        }

        // === INICIO SECCIÓN CRÍTICA ===
        //
        // Estas cuatro lineas leen y escriben estado compartido sin ninguna
        // exclusion mutua. Dos productores pueden ejecutarlas entrelazadas:
        //
        //   - Items PERDIDOS: ambos leen el mismo cabeza_, ambos escriben en
        //     la misma posicion, y el segundo pisa al primero.
        //   - Items CORRUPTOS: asignar un Item son 40 bytes, no una escritura
        //     atomica; un consumidor puede leer la posicion a medio escribir.
        //   - OCUPACION > N: entre el while de arriba y esta linea, otro
        //     productor pudo llenar el buffer. Ya pasamos el chequeo, asi que
        //     escribimos igual. Ese hueco entre verificar y actuar es el
        //     corazon de toda condicion de carrera.
        const int pos = cabeza_ % capacidad_;
        tz.cuenta_antes = cuenta_;
        tz.posicion     = pos;

        datos_[static_cast<size_t>(pos)] = it;
        cabeza_ = (cabeza_ + 1) % capacidad_;

        // Escrito expandido para que se vean los TRES pasos que ejecuta el
        // procesador: leer cuenta_, sumarle uno, escribir el resultado. Dos
        // hilos pueden leer el mismo valor y escribir el mismo resultado, con
        // lo que se pierde un incremento.
        cuenta_ = cuenta_ + 1;

        tz.cuenta_despues = cuenta_;
        // === FIN SECCIÓN CRÍTICA ===
    }

    // Devuelve false cuando no queda nada por consumir y la produccion ya
    // termino. No hay temporizadores (RF-7).
    bool consumir(Item& destino, Traza& tz) {
        for (;;) {
            if (cuenta_ > 0) {
                // === INICIO SECCIÓN CRÍTICA ===
                //
                //   - Items DUPLICADOS: dos consumidores leen el mismo cola_ y
                //     se llevan los dos la misma posicion.
                //   - Items FANTASMA: se paso el chequeo de "no esta vacio",
                //     pero otro consumidor vacio el buffer antes de llegar
                //     aca; se lee una posicion nunca escrita o ya consumida.
                //   - cuenta_ puede quedar NEGATIVA por la misma razon por la
                //     que puede superar N en producir().
                const int pos = cola_ % capacidad_;
                tz.cuenta_antes = cuenta_;
                tz.posicion     = pos;

                destino = datos_[static_cast<size_t>(pos)];
                cola_ = (cola_ + 1) % capacidad_;
                cuenta_ = cuenta_ - 1;

                tz.cuenta_despues = cuenta_;
                // === FIN SECCIÓN CRÍTICA ===
                return true;
            }

            // Buffer vacio. Si la produccion ya termino, no vendra nada mas.
            //
            // Esta condicion es la que garantiza la terminacion sin recurrir a
            // ningun temporizador. Despues de que el hilo principal activa la
            // bandera, cuenta_ solo puede decrecer (ya nadie produce), de modo
            // que el bucle converge aunque el contador este corrompido.
            if (fin_.load(std::memory_order_relaxed)) return false;

            std::this_thread::yield();
        }
    }

    void marcarFinProduccion() { fin_.store(true, std::memory_order_relaxed); }

    int capacidad() const { return capacidad_; }

private:
    int                        capacidad_;
    std::vector<Item>          datos_;
    volatile int               cabeza_;   // donde escribe el productor
    volatile int               cola_;     // donde lee el consumidor
    volatile int               cuenta_;   // <-- LA VARIABLE MAS FRAGIL
    std::atomic<bool>          fin_;      // solo terminacion, no protege nada
};

// ===========================================================================
// NOTA SOBRE std::this_thread::yield() EN LOS BUCLES DE ESPERA
//
// QUE ES. Una indicacion al planificador de que este hilo no puede avanzar y
// conviene darle el procesador a otro. No es un retardo (no fija ninguna
// duracion), no es sincronizacion (no ordena accesos a memoria ni excluye a
// nadie de la seccion critica) y no altera en nada las tres lineas
// desprotegidas que producen la corrupcion.
//
// POR QUE ESTA. Sin el, con 8 hilos girando sobre 6 procesadores logicos, el
// planificador entra en un patron patologico: los hilos que no pueden avanzar
// consumen el procesador que necesitan justamente los que si podrian. Medido
// en la configuracion N=50 con 4 productores y 4 consumidores, sobre 12
// corridas:
//
//   sin yield():  mediana 10 ms, con dos corridas de 21 s y 371 s
//   con yield():  12 corridas entre 4.1 ms y 9.1 ms, sin cola
//
// POR QUE NO DEBILITA LA EVIDENCIA. En esas mismas 12 corridas con yield() el
// invariante se violo 12 de 12 veces, con magnitudes de corrupcion equivalentes
// a las obtenidas sin el (items consumidos entre 17 344 y 27 975 frente a los
// 20 000 producidos). El yield() actua en el bucle de ESPERA, no en la seccion
// critica: no reduce la probabilidad de la carrera, solo evita que el programa
// tarde minutos en terminar.
//
// POR QUE NO ESTA EN LA VERSION B. La version B no tiene bucles de espera: sus
// hilos se bloquean en la variable de condicion y el planificador ya sabe que
// no son ejecutables. No hay donde ponerlo.
// ===========================================================================

#endif

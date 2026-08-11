#ifndef ITEM_H
#define ITEM_H

#include <chrono>

// Reloj monotono (RF-8). No salta si se cambia la hora del sistema durante la
// corrida, a diferencia de system_clock: es el correcto para medir duraciones.
typedef std::chrono::steady_clock Reloj;

// ---------------------------------------------------------------------------
// Item que viaja por el buffer compartido.
//
// Lleva CINCO campos a proposito, y cada uno cumple una funcion distinta en la
// verificacion de invariantes (RF-4):
//
//   valor         entero que se suma para la suma de control obligatoria.
//                 Si se pierde o se duplica un item, la suma producida y la
//                 consumida dejan de coincidir.
//
//   id_unico      identidad global del item. Permite ademas saber CUALES se
//                 perdieron y cuales se duplicaron, cosa que la suma sola no
//                 distingue (dos errores opuestos podrian cancelarse).
//
//   verificacion  suma derivada de los otros campos. Detecta escrituras
//                 entrelazadas campo por campo: un item cuyos campos provienen
//                 de dos escrituras distintas no la satisface.
//
// Se usan varios campos y no un solo entero porque en x86-64 una escritura
// alineada de 8 bytes es atomica a nivel de instruccion: con un unico entero
// nunca se observaria un valor a medio escribir y la corrupcion se limitaria a
// perdidas y sobrescrituras.
// ---------------------------------------------------------------------------
struct Item {
    int  productor_id;
    long secuencia;
    long id_unico;
    long valor;
    long verificacion;
};

// Constante no nula: asi una posicion del buffer nunca escrita (todos sus
// campos en cero) NO supera la verificacion y es distinguible de un item real.
const long MARCA_VERIFICACION = 0x5A5A5A5AL;

inline long calcularId(int productor_id, long secuencia) {
    return static_cast<long>(productor_id) * 1000000L + secuencia;
}

inline long calcularVerificacion(int productor_id, long secuencia,
                                 long id_unico, long valor) {
    return MARCA_VERIFICACION
         ^ (static_cast<long>(productor_id) * 31L)
         ^ (secuencia * 131L)
         ^ (id_unico * 17L)
         ^ (valor * 7L);
}

// Los identificadores de productor empiezan en 1 (no en 0) para que ningun
// id_unico legitimo valga 0 y no se confunda con una posicion sin escribir.
inline Item crearItem(int productor_id, long secuencia) {
    Item it;
    it.productor_id = productor_id;
    it.secuencia    = secuencia;
    it.id_unico     = calcularId(productor_id, secuencia);
    it.valor        = secuencia + 1;          // valor para la suma de control
    it.verificacion = calcularVerificacion(productor_id, secuencia,
                                           it.id_unico, it.valor);
    return it;
}

inline Item itemVacio() {
    Item it;
    it.productor_id = 0;
    it.secuencia    = 0;
    it.id_unico     = 0;
    it.valor        = 0;
    it.verificacion = 0;
    return it;
}

inline bool itemIntegro(const Item& it) {
    return it.verificacion == calcularVerificacion(it.productor_id, it.secuencia,
                                                   it.id_unico, it.valor);
}

// Datos que el buffer reporta de cada operacion, para el log de eventos (RF-9).
// Se rellena siempre; el llamador decide si lo guarda o lo descarta.
struct Traza {
    int posicion      = -1;   // posicion del arreglo que se toco
    int cuenta_antes  = 0;    // ocupacion observada antes de la operacion
    int cuenta_despues= 0;    // ocupacion observada despues
};

#endif

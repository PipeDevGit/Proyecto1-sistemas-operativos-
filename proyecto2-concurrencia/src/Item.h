#ifndef ITEM_H
#define ITEM_H

#include <chrono>

// Reloj monotono: no salta si alguien cambia la hora del sistema durante la
// corrida, a diferencia de system_clock. Es el correcto para medir duraciones.
typedef std::chrono::steady_clock Reloj;

// ---------------------------------------------------------------------------
// Item que circula por el buffer compartido.
//
// Lleva CUATRO campos a proposito. Si fuera un solo entero de 64 bits, en
// x86-64 la escritura alineada de 8 bytes es atomica por hardware: nunca se
// veria un valor "a medias", solo items perdidos o sobrescritos. Con cuatro
// campos, dos productores pueden entrelazar sus escrituras campo por campo y
// dejar en el buffer un item que NUNCA produjo nadie.
// ---------------------------------------------------------------------------
struct Item {
    int  productor_id;   // que hilo lo produjo (empieza en 1, nunca 0)
    long secuencia;      // n-esimo item de ESE productor
    long id_unico;       // productor_id * 1000000 + secuencia
    long verificacion;   // suma de control derivada de los otros tres campos
};

// Los ids de productor empiezan en 1, asi que id_unico nunca vale 0.
// Eso permite distinguir una casilla del buffer que jamas se escribio.
inline long calcularId(int productor_id, long secuencia) {
    return static_cast<long>(productor_id) * 1000000L + secuencia;
}

// La constante hace que un item en ceros (casilla nunca escrita) NO pase la
// verificacion: sin ella, calcularVerificacion(0,0,0) daria 0 y un hueco
// vacio se veria como un item legitimo.
inline long calcularVerificacion(int productor_id, long secuencia, long id_unico) {
    return 0x5A5A5A5AL
         ^ (static_cast<long>(productor_id) * 31L)
         ^ (secuencia * 131L)
         ^ (id_unico * 17L);
}

inline Item crearItem(int productor_id, long secuencia) {
    Item it;
    it.productor_id = productor_id;
    it.secuencia    = secuencia;
    it.id_unico     = calcularId(productor_id, secuencia);
    it.verificacion = calcularVerificacion(productor_id, secuencia, it.id_unico);
    return it;
}

// Si esto da false, los cuatro campos no provienen de una sola escritura:
// es una combinacion fabricada por el entrelazado de dos productores.
inline bool itemIntegro(const Item& it) {
    return it.verificacion ==
           calcularVerificacion(it.productor_id, it.secuencia, it.id_unico);
}

inline Item itemVacio() {
    Item it;
    it.productor_id = 0;
    it.secuencia    = 0;
    it.id_unico     = 0;
    it.verificacion = 0;
    return it;
}

#endif

#include "Registro.h"
#include <unordered_map>
#include <unordered_set>

// Reconciliacion post-join, en un solo hilo. Aca ya no hay concurrencia:
// todo lo que se lee son vectores privados de hilos que ya terminaron.
Metricas reconciliar(const std::vector<RegistroProductor>& productores,
                     const std::vector<RegistroConsumidor>& consumidores,
                     int capacidad) {
    Metricas m;

    std::unordered_set<long> entregados;
    for (const RegistroProductor& p : productores) {
        m.producidos += static_cast<long>(p.ids.size());
        m.timeouts   += p.timeouts;
        m.drenados   += p.drenados;
        if (p.max_cuenta > m.max_cuenta) m.max_cuenta = p.max_cuenta;
        for (long id : p.ids) entregados.insert(id);
    }

    // Cuantas veces se consumio cada id. Los items con suma de control
    // invalida se cuentan aparte y NO entran a este conteo: su id_unico es
    // basura, asi que mezclarlos inflaria artificialmente "fantasma".
    std::unordered_map<long, long> conteo;
    for (const RegistroConsumidor& c : consumidores) {
        m.consumidos += static_cast<long>(c.items.size());
        m.timeouts   += c.timeouts;
        m.drenados   += c.drenados;
        if (c.max_cuenta > m.max_cuenta) m.max_cuenta = c.max_cuenta;
        for (const Item& it : c.items) {
            if (!itemIntegro(it)) {
                m.corruptos++;
                continue;
            }
            conteo[it.id_unico]++;
        }
    }

    for (const std::pair<const long, long>& par : conteo) {
        if (entregados.find(par.first) == entregados.end()) {
            m.fantasma += par.second;          // nadie lo produjo
        } else if (par.second > 1) {
            m.duplicados += par.second - 1;    // se consumio de mas
        }
    }

    for (long id : entregados) {
        if (conteo.find(id) == conteo.end()) m.perdidos++;
    }

    // Items validos que llegaron a un consumidor exactamente una vez.
    // Identidad que debe cumplirse siempre:
    //   entregados_ok == producidos - perdidos
    //                 == consumidos - duplicados - fantasma - corruptos
    m.entregados_ok = m.producidos - m.perdidos;

    m.violo_capacidad = m.max_cuenta > capacidad;
    return m;
}

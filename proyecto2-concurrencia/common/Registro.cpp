#include "Registro.h"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <unordered_map>
#include <unordered_set>

Metricas reconciliar(const std::vector<RegistroProductor>& productores,
                     const std::vector<RegistroConsumidor>& consumidores,
                     int capacidad) {
    Metricas m;
    m.capacidad = capacidad;

    std::unordered_set<long> entregados;
    for (size_t i = 0; i < productores.size(); i++) {
        const RegistroProductor& p = productores[i];
        m.producidos     += static_cast<long>(p.ids.size());
        m.suma_producida += p.suma;
        if (p.max_cuenta > m.max_cuenta) m.max_cuenta = p.max_cuenta;
        for (size_t k = 0; k < p.ids.size(); k++) entregados.insert(p.ids[k]);
    }

    // Cuantas veces se consumio cada id. Los items cuya verificacion falla se
    // cuentan aparte y NO entran en este conteo: su id_unico es basura, de modo
    // que mezclarlos inflaria artificialmente la cifra de "fantasma".
    std::unordered_map<long, long> conteo;
    for (size_t i = 0; i < consumidores.size(); i++) {
        const RegistroConsumidor& c = consumidores[i];
        m.consumidos     += static_cast<long>(c.items.size());
        m.suma_consumida += c.suma;
        if (c.max_cuenta > m.max_cuenta) m.max_cuenta = c.max_cuenta;
        for (size_t k = 0; k < c.items.size(); k++) {
            if (!itemIntegro(c.items[k])) { m.corruptos++; continue; }
            conteo[c.items[k].id_unico]++;
        }
    }

    for (std::unordered_map<long, long>::iterator it = conteo.begin();
         it != conteo.end(); ++it) {
        if (entregados.find(it->first) == entregados.end()) {
            m.fantasma += it->second;          // nadie lo produjo
        } else if (it->second > 1) {
            m.duplicados += it->second - 1;    // se consumio de mas
        }
    }

    for (std::unordered_set<long>::iterator it = entregados.begin();
         it != entregados.end(); ++it) {
        if (conteo.find(*it) == conteo.end()) m.perdidos++;
    }

    // Identidad de control que debe cumplirse en toda corrida:
    //   entregados_ok == producidos - perdidos
    //                 == consumidos - duplicados - fantasma - corruptos
    m.entregados_ok = m.producidos - m.perdidos;
    return m;
}

bool volcarLog(const std::string& ruta,
               const std::vector<RegistroProductor>& productores,
               const std::vector<RegistroConsumidor>& consumidores,
               Reloj::time_point origen) {
    std::vector<Evento> todos;
    for (size_t i = 0; i < productores.size(); i++)
        todos.insert(todos.end(), productores[i].eventos.begin(),
                                  productores[i].eventos.end());
    for (size_t i = 0; i < consumidores.size(); i++)
        todos.insert(todos.end(), consumidores[i].eventos.begin(),
                                  consumidores[i].eventos.end());

    // Los eventos se recogen por hilo y se ordenan aca, ya en un solo hilo.
    // El orden por marca de tiempo es aproximado: dos eventos separados por
    // nanosegundos pueden aparecer invertidos respecto del orden real. Es
    // suficiente para reconstruir el intercalado, que es lo que pide RF-9.
    std::sort(todos.begin(), todos.end(),
              [](const Evento& a, const Evento& b) { return a.instante < b.instante; });

    std::ofstream f(ruta.c_str());
    if (!f.is_open()) return false;

    f << "# Log de eventos. Tiempo relativo al inicio de la corrida.\n";
    f << "# tipo: P=produccion C=consumo | pos=posicion del buffer\n";
    f << "# cuenta: ocupacion observada antes -> despues de la operacion\n";
    f << "#\n";
    f << "#      ms  hilo  tipo        id  valor  pos   cuenta  integro\n";

    char linea[256];
    for (size_t i = 0; i < todos.size(); i++) {
        const Evento& e = todos[i];
        const double ms = std::chrono::duration<double, std::milli>(e.instante - origen).count();
        snprintf(linea, sizeof(linea),
                 "%9.4f   %c%-2d   %s  %8ld  %5ld  %3d   %3d->%-3d  %s",
                 ms, e.tipo, e.hilo,
                 e.tipo == 'P' ? "PROD" : "CONS",
                 e.id_unico, e.valor, e.posicion,
                 e.cuenta_antes, e.cuenta_despues,
                 e.tipo == 'C' ? (e.integro ? "si" : "NO") : "-");
        f << linea << "\n";
    }
    return true;
}

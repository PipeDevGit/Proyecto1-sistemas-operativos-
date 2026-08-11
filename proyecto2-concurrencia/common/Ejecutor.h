#ifndef EJECUTOR_H
#define EJECUTOR_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "Item.h"
#include "Registro.h"

// ---------------------------------------------------------------------------
// Configuracion comun a las dos versiones (RF-1, RF-2, RF-3).
// Ambas versiones aceptan exactamente los mismos parametros, de modo que la
// comparacion sea valida.
// ---------------------------------------------------------------------------
struct Config {
    int         capacidad          = 10;   // N
    int         productores        = 2;
    int         consumidores       = 2;
    long        items_por_productor= 5000;
    bool        color              = true;
    std::string csv;
    std::string log;

    long totalEsperado() const {
        return static_cast<long>(productores) * items_por_productor;
    }
};

inline void imprimirAyuda(const char* prog, const char* version) {
    printf("Simulador Productor-Consumidor - VERSION %s\n\n", version);
    printf("Uso: %s [opciones]\n\n", prog);
    printf("  --capacidad N     tamano del buffer (10)\n");
    printf("  --productores N   hilos productores, minimo 2 (2)\n");
    printf("  --consumidores N  hilos consumidores, minimo 2 (2)\n");
    printf("  --items N         items por productor (5000)\n");
    printf("  --csv archivo     anexa una fila con los resultados\n");
    printf("  --log archivo     vuelca el log de eventos (RF-9)\n");
    printf("  --sin-color       desactiva los codigos ANSI\n");
    printf("  --ayuda           muestra esta ayuda\n");
}

inline bool parsearArgumentos(int argc, char** argv, Config& cfg, const char* version) {
    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        const bool hay = (i + 1 < argc);
        if      (a == "--capacidad"    && hay) cfg.capacidad           = std::atoi(argv[++i]);
        else if (a == "--productores"  && hay) cfg.productores         = std::atoi(argv[++i]);
        else if (a == "--consumidores" && hay) cfg.consumidores        = std::atoi(argv[++i]);
        else if (a == "--items"        && hay) cfg.items_por_productor = std::atol(argv[++i]);
        else if (a == "--csv"          && hay) cfg.csv                 = argv[++i];
        else if (a == "--log"          && hay) cfg.log                 = argv[++i];
        else if (a == "--sin-color")           cfg.color               = false;
        else if (a == "--ayuda" || a == "-h") { imprimirAyuda(argv[0], version); return false; }
        else {
            std::cerr << "Opcion no reconocida: " << a << "\n";
            imprimirAyuda(argv[0], version);
            return false;
        }
    }
    if (cfg.productores < 2 || cfg.consumidores < 2) {
        std::cerr << "Error: el enunciado exige al menos 2 productores y 2 consumidores (RF-2).\n";
        return false;
    }
    if (cfg.capacidad < 1 || cfg.items_por_productor < 1) {
        std::cerr << "Error: capacidad e items deben ser positivos.\n";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Ejecuta una corrida completa.
//
// Es template y no herencia con metodos virtuales a proposito: una llamada
// virtual en el camino caliente agregaria una indireccion que se sumaria al
// tiempo medido, contaminando justamente la magnitud que se quiere comparar.
//
// TERMINACION (RF-7): no hay temporizadores de ninguna clase.
//
//   Productores: cada uno produce una cantidad fija conocida de antemano
//   (contador de items restantes). Terminan solos.
//
//   Consumidores: consumen hasta que consumir() devuelve false, lo que ocurre
//   cuando el buffer esta vacio Y la produccion ya termino. El hilo principal
//   activa la bandera de fin despues de unir a todos los productores, de modo
//   que la condicion no puede activarse antes de tiempo.
//
// La bandera de fin es atomica en AMBAS versiones. No protege el buffer ni
// ninguna de sus variables, y por tanto no constituye sincronizacion del
// recurso compartido: su unico proposito es la terminacion. Sin ella, la
// lectura de un bool corriente dentro del bucle de espera seria comportamiento
// indefinido y el compilador podria eliminarla, dejando un ciclo infinito.
// ---------------------------------------------------------------------------
template <typename TBuffer>
Metricas ejecutar(const Config& cfg, bool conLog) {
    TBuffer buffer(cfg.capacidad);

    std::vector<RegistroProductor>  rp(static_cast<size_t>(cfg.productores));
    std::vector<RegistroConsumidor> rc(static_cast<size_t>(cfg.consumidores));

    // Reservar antes de medir: si un vector creciera durante la seccion
    // cronometrada, la reasignacion de memoria se sumaria al tiempo.
    const long por_consumidor_estimado = cfg.totalEsperado() / cfg.consumidores + 1;
    for (int i = 0; i < cfg.productores; i++) {
        rp[static_cast<size_t>(i)].ids.reserve(static_cast<size_t>(cfg.items_por_productor));
        if (conLog) rp[static_cast<size_t>(i)].eventos.reserve(static_cast<size_t>(cfg.items_por_productor));
    }
    for (int i = 0; i < cfg.consumidores; i++) {
        rc[static_cast<size_t>(i)].items.reserve(static_cast<size_t>(por_consumidor_estimado));
        if (conLog) rc[static_cast<size_t>(i)].eventos.reserve(static_cast<size_t>(por_consumidor_estimado));
    }

    const Reloj::time_point t0 = Reloj::now();

    std::vector<std::thread> productores;
    std::vector<std::thread> consumidores;
    productores.reserve(static_cast<size_t>(cfg.productores));
    consumidores.reserve(static_cast<size_t>(cfg.consumidores));

    for (int p = 0; p < cfg.productores; p++) {
        productores.emplace_back([&, p]() {
            RegistroProductor& reg = rp[static_cast<size_t>(p)];
            for (long s = 0; s < cfg.items_por_productor; s++) {
                Item it = crearItem(p + 1, s);   // ids de productor desde 1
                Traza tz;
                buffer.producir(it, tz);
                reg.ids.push_back(it.id_unico);
                reg.suma += it.valor;
                if (tz.cuenta_despues > reg.max_cuenta) reg.max_cuenta = tz.cuenta_despues;
                if (conLog) {
                    Evento e;
                    e.instante = Reloj::now(); e.tipo = 'P'; e.hilo = p;
                    e.id_unico = it.id_unico;  e.valor = it.valor;
                    e.posicion = tz.posicion;
                    e.cuenta_antes = tz.cuenta_antes; e.cuenta_despues = tz.cuenta_despues;
                    e.integro = true;
                    reg.eventos.push_back(e);
                }
            }
        });
    }

    for (int c = 0; c < cfg.consumidores; c++) {
        consumidores.emplace_back([&, c]() {
            RegistroConsumidor& reg = rc[static_cast<size_t>(c)];
            for (;;) {
                Item it = itemVacio();
                Traza tz;
                if (!buffer.consumir(it, tz)) break;   // vacio y produccion terminada
                reg.items.push_back(it);
                reg.suma += it.valor;
                if (tz.cuenta_antes > reg.max_cuenta) reg.max_cuenta = tz.cuenta_antes;
                if (conLog) {
                    Evento e;
                    e.instante = Reloj::now(); e.tipo = 'C'; e.hilo = c;
                    e.id_unico = it.id_unico;  e.valor = it.valor;
                    e.posicion = tz.posicion;
                    e.cuenta_antes = tz.cuenta_antes; e.cuenta_despues = tz.cuenta_despues;
                    e.integro = itemIntegro(it);
                    reg.eventos.push_back(e);
                }
            }
        });
    }

    for (size_t i = 0; i < productores.size(); i++) productores[i].join();

    // Toda la produccion termino: recien ahora se habilita la salida de los
    // consumidores. Antes de este punto la condicion de fin no puede cumplirse.
    buffer.marcarFinProduccion();

    for (size_t i = 0; i < consumidores.size(); i++) consumidores[i].join();

    const Reloj::time_point t1 = Reloj::now();

    Metricas m = reconciliar(rp, rc, cfg.capacidad);
    m.milisegundos = std::chrono::duration<double, std::milli>(t1 - t0).count();

    if (conLog && !cfg.log.empty()) {
        if (!volcarLog(cfg.log, rp, rc, t0))
            std::cerr << "Aviso: no se pudo escribir el log en " << cfg.log << "\n";
    }
    return m;
}

// ---------------------------------------------------------------------------
inline void imprimirCabecera(const Config& cfg, const char* version, const char* mecanismo) {
    const char* NEG = cfg.color ? "\033[1m"  : "";
    const char* GRIS= cfg.color ? "\033[90m" : "";
    const char* FIN = cfg.color ? "\033[0m"  : "";

    printf("\n%s================================================%s\n", GRIS, FIN);
    printf(" %sVERSION %s%s  -  %s\n", NEG, version, FIN, mecanismo);
    printf("%s================================================%s\n", GRIS, FIN);
    printf("  Buffer (N)                 %8d\n", cfg.capacidad);
    printf("  Productores                %8d\n", cfg.productores);
    printf("  Consumidores               %8d\n", cfg.consumidores);
    printf("  Items por productor        %8ld\n", cfg.items_por_productor);
    // RF-3: el total esperado se conoce e imprime ANTES de arrancar.
    printf("  %sTotal esperado a producir  %8ld%s   (%d x %ld)\n",
           NEG, cfg.totalEsperado(), FIN, cfg.productores, cfg.items_por_productor);
    printf("\n");
    fflush(stdout);
}

inline void imprimirResultado(const Config& cfg, const Metricas& m) {
    const char* ROJO = cfg.color ? "\033[31m" : "";
    const char* VERDE= cfg.color ? "\033[32m" : "";
    const char* GRIS = cfg.color ? "\033[90m" : "";
    const char* NEG  = cfg.color ? "\033[1m"  : "";
    const char* FIN  = cfg.color ? "\033[0m"  : "";

    const bool ok = m.invarianteOk();
    const char* C  = ok ? VERDE : ROJO;

    printf("  %s--- verificacion de invariantes (RF-4) ---%s\n", GRIS, FIN);
    printf("  Producidos                 %8ld\n", m.producidos);
    printf("  Consumidos                 %8ld%s\n", m.consumidos,
           m.producidos == m.consumidos ? "" : "   <-- DIFIERE");
    printf("  Suma producida             %8ld\n", m.suma_producida);
    printf("  Suma consumida             %8ld%s%s%s\n", m.suma_consumida,
           m.sumasCoinciden() ? "" : ROJO,
           m.sumasCoinciden() ? "" : "   <-- DIFIERE", FIN);
    printf("  %sPerdidos                   %8ld%s\n", m.perdidos ? ROJO : GRIS, m.perdidos, FIN);
    printf("  %sDuplicados                 %8ld%s\n", m.duplicados ? ROJO : GRIS, m.duplicados, FIN);
    printf("  %sFantasma                   %8ld%s\n", m.fantasma ? ROJO : GRIS, m.fantasma, FIN);
    printf("  %sCorruptos                  %8ld%s\n", m.corruptos ? ROJO : GRIS, m.corruptos, FIN);
    printf("  Ocupacion maxima           %8d   (N = %d)%s%s%s\n",
           m.max_cuenta, m.capacidad,
           m.excedioCapacidad() ? ROJO : "",
           m.excedioCapacidad() ? "  <-- EXCEDIO N" : "", FIN);
    printf("  %s%sINVARIANTE: %s%s\n", NEG, C, ok ? "OK" : "VIOLADO", FIN);
    printf("  Tiempo total               %8.3f ms\n", m.milisegundos);
    printf("\n");
}

inline void anexarCsv(const Config& cfg, const Metricas& m, const char* version) {
    if (cfg.csv.empty()) return;
    const bool nuevo = !std::ifstream(cfg.csv.c_str()).good();
    std::ofstream f(cfg.csv.c_str(), std::ios::app);
    if (!f.is_open()) {
        std::cerr << "Aviso: no se pudo escribir el CSV en " << cfg.csv << "\n";
        return;
    }
    if (nuevo) {
        f << "version,capacidad,productores,consumidores,items_por_productor,"
             "producidos,consumidos,suma_producida,suma_consumida,perdidos,"
             "duplicados,fantasma,corruptos,entregados_ok,max_cuenta,"
             "excedio_capacidad,invariante_ok,milisegundos\n";
    }
    f << version << ',' << cfg.capacidad << ',' << cfg.productores << ','
      << cfg.consumidores << ',' << cfg.items_por_productor << ','
      << m.producidos << ',' << m.consumidos << ','
      << m.suma_producida << ',' << m.suma_consumida << ','
      << m.perdidos << ',' << m.duplicados << ',' << m.fantasma << ','
      << m.corruptos << ',' << m.entregados_ok << ',' << m.max_cuenta << ','
      << (m.excedioCapacidad() ? 1 : 0) << ',' << (m.invarianteOk() ? 1 : 0) << ','
      << m.milisegundos << '\n';
}

#endif

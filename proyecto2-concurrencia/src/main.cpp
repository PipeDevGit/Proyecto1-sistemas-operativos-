// ---------------------------------------------------------------------------
// Simulador de Concurrencia - Productor/Consumidor
// TIIT2007 Sistemas Operativos - Universidad Invenio
//
// Orquestador: crea los hilos, mide el tiempo, y reconcilia al final.
// La logica de sincronizacion vive en BufferA.h ... BufferD.h.
//
// Nota: toda la salida es ASCII (sin tildes) a proposito, para que se vea
// igual en la consola de Linux y en la consola clasica de Windows.
// ---------------------------------------------------------------------------
#include <chrono>
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
#include "BufferA.h"
#include "BufferB.h"
#include "BufferC.h"
#include "BufferD.h"

struct Config {
    char        variante     = 'A';
    int         productores  = 4;
    int         consumidores = 4;
    int         capacidad    = 8;
    long        items        = 50000;
    int         timeout_seg  = 10;
    int         idle_ms      = 500;
    bool        color        = true;
    std::string csv;
};

// ---------------------------------------------------------------------------
// Ejecuta una corrida completa con el tipo de buffer que se le pase.
//
// Es template y no herencia con metodos virtuales a proposito: una llamada
// virtual en el camino caliente (millones de invocaciones) agregaria una
// indireccion que contaminaria la medicion de tiempos.
// ---------------------------------------------------------------------------
template <typename TBuffer>
Metricas ejecutar(const Config& cfg) {
    TBuffer buffer(cfg.capacidad);

    // Cuotas fijas, decididas ANTES de arrancar. Ningun hilo consulta estado
    // compartido para saber cuando parar: si la terminacion dependiera de
    // cuenta_, con cuenta_ corrupta la version A podria colgarse para siempre.
    const long por_productor  = cfg.items / cfg.productores;
    const long total          = por_productor * cfg.productores;
    const long por_consumidor = total / cfg.consumidores;
    const long resto          = total - por_consumidor * cfg.consumidores;

    std::vector<RegistroProductor>  rp(static_cast<size_t>(cfg.productores));
    std::vector<RegistroConsumidor> rc(static_cast<size_t>(cfg.consumidores));

    // Reservar ANTES de arrancar: si un vector creciera durante la seccion
    // cronometrada, el realloc se sumaria al tiempo medido.
    for (int i = 0; i < cfg.productores; i++)
        rp[static_cast<size_t>(i)].ids.reserve(static_cast<size_t>(por_productor));
    for (int i = 0; i < cfg.consumidores; i++)
        rc[static_cast<size_t>(i)].items.reserve(static_cast<size_t>(por_consumidor + resto));

    // Dos limites distintos, y la diferencia importa:
    //
    //  - 'limite' es el watchdog duro de la corrida entera. Si se alcanza, la
    //    corrida se marca TIMEOUT y no sirve para el informe.
    //
    //  - 'idle' es cuanto tolera un hilo SIN progresar antes de rendirse.
    //    Hace falta porque la version A pierde items: los consumidores nunca
    //    llegan a su cuota y, sin esto, toda corrida de A duraria exactamente
    //    lo que el watchdog, volviendo el tiempo inutil para comparar.
    //    En B/C/D no se pierde nada, asi que este corte nunca se dispara.
    //    El corte NO crea ni evita la corrupcion: solo decide cuando parar de
    //    esperar algo que ya no va a llegar. Se reporta por separado.
    const std::chrono::milliseconds idle(cfg.idle_ms);
    const Reloj::time_point limite = Reloj::now() + std::chrono::seconds(cfg.timeout_seg);
    const Reloj::time_point t0     = Reloj::now();

    std::vector<std::thread> hilos;
    hilos.reserve(static_cast<size_t>(cfg.productores + cfg.consumidores));

    for (int p = 0; p < cfg.productores; p++) {
        hilos.emplace_back([&, p]() {
            RegistroProductor& reg = rp[static_cast<size_t>(p)];
            for (long s = 0; s < por_productor; s++) {
                Item it = crearItem(p + 1, s);   // ids de productor desde 1
                // Este now() ya hacia falta para el corte; se reutiliza como
                // marca de tiempo para no agregar otra llamada al reloj
                // dentro de la seccion cronometrada.
                const Reloj::time_point ahora = Reloj::now();
                Reloj::time_point corte = ahora + idle;
                if (corte > limite) corte = limite;
                if (!buffer.producir(it, corte)) {
                    if (Reloj::now() >= limite) reg.timeouts++; else reg.drenados++;
                    break;   // reg.ultimo conserva la ultima operacion exitosa
                }
                reg.ids.push_back(it.id_unico);
                reg.ultimo = ahora;
                int c = buffer.cuentaActual();
                if (c > reg.max_cuenta) reg.max_cuenta = c;
            }
        });
    }

    for (int c = 0; c < cfg.consumidores; c++) {
        const long cuota = por_consumidor + (c == cfg.consumidores - 1 ? resto : 0);
        hilos.emplace_back([&, c, cuota]() {
            RegistroConsumidor& reg = rc[static_cast<size_t>(c)];
            for (long k = 0; k < cuota; k++) {
                Item it = itemVacio();
                const Reloj::time_point ahora = Reloj::now();
                Reloj::time_point corte = ahora + idle;
                if (corte > limite) corte = limite;
                if (!buffer.consumir(it, corte)) {
                    if (Reloj::now() >= limite) reg.timeouts++; else reg.drenados++;
                    break;
                }
                reg.items.push_back(it);
                reg.ultimo = ahora;
                int n = buffer.cuentaActual();
                if (n > reg.max_cuenta) reg.max_cuenta = n;
            }
        });
    }

    for (size_t i = 0; i < hilos.size(); i++) hilos[i].join();

    // El tiempo se mide hasta la ULTIMA OPERACION EXITOSA, no hasta que los
    // hilos salen. Motivo: cuando la version A pierde items, los consumidores
    // se quedan esperando algo que ya no va a llegar y recien cortan al
    // cumplirse 'idle'. Ese tiempo muerto no es trabajo y, si se contara,
    // dominaria la medicion de A y haria incomparables las variantes.
    Reloj::time_point ultimo = t0;
    for (size_t i = 0; i < rp.size(); i++)
        if (rp[i].ultimo > ultimo) ultimo = rp[i].ultimo;
    for (size_t i = 0; i < rc.size(); i++)
        if (rc[i].ultimo > ultimo) ultimo = rc[i].ultimo;

    Metricas m = reconciliar(rp, rc, cfg.capacidad);
    m.segundos = std::chrono::duration<double>(ultimo - t0).count();
    return m;
}

// ---------------------------------------------------------------------------
static void imprimir(const Config& cfg, const Metricas& m) {
    const char* ROJO  = cfg.color ? "\033[31m" : "";
    const char* VERDE = cfg.color ? "\033[32m" : "";
    const char* GRIS  = cfg.color ? "\033[90m" : "";
    const char* NEG   = cfg.color ? "\033[1m"  : "";
    const char* FIN   = cfg.color ? "\033[0m"  : "";

    const bool mal = m.inconsistencias() > 0 || m.violo_capacidad;
    const char* C  = mal ? ROJO : VERDE;

    printf("\n%s+----------------------------------------------------+%s\n", GRIS, FIN);
    printf("%s|%s %sVARIANTE %c%s  %d prod / %d cons / cap %d / %ld items %s|%s\n",
           GRIS, FIN, NEG, cfg.variante, FIN,
           cfg.productores, cfg.consumidores, cfg.capacidad, cfg.items, GRIS, FIN);
    printf("%s+----------------------------------------------------+%s\n", GRIS, FIN);

    printf("  Producidos           %10ld\n", m.producidos);
    printf("  Consumidos           %10ld\n", m.consumidos);
    printf("  %sPerdidos             %s%10ld%s\n", m.perdidos   ? ROJO : GRIS, m.perdidos   ? ROJO : "", m.perdidos, FIN);
    printf("  %sDuplicados           %s%10ld%s\n", m.duplicados ? ROJO : GRIS, m.duplicados ? ROJO : "", m.duplicados, FIN);
    printf("  %sFantasma             %s%10ld%s\n", m.fantasma   ? ROJO : GRIS, m.fantasma   ? ROJO : "", m.fantasma, FIN);
    printf("  %sCorruptos            %s%10ld%s\n", m.corruptos  ? ROJO : GRIS, m.corruptos  ? ROJO : "", m.corruptos, FIN);
    printf("  %s------------------------------------%s\n", GRIS, FIN);
    printf("  %sINCONSISTENCIAS      %10ld%s\n", C, m.inconsistencias(), FIN);
    printf("  Entregados OK        %10ld  (validos, exactamente una vez)\n", m.entregados_ok);
    printf("  Max cuenta observada %10d  (capacidad %d)%s%s%s\n",
           m.max_cuenta, cfg.capacidad,
           m.violo_capacidad ? ROJO : "", m.violo_capacidad ? "  <-- VIOLACION" : "", FIN);
    if (m.drenados)
        printf("  %sCortes por drenado   %10ld  (hilos que dejaron de progresar)%s\n",
               GRIS, m.drenados, FIN);
    if (m.timeouts)
        printf("  %sTIMEOUTS             %10ld  <-- corrida NO valida para el informe%s\n",
               ROJO, m.timeouts, FIN);
    printf("  Tiempo               %10.4f s\n", m.segundos);
    if (m.entregados_ok > 0) {
        // Se normaliza por items entregados OK, no por tiempo total: la
        // version A hace menos trabajo porque pierde items, asi que comparar
        // tiempos totales seria comparar cosas distintas.
        printf("  Tiempo por item OK   %10.4f us\n",
               m.segundos * 1e6 / static_cast<double>(m.entregados_ok));
    }
    printf("\n");
}

static void guardarCsv(const Config& cfg, const Metricas& m) {
    if (cfg.csv.empty()) return;
    std::ifstream prueba(cfg.csv.c_str());
    const bool existe = prueba.good();
    prueba.close();

    std::ofstream f(cfg.csv.c_str(), std::ios::app);
    if (!f.is_open()) {
        std::cerr << "Aviso: no se pudo abrir el CSV: " << cfg.csv << "\n";
        return;
    }
    if (!existe) {
        f << "variante,productores,consumidores,capacidad,items,"
             "producidos,consumidos,perdidos,duplicados,fantasma,corruptos,"
             "inconsistencias,entregados_ok,max_cuenta,violo_capacidad,"
             "drenados,timeouts,segundos\n";
    }
    f << cfg.variante << ',' << cfg.productores << ',' << cfg.consumidores << ','
      << cfg.capacidad << ',' << cfg.items << ',' << m.producidos << ',' << m.consumidos << ','
      << m.perdidos << ',' << m.duplicados << ',' << m.fantasma << ',' << m.corruptos << ','
      << m.inconsistencias() << ',' << m.entregados_ok << ',' << m.max_cuenta << ','
      << (m.violo_capacidad ? 1 : 0) << ',' << m.drenados << ',' << m.timeouts << ','
      << m.segundos << '\n';
}

static void ayuda(const char* prog) {
    printf("Uso: %s [opciones]\n", prog);
    printf("  --variante A|B|C|D   version del buffer (por defecto A)\n");
    printf("  --productores N      hilos productores (4)\n");
    printf("  --consumidores N     hilos consumidores (4)\n");
    printf("  --capacidad N        tamano del buffer (8)\n");
    printf("  --items N            items totales a producir (50000)\n");
    printf("  --timeout N          limite en segundos por corrida (10)\n");
    printf("  --csv RUTA           agrega una fila al archivo CSV\n");
    printf("  --sin-color          desactiva los colores ANSI\n");
    printf("  --ayuda              muestra esta ayuda\n");
}

int main(int argc, char** argv) {
    Config cfg;

    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        const bool hay = (i + 1 < argc);
        if      (a == "--variante"     && hay) cfg.variante     = argv[++i][0];
        else if (a == "--productores"  && hay) cfg.productores  = std::atoi(argv[++i]);
        else if (a == "--consumidores" && hay) cfg.consumidores = std::atoi(argv[++i]);
        else if (a == "--capacidad"    && hay) cfg.capacidad    = std::atoi(argv[++i]);
        else if (a == "--items"        && hay) cfg.items        = std::atol(argv[++i]);
        else if (a == "--timeout"      && hay) cfg.timeout_seg  = std::atoi(argv[++i]);
        else if (a == "--idle-ms"      && hay) cfg.idle_ms      = std::atoi(argv[++i]);
        else if (a == "--csv"          && hay) cfg.csv          = argv[++i];
        else if (a == "--sin-color")           cfg.color        = false;
        else if (a == "--ayuda" || a == "-h") { ayuda(argv[0]); return 0; }
        else { std::cerr << "Opcion no reconocida: " << a << "\n"; ayuda(argv[0]); return 1; }
    }

    if (cfg.productores < 2 || cfg.consumidores < 2) {
        std::cerr << "Error: el enunciado exige al menos 2 productores y 2 consumidores.\n";
        return 1;
    }
    if (cfg.idle_ms < 1) {
        std::cerr << "Error: --idle-ms debe ser positivo.\n";
        return 1;
    }
    if (cfg.capacidad < 1 || cfg.items < 1 || cfg.timeout_seg < 1) {
        std::cerr << "Error: capacidad, items y timeout deben ser positivos.\n";
        return 1;
    }

    Metricas m;
    switch (cfg.variante) {
        case 'A': m = ejecutar<BufferA>(cfg); break;
        case 'B': m = ejecutar<BufferB>(cfg); break;
        case 'C': m = ejecutar<BufferC>(cfg); break;
        case 'D': m = ejecutar<BufferD>(cfg); break;
        default:
            std::cerr << "Variante '" << cfg.variante << "' todavia no implementada.\n";
            return 1;
    }

    imprimir(cfg, m);
    guardarCsv(cfg, m);
    return 0;
}

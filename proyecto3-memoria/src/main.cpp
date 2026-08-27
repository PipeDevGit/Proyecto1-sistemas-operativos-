// ---------------------------------------------------------------------------
// Simulador de Gestion de Memoria - Reemplazo de paginas
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// Compara FIFO y LRU sobre la misma cadena de referencias, con el numero de
// marcos configurable por linea de comandos. Incluye ademas el algoritmo
// OPTIMO como cota inferior teorica (no exigido por el enunciado).
//
// Toda la salida es ASCII a proposito, para que se vea igual en la consola de
// Linux y en la consola clasica de Windows.
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <new>
#include <string>
#include <vector>

#include "Cadena.h"
#include "Simulacion.h"
#include "FIFO.h"
#include "LRU.h"
#include "Optimo.h"

struct Config {
    std::string cadena   = "data/corta.txt";
    std::string algoritmo = "todos";
    int         marcos   = 3;
    bool        traza    = false;
    bool        color    = true;
    std::string csv;
};

static void ayuda(const char* prog) {
    printf("Simulador de reemplazo de paginas - FIFO vs LRU\n\n");
    printf("Uso: %s [opciones]\n\n", prog);
    printf("  --cadena archivo    archivo con la cadena de referencias (data/corta.txt)\n");
    printf("  --marcos N          numero de marcos de memoria fisica (3)\n");
    printf("  --algoritmo A       fifo | lru | optimo | todos (todos)\n");
    printf("  --traza             imprime el estado de los marcos paso a paso\n");
    printf("  --csv archivo       anexa una fila por algoritmo con los resultados\n");
    printf("  --sin-color         desactiva los codigos ANSI\n");
    printf("  --ayuda             muestra esta ayuda\n");
}

// ---------------------------------------------------------------------------
// Traza paso a paso (RF-5). Reproduce la presentacion habitual de los libros:
// una fila por referencia, una columna por marco.
// ---------------------------------------------------------------------------
static void imprimirTraza(const std::vector<Paso>& traza, int marcos,
                          const std::string& algoritmo, bool color) {
    const char* ROJO  = color ? "\033[31m" : "";
    const char* VERDE = color ? "\033[32m" : "";
    const char* GRIS  = color ? "\033[90m" : "";
    const char* NEG   = color ? "\033[1m"  : "";
    const char* FIN   = color ? "\033[0m"  : "";

    printf("\n  %sTRAZA PASO A PASO - %s%s\n", NEG, algoritmo.c_str(), FIN);

    printf("  %s", GRIS);
    printf("  #  Ref |");
    for (int m = 0; m < marcos; m++) printf("%4d", m);
    printf("  | Resultado%s\n", FIN);

    printf("  %s", GRIS);
    printf("  ------------");
    for (int m = 0; m < marcos; m++) printf("----");
    printf("--------------------%s\n", FIN);

    for (size_t i = 0; i < traza.size(); i++) {
        const Paso& p = traza[i];
        printf("  %3d  %3d |", p.indice + 1, p.pagina);
        for (int m = 0; m < marcos; m++) {
            const int v = p.marcos[static_cast<size_t>(m)];
            if (v == MARCO_LIBRE) printf("   %s-%s", GRIS, FIN);
            else if (m == p.marcoUsado && p.fallo) printf("%s%4d%s", NEG, v, FIN);
            else printf("%4d", v);
        }
        if (p.fallo) {
            printf("  | %sFALLO%s", ROJO, FIN);
            if (p.victima != SIN_VICTIMA) printf("  %s(desaloja %d)%s", GRIS, p.victima, FIN);
        } else {
            printf("  | %sacierto%s", VERDE, FIN);
        }
        printf("\n");
    }
    printf("\n");
}

static void imprimirResumen(const std::vector<Metricas>& res, const Config& cfg) {
    const char* ROJO  = cfg.color ? "\033[31m" : "";
    const char* VERDE = cfg.color ? "\033[32m" : "";
    const char* GRIS  = cfg.color ? "\033[90m" : "";
    const char* NEG   = cfg.color ? "\033[1m"  : "";
    const char* FIN   = cfg.color ? "\033[0m"  : "";

    // El recuadro se dimensiona segun el contenido real. Con un ancho fijo,
    // una ruta de cadena larga desborda el borde y la cabecera queda
    // descuadrada respecto de la tabla de abajo.
    char cabecera[512];
    snprintf(cabecera, sizeof(cabecera), " RESULTADOS  cadena: %s   marcos: %d ",
             cfg.cadena.c_str(), cfg.marcos);
    int ancho = static_cast<int>(strlen(cabecera));
    if (ancho < 62) ancho = 62;   // nunca mas angosto que la tabla de resultados

    printf("\n%s+", GRIS);
    for (int i = 0; i < ancho; i++) printf("-");
    printf("+%s\n", FIN);
    printf("%s|%s%s%-*s%s%s|%s\n", GRIS, FIN, NEG, ancho, cabecera, FIN, GRIS, FIN);
    printf("%s+", GRIS);
    for (int i = 0; i < ancho; i++) printf("-");
    printf("+%s\n", FIN);

    printf("  %s%-10s %12s %10s %10s %10s %12s%s\n", NEG,
           "Algoritmo", "Referencias", "Aciertos", "Fallos", "% fallos", "Tiempo(us)", FIN);
    printf("  %s", GRIS);
    printf("  --------------------------------------------------------------");
    printf("%s\n", FIN);

    long mejor = -1;
    for (size_t i = 0; i < res.size(); i++)
        if (mejor < 0 || res[i].fallos < mejor) mejor = res[i].fallos;

    for (size_t i = 0; i < res.size(); i++) {
        const Metricas& m = res[i];
        const bool esMejor = (m.fallos == mejor);
        printf("  %-10s %12ld %10ld %s%10ld%s %9.2f%% %12.1f%s\n",
               m.algoritmo.c_str(), m.referencias, m.aciertos,
               esMejor ? VERDE : ROJO, m.fallos, FIN,
               m.tasaFallos(), m.microsegundos,
               m.coherente() ? "" : "  <-- INCOHERENTE");
    }
    printf("\n");
}

static void anexarCsv(const Config& cfg, const std::vector<Metricas>& res) {
    if (cfg.csv.empty()) return;
    const bool nuevo = !std::ifstream(cfg.csv.c_str()).good();
    std::ofstream f(cfg.csv.c_str(), std::ios::app);
    if (!f.is_open()) {
        std::cerr << "Aviso: no se pudo escribir el CSV en " << cfg.csv << "\n";
        return;
    }
    if (nuevo) f << "algoritmo,cadena,marcos,referencias,aciertos,fallos,tasa_fallos,microsegundos\n";
    for (size_t i = 0; i < res.size(); i++) {
        const Metricas& m = res[i];
        f << m.algoritmo << ',' << m.cadena << ',' << m.marcos << ','
          << m.referencias << ',' << m.aciertos << ',' << m.fallos << ','
          << m.tasaFallos() << ',' << m.microsegundos << '\n';
    }
}

int main(int argc, char** argv) {
    Config cfg;

    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        const bool hay = (i + 1 < argc);
        if      (a == "--cadena"    && hay) cfg.cadena    = argv[++i];
        else if (a == "--marcos"    && hay) cfg.marcos    = std::atoi(argv[++i]);
        else if (a == "--algoritmo" && hay) cfg.algoritmo = argv[++i];
        else if (a == "--csv"       && hay) cfg.csv       = argv[++i];
        else if (a == "--traza")            cfg.traza     = true;
        else if (a == "--sin-color")        cfg.color     = false;
        else if (a == "--ayuda" || a == "-h") { ayuda(argv[0]); return 0; }
        else {
            // Se distingue una opcion valida a la que le falta el valor de una
            // opcion que directamente no existe: el usuario necesita saber cual
            // de los dos errores cometio.
            if (a == "--cadena" || a == "--marcos" ||
                a == "--algoritmo" || a == "--csv") {
                std::cerr << "Error: la opcion " << a << " requiere un valor.\n\n";
            } else {
                std::cerr << "Error: opcion no reconocida: " << a << "\n\n";
            }
            ayuda(argv[0]);
            return 1;
        }
    }

    if (cfg.marcos < 1) {
        std::cerr << "Error: --marcos debe ser al menos 1.\n";
        return 1;
    }
    if (cfg.algoritmo != "fifo" && cfg.algoritmo != "lru" &&
        cfg.algoritmo != "optimo" && cfg.algoritmo != "todos") {
        std::cerr << "Error: --algoritmo debe ser fifo, lru, optimo o todos.\n";
        return 1;
    }

    std::vector<int> cadena;
    try {
        cadena = leerCadena(cfg.cadena);
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    // Mas marcos que referencias es imposible de aprovechar: cada referencia
    // carga a lo sumo una pagina. Sin este limite, un valor absurdo como
    // --marcos 2000000000 intentaba reservar el arreglo de marcos y abortaba
    // con un std::bad_alloc sin capturar, en vez de dar un error legible.
    if (static_cast<size_t>(cfg.marcos) > cadena.size()) {
        std::cerr << "Error: --marcos (" << cfg.marcos << ") supera el numero de "
                  << "referencias de la cadena (" << cadena.size() << ").\n"
                  << "       Mas marcos que referencias no puede cambiar el "
                  << "resultado.\n";
        return 1;
    }

    const int distintas = paginasDistintas(cadena);
    if (cfg.color) printf("\033[90m");
    printf("  Cadena: %s\n", cfg.cadena.c_str());
    printf("  Referencias: %d | Paginas distintas: %d | Marcos: %d\n",
           static_cast<int>(cadena.size()), distintas, cfg.marcos);
    if (cfg.marcos >= distintas) {
        printf("  Nota: con %d marcos caben las %d paginas distintas; a partir de\n",
               cfg.marcos, distintas);
        printf("        aqui no puede haber reemplazos y los fallos son solo los\n");
        printf("        obligatorios de carga inicial.\n");
    }
    if (cfg.color) printf("\033[0m");

    std::vector<Metricas> resultados;
    std::vector<Paso> traza;

    const bool todos = (cfg.algoritmo == "todos");

    // Red de seguridad ante agotamiento de memoria. Con una cadena muy larga y
    // la traza activada se reserva un registro por referencia; si el sistema no
    // puede satisfacerlo, conviene informarlo y salir con un codigo propio en
    // lugar de abortar con una excepcion sin capturar.
    try {
        if (todos || cfg.algoritmo == "fifo") {
            traza.clear();
            resultados.push_back(simular<FIFO>(cadena, cfg.marcos, "FIFO", cfg.cadena,
                                               cfg.traza ? &traza : 0));
            if (cfg.traza) imprimirTraza(traza, cfg.marcos, "FIFO", cfg.color);
        }
        if (todos || cfg.algoritmo == "lru") {
            traza.clear();
            resultados.push_back(simular<LRU>(cadena, cfg.marcos, "LRU", cfg.cadena,
                                              cfg.traza ? &traza : 0));
            if (cfg.traza) imprimirTraza(traza, cfg.marcos, "LRU", cfg.color);
        }
        if (todos || cfg.algoritmo == "optimo") {
            traza.clear();
            resultados.push_back(simular<Optimo>(cadena, cfg.marcos, "OPTIMO", cfg.cadena,
                                                 cfg.traza ? &traza : 0));
            if (cfg.traza) imprimirTraza(traza, cfg.marcos, "OPTIMO", cfg.color);
        }
    } catch (const std::bad_alloc&) {
        std::cerr << "\nError: memoria insuficiente para simular esta combinacion.\n"
                  << "       Probá con una cadena mas corta o sin --traza.\n";
        return 3;
    }

    imprimirResumen(resultados, cfg);
    anexarCsv(cfg, resultados);

    // Control de coherencia: aciertos + fallos debe igualar el total de
    // referencias en todos los algoritmos. Si falla, algo esta mal contado.
    for (size_t i = 0; i < resultados.size(); i++) {
        if (!resultados[i].coherente()) {
            std::cerr << "Error interno: metricas incoherentes en "
                      << resultados[i].algoritmo << "\n";
            return 2;
        }
    }
    return 0;
}

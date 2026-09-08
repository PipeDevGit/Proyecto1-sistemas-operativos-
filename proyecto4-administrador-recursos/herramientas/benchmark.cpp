// ---------------------------------------------------------------------------
// benchmark.cpp - Medicion de las operaciones de archivo
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   El resultado experimental obligatorio del enunciado: "tiempo de respuesta
//   de las operaciones de archivo bajo distinta cantidad de archivos (10 vs.
//   100 vs. 1000)". Ademas mide el costo de las dos rutas de monitoreo de
//   procesos, que es la comparacion cuantitativa explicita que pide la
//   Rubrica 1.
//
// SE MIDE EL CODIGO QUE SE ENTREGA
//   Las operaciones se cronometran llamando a gestor::crear, gestor::listar y
//   gestor::eliminar -las mismas funciones que usa el menu-, no a una version
//   simplificada escrita para el experimento. Eso incluye el costo de validar
//   cada ruta contra el directorio controlado, que es parte real del precio de
//   usar esta herramienta y no deberia esconderse.
//
// METODOLOGIA (y por que cada decision)
//
//   1. TAMANOS ALTERNADOS, no agrupados.
//      Se corre 10, 100, 1000, y recien entonces la repeticion siguiente. Si se
//      hicieran las cinco de 10 juntas y despues las cinco de 100, cualquier
//      cosa que pase en la maquina durante ese rato -otro proceso, el
//      recolector de basura del sistema- caeria entera sobre un solo tamano y
//      se leeria como si ese tamano fuera lento. Alternando, el ruido se
//      reparte parejo entre los tres.
//
//   2. SE REPORTA LA MEDIANA, no el promedio.
//      Una sola corrida contaminada por otro proceso puede multiplicar el
//      promedio; la mediana la ignora. En el Proyecto 2 una medicion unica dio
//      una diferencia de factor 10 por ruido de la VM.
//
//   3. LA PRIMERA REPETICION SE MARCA, NO SE DESCARTA.
//      Sale en el CSV con repeticion = 0 y una columna que la identifica. Mide
//      la cache fria del sistema de archivos, que es informacion legitima: se
//      informa aparte en vez de borrarla en silencio.
//
//   4. SE MIDE EL LISTADO EN DOS NIVELES.
//      'listar' recorre el directorio Y lee los metadatos de cada entrada, que
//      es lo que hace la herramienta de verdad. 'recorrer' solo enumera. La
//      diferencia entre los dos dice cuanto cuesta el stat por archivo frente
//      al costo fijo de abrir el directorio, que es justo lo que explica por
//      que listar no escala igual que crear.
//
//   5. SE MIDE APARTE EL COSTO DE LA VALIDACION.
//      La operacion 'validar' llama a Sandbox::resolver sin tocar el disco.
//      Todas las demas operaciones la incluyen, asi que sin medirla por
//      separado no se podria responder cuanto del tiempo de crear es la
//      comprobacion de seguridad y cuanto el sistema de archivos. Es una
//      pregunta natural, porque el directorio controlado es el centro del
//      proyecto y toda proteccion tiene un precio que conviene conocer.
//
// USO
//   ./herramientas/benchmark [opciones]
//     --base DIR         directorio de trabajo (results/bench_ws)
//     --repeticiones N   repeticiones ademas de la de cache fria (5)
//     --csv ARCHIVO      anexa las mediciones
//     --escenario NOMBRE etiqueta de la corrida (sin_carga)
//     --tam LISTA        tamanos separados por coma (10,100,1000)
// ---------------------------------------------------------------------------
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "GestorArchivos.h"
#include "MonitorProcesos.h"
#include "Sandbox.h"

namespace fs = std::filesystem;
using Reloj = std::chrono::steady_clock;

namespace {

// Sumidero de resultados: existe solo para que el compilador no pueda
// descartar los bucles de la carga interna.
volatile double sumidero = 0.0;

// steady_clock y no system_clock: system_clock puede saltar hacia atras si el
// sistema ajusta la hora (NTP) durante la medicion, y eso daria duraciones
// negativas. steady_clock solo avanza.
double microsegundos(Reloj::time_point a, Reloj::time_point b) {
    return std::chrono::duration<double, std::micro>(b - a).count();
}

struct Config {
    std::string base       = "results/bench_ws";
    std::string csv;
    std::string escenario  = "sin_carga";
    int         repeticiones = 5;
    std::vector<int> tamanos = {10, 100, 1000};

    // Carga generada DENTRO de este mismo proceso mientras se mide.
    long cargaMb    = 0;   // megabytes a reservar y tocar
    long cargaHilos = 0;   // hilos quemando CPU
};

// ---------------------------------------------------------------------------
// Carga interna
//
// POR QUE LA CARGA SE GENERA AQUI DENTRO Y NO SOLO CON PROCESOS APARTE
//   La primera version dejaba toda la carga en manos de instancias externas de
//   herramientas/carga lanzadas por el script. Al comprobar la evidencia
//   -contando cuantas seguian vivas- resulto que morian a los pocos segundos
//   cuando el protocolo corre por SSH sin terminal, y las mediciones "bajo
//   carga" se estaban tomando sobre un sistema en reposo.
//
//   Generando la presion de memoria y CPU dentro del proceso que mide, esa
//   dependencia desaparece: los hilos viven exactamente lo que vive la
//   medicion. Las instancias externas se conservan igual, porque son las que
//   suben el numero de PROCESOS del sistema, que es la otra mitad de lo que
//   pide el enunciado; pero ya no son el unico sosten del experimento.
//
//   El costo de esta decision, que hay que declarar: los hilos de carga
//   compiten por la CPU con el hilo que mide, dentro del mismo proceso. Eso es
//   justamente lo que se quiere -medir bajo contencion- pero significa que la
//   carga no es "externa y ajena", sino parte del mismo espacio de direcciones.
// ---------------------------------------------------------------------------
class CargaInterna {
public:
    CargaInterna(long mb, long hilos) {
        if (mb > 0) {
            try {
                bloque_.resize(static_cast<size_t>(mb) * 1024u * 1024u);
            } catch (const std::bad_alloc&) {
                std::fprintf(stderr, "Aviso: no se pudieron reservar %ld MB de carga.\n", mb);
                bloque_.clear();
            }
            // Tocar cada pagina: reservar no basta, Linux entrega las paginas
            // de forma perezosa y sin escribir en ellas la carga seria ficticia.
            for (size_t i = 0; i < bloque_.size(); i += 4096) bloque_[i] = 1;
        }
        for (long i = 0; i < hilos; ++i)
            equipo_.emplace_back([this] {
    // El resultado se escribe en un sumidero volatile al terminar. Marcar el
    // acumulador como volatile no alcanza: g++ 16.2.0 avisa igual de que se
    // asigna y nunca se lee, y el proyecto se entrega sin advertencias.
    // Escribiendo en un sumidero volatile la variable si se lee, y el
    // compilador tampoco puede borrar el bucle, porque tiene que producir ese
    // valor. Solo se ve compilando en Windows: g++ 15.2.0 no lo reporta.
                double x = 0.0;
                while (seguir_.load(std::memory_order_relaxed))
                    for (int k = 1; k < 10000; ++k) x += 1.0 / static_cast<double>(k);
                sumidero = x;
            });
    }

    ~CargaInterna() {
        seguir_.store(false, std::memory_order_relaxed);
        for (std::thread& t : equipo_) if (t.joinable()) t.join();
    }

    CargaInterna(const CargaInterna&) = delete;
    CargaInterna& operator=(const CargaInterna&) = delete;

    size_t megabytes() const { return bloque_.size() / (1024 * 1024); }
    size_t hilos()     const { return equipo_.size(); }

private:
    std::vector<char>        bloque_;
    std::vector<std::thread> equipo_;
    std::atomic<bool>        seguir_{true};
};

struct Medicion {
    std::string escenario, operacion;
    int    n = 0;
    int    repeticion = 0;
    bool   cacheFria = false;
    double us = 0.0;
};

void ayuda(const char* prog) {
    std::printf("Benchmark de operaciones de archivo - Proyecto 4\n\n");
    std::printf("Uso: %s [opciones]\n\n", prog);
    std::printf("  --base DIR          directorio de trabajo (results/bench_ws)\n");
    std::printf("  --repeticiones N    repeticiones ademas de la de cache fria (5)\n");
    std::printf("  --csv ARCHIVO       anexa las mediciones al archivo\n");
    std::printf("  --escenario NOMBRE  etiqueta de la corrida (sin_carga)\n");
    std::printf("  --tam 10,100,1000   tamanos a medir\n");
    std::printf("  --carga-mb N        MB de carga generados dentro de este proceso (0)\n");
    std::printf("  --carga-hilos N     hilos de carga dentro de este proceso (0)\n");
    std::printf("  --ayuda             muestra esta ayuda\n");
}

std::vector<int> partirTamanos(const std::string& s) {
    std::vector<int> v;
    std::string actual;
    for (char c : s) {
        if (c == ',') { if (!actual.empty()) v.push_back(std::atoi(actual.c_str())); actual.clear(); }
        else actual += c;
    }
    if (!actual.empty()) v.push_back(std::atoi(actual.c_str()));
    return v;
}

std::string nombreArchivo(int i) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "f%05d.txt", i);
    return buf;
}

// Contenido de 512 bytes: es el tamano que usa la guia de Semana 10 del curso.
// Importa que sea el mismo en todas las corridas, porque el costo de escribir
// depende del tamano y mezclarlos haria incomparables los resultados.
const std::string& contenido512() {
    static const std::string c(512, 'x');
    return c;
}

// Una ronda completa sobre un tamano: crear, recorrer, listar y eliminar.
void unaRonda(const Sandbox& caja, int n, const Config& cfg,
              int repeticion, bool cacheFria, std::vector<Medicion>& salida) {
    auto anotar = [&](const char* op, double us) {
        Medicion m;
        m.escenario  = cfg.escenario;
        m.operacion  = op;
        m.n          = n;
        m.repeticion = repeticion;
        m.cacheFria  = cacheFria;
        m.us         = us;
        salida.push_back(m);
    };

    // --- validar: SOLO la comprobacion del directorio controlado ------------
    //
    // Se mide aparte porque todas las demas operaciones la incluyen: crear,
    // eliminar y consultar metadatos pasan primero por Sandbox::resolver. Sin
    // este dato no se podria responder cuanto del tiempo de "crear" es la
    // comprobacion de seguridad y cuanto el sistema de archivos.
    //
    // No toca el disco: weakly_canonical si consulta el sistema de archivos
    // para resolver el tramo que existe, pero no escribe nada.
    auto t0 = Reloj::now();
    for (int i = 0; i < n; ++i) {
        const Resolucion r = caja.resolver(nombreArchivo(i));
        if (!r.ok) std::fprintf(stderr, "Aviso: resolucion inesperadamente rechazada\n");
    }
    auto t1 = Reloj::now();
    anotar("validar", microsegundos(t0, t1));

    // --- crear -------------------------------------------------------------
    t0 = Reloj::now();
    for (int i = 0; i < n; ++i) gestor::crear(caja, nombreArchivo(i), contenido512());
    t1 = Reloj::now();
    anotar("crear", microsegundos(t0, t1));

    // --- recorrer: solo enumerar, sin leer metadatos ------------------------
    t0 = Reloj::now();
    std::error_code ec;
    size_t contadas = 0;
    for (const auto& e : fs::directory_iterator(caja.base(), ec)) { (void)e; ++contadas; }
    t1 = Reloj::now();
    anotar("recorrer", microsegundos(t0, t1));

    // --- listar: enumerar Y leer metadatos de cada entrada -------------------
    std::vector<EntradaArchivo> entradas;
    t0 = Reloj::now();
    gestor::listar(caja, entradas);
    t1 = Reloj::now();
    anotar("listar", microsegundos(t0, t1));

    // --- eliminar ------------------------------------------------------------
    t0 = Reloj::now();
    for (int i = 0; i < n; ++i) gestor::eliminar(caja, nombreArchivo(i));
    t1 = Reloj::now();
    anotar("eliminar", microsegundos(t0, t1));

    // Comprobacion de coherencia: si el conteo no cuadra, la medicion no vale
    // y hay que saberlo antes de meterla en el informe.
    if (contadas < static_cast<size_t>(n))
        std::fprintf(stderr,
                     "Aviso: se esperaban al menos %d entradas y se recorrieron %zu "
                     "(n=%d, repeticion=%d)\n", n, contadas, n, repeticion);
}

// Costo de cada ruta de monitoreo de procesos. Es la comparacion cuantitativa
// explicita que pide la Rubrica 1: leer /proc no crea ningun proceso, mientras
// que invocar 'ps' implica fork + exec de un interprete de comandos y despues
// analizar su salida de texto.
void medirFuentesDeProcesos(const Config& cfg, std::vector<Medicion>& salida) {
    for (const IProveedorProcesos* p : sistema::proveedoresProcesos()) {
        if (!p->disponible()) continue;

        for (int r = 0; r <= cfg.repeticiones; ++r) {
            std::vector<ProcesoInfo> lista;
            std::string fuente;

            const auto t0 = Reloj::now();
            const std::string err = sistema::listarProcesos(lista, fuente, p->nombre());
            const auto t1 = Reloj::now();
            if (!err.empty()) break;

            Medicion m;
            m.escenario  = cfg.escenario;
            m.operacion  = std::string("procesos:") + p->nombre();
            m.n          = static_cast<int>(lista.size());
            m.repeticion = r;
            m.cacheFria  = (r == 0);
            m.us         = microsegundos(t0, t1);
            salida.push_back(m);
        }
    }
}

double mediana(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t m = v.size() / 2;
    return (v.size() % 2 == 1) ? v[m] : (v[m - 1] + v[m]) / 2.0;
}

} // namespace

int main(int argc, char** argv) {
    Config cfg;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto valor = [&](const char* que) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: %s necesita un valor.\n", que);
                std::exit(1);
            }
            return argv[++i];
        };
        if      (a == "--ayuda" || a == "-h") { ayuda(argv[0]); return 0; }
        else if (a == "--base")         cfg.base = valor("--base");
        else if (a == "--csv")          cfg.csv = valor("--csv");
        else if (a == "--escenario")    cfg.escenario = valor("--escenario");
        else if (a == "--repeticiones") cfg.repeticiones = std::atoi(valor("--repeticiones"));
        else if (a == "--tam")          cfg.tamanos = partirTamanos(valor("--tam"));
        else if (a == "--carga-mb")     cfg.cargaMb = std::atol(valor("--carga-mb"));
        else if (a == "--carga-hilos")  cfg.cargaHilos = std::atol(valor("--carga-hilos"));
        else { std::fprintf(stderr, "Error: opcion desconocida %s\n", a.c_str()); return 1; }
    }

    if (cfg.repeticiones < 1) {
        std::fprintf(stderr, "Error: --repeticiones debe ser al menos 1.\n");
        return 1;
    }
    if (cfg.tamanos.empty()) {
        std::fprintf(stderr, "Error: no se indico ningun tamano.\n");
        return 1;
    }

    std::error_code ec;
    fs::remove_all(cfg.base, ec);   // se parte de un directorio limpio

    Sandbox caja(cfg.base);
    if (!caja.listo()) {
        std::fprintf(stderr, "Error: %s\n", caja.error().c_str());
        return 1;
    }

    std::printf("Benchmark - escenario '%s'\n", cfg.escenario.c_str());
    std::printf("%s\n", sistema::descripcionPlataforma().c_str());
    std::printf("Directorio: %s\n", caja.base().string().c_str());
    std::printf("Tamanos:");
    for (int n : cfg.tamanos) std::printf(" %d", n);
    std::printf("   Repeticiones: %d (+1 de cache fria)\n\n", cfg.repeticiones);

    // La carga interna se levanta ANTES de la primera medicion y se apaga sola
    // al salir del ambito, despues de la ultima. Asi ninguna medicion queda
    // fuera de la condicion que dice el escenario.
    CargaInterna carga(cfg.cargaMb, cfg.cargaHilos);
    if (cfg.cargaMb > 0 || cfg.cargaHilos > 0)
        std::printf("  carga interna: %zu MB tocados, %zu hilos de CPU\n\n",
                    carga.megabytes(), carga.hilos());

    std::vector<Medicion> mediciones;

    // Repeticion 0: cache fria. Se corre primero y se marca; no se descarta.
    std::printf("  repeticion 0 (cache fria)");
    std::fflush(stdout);
    for (int n : cfg.tamanos) unaRonda(caja, n, cfg, 0, true, mediciones);
    std::printf("  ok\n");

    // Repeticiones 1..N, ALTERNANDO tamanos dentro de cada repeticion.
    for (int r = 1; r <= cfg.repeticiones; ++r) {
        std::printf("  repeticion %d", r);
        std::fflush(stdout);
        for (int n : cfg.tamanos) unaRonda(caja, n, cfg, r, false, mediciones);
        std::printf("  ok\n");
    }

    std::printf("  fuentes de procesos");
    std::fflush(stdout);
    medirFuentesDeProcesos(cfg, mediciones);
    std::printf("  ok\n");

    // --- resumen en pantalla ------------------------------------------------
    std::printf("\n  %-22s %8s %14s %14s\n", "Operacion", "N", "Mediana(us)", "Cache fria(us)");
    std::printf("  ");
    for (int i = 0; i < 62; ++i) std::printf("-");
    std::printf("\n");

    std::vector<std::string> operaciones;
    for (const Medicion& m : mediciones)
        if (std::find(operaciones.begin(), operaciones.end(), m.operacion) == operaciones.end())
            operaciones.push_back(m.operacion);

    for (const std::string& op : operaciones) {
        std::vector<int> enes;
        for (const Medicion& m : mediciones)
            if (m.operacion == op &&
                std::find(enes.begin(), enes.end(), m.n) == enes.end()) enes.push_back(m.n);
        std::sort(enes.begin(), enes.end());

        for (int n : enes) {
            std::vector<double> tibias;
            double fria = 0.0;
            for (const Medicion& m : mediciones) {
                if (m.operacion != op || m.n != n) continue;
                if (m.cacheFria) fria = m.us; else tibias.push_back(m.us);
            }
            if (tibias.empty()) continue;
            std::printf("  %-22s %8d %14.1f %14.1f\n",
                        op.c_str(), n, mediana(tibias), fria);
        }
    }

    // --- CSV ----------------------------------------------------------------
    if (!cfg.csv.empty()) {
        const bool nuevo = !fs::exists(cfg.csv, ec);
        std::ofstream f(cfg.csv, std::ios::app);
        if (!f) {
            std::fprintf(stderr, "Error: no se pudo abrir %s para escribir.\n", cfg.csv.c_str());
            return 1;
        }
        if (nuevo) f << "escenario,operacion,n,repeticion,cache_fria,microsegundos\n";
        for (const Medicion& m : mediciones)
            f << m.escenario << ',' << m.operacion << ',' << m.n << ','
              << m.repeticion << ',' << (m.cacheFria ? 1 : 0) << ','
              << m.us << '\n';
        std::printf("\n  %zu mediciones anexadas a %s\n", mediciones.size(), cfg.csv.c_str());
    }

    fs::remove_all(cfg.base, ec);
    return 0;
}

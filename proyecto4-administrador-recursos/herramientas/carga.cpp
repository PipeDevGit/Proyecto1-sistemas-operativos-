// ---------------------------------------------------------------------------
// carga.cpp - Generador de carga controlada de memoria y CPU
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   El enunciado pide medir "el comportamiento de la herramienta mientras el
//   sistema tiene una carga de procesos/memoria elevada", y dice que puede
//   apoyarse en las cargas de los Laboratorios 4 y 6. Este programa produce esa
//   carga de forma controlada y reproducible.
//
// POR QUE NO SE USA stress NI stress-ng
//   No estan instalados en la VM del curso, y agregarlos obligaria a correr
//   'sudo apt install' antes de poder reproducir los experimentos. Escribir la
//   carga como parte del proyecto la hace versionable, portable a Windows sin
//   instalar nada, y sobre todo EXPLICABLE: se sabe exactamente que hace,
//   porque son cincuenta lineas propias en vez de una herramienta externa.
//
// COMO SE CONSIGUE CARGA DE PROCESOS
//   Este programa es UNA sola instancia. La carga de muchos procesos la produce
//   el script lanzando varias instancias en segundo plano. Se hace asi -y no
//   con fork() dentro del programa- porque fork() es de POSIX y no existe en
//   Windows: meterlo aqui obligaria a un #ifdef, y la regla del proyecto es que
//   SistemaInfo.cpp sea el unico archivo con condicionales de plataforma.
//   Lanzar N instancias desde el script consigue lo mismo sin romper la regla.
//
// POR QUE SE ESCRIBE EN LA MEMORIA RESERVADA
//   Reservar con new no basta: Linux entrega paginas de forma perezosa, asi que
//   la memoria no se ocupa de verdad hasta que se TOCA. Sin escribir en ella,
//   VmRSS casi no sube y la "carga de memoria" seria ficticia. Se escribe una
//   vez por pagina, que es el minimo para forzar la asignacion real.
//
// USO
//   ./herramientas/carga [--mb N] [--hilos N] [--segundos N]
// ---------------------------------------------------------------------------
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

namespace {

// Tamano de pagina supuesto para tocar la memoria. No hace falta que coincida
// exactamente con el del sistema: si el real fuera mayor se tocarian algunas
// paginas de mas, y si fuera menor igual se toca al menos una vez cada 4 KB.
// Se evita a proposito sysconf(), que obligaria a un #ifdef en este archivo.
const size_t PASO_PAGINA = 4096;

std::atomic<bool> seguir{true};

// Sumidero de resultados: existe solo para que el compilador no pueda
// descartar los bucles de carga.
volatile double sumidero = 0.0;

void quemarCpu() {
    // El resultado se escribe en un sumidero volatile al terminar. Marcar el
    // acumulador como volatile no alcanza: g++ 16.2.0 avisa igual de que se
    // asigna y nunca se lee, y el proyecto se entrega sin advertencias.
    // Escribiendo en un sumidero volatile la variable si se lee, y el
    // compilador tampoco puede borrar el bucle, porque tiene que producir ese
    // valor. Solo se ve compilando en Windows: g++ 15.2.0 no lo reporta.
    double x = 0.0;
    while (seguir.load(std::memory_order_relaxed))
        for (int i = 1; i < 10000; ++i) x += 1.0 / static_cast<double>(i);
    sumidero = x;
}

void ayuda(const char* prog) {
    std::printf("Generador de carga controlada - Proyecto 4\n\n");
    std::printf("Uso: %s [opciones]\n\n", prog);
    std::printf("  --mb N        megabytes a reservar y TOCAR (256)\n");
    std::printf("  --hilos N     hilos quemando CPU (2)\n");
    std::printf("  --segundos N  cuanto dura la carga (20)\n");
    std::printf("  --silencioso  no imprime nada\n");
    std::printf("  --ayuda       muestra esta ayuda\n\n");
    std::printf("La carga de muchos PROCESOS se consigue lanzando varias\n");
    std::printf("instancias en segundo plano; ver scripts/experimentos.sh\n");
}

} // namespace

int main(int argc, char** argv) {
    long mb = 256, hilos = 2, segundos = 20;
    bool silencioso = false;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto valor = [&](const char* que) -> long {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: %s necesita un numero.\n", que);
                std::exit(1);
            }
            return std::atol(argv[++i]);
        };
        if      (a == "--ayuda" || a == "-h") { ayuda(argv[0]); return 0; }
        else if (a == "--mb")         mb = valor("--mb");
        else if (a == "--hilos")      hilos = valor("--hilos");
        else if (a == "--segundos")   segundos = valor("--segundos");
        else if (a == "--silencioso") silencioso = true;
        else { std::fprintf(stderr, "Error: opcion desconocida %s\n", a.c_str()); return 1; }
    }

    if (mb < 0 || hilos < 0 || segundos <= 0) {
        std::fprintf(stderr, "Error: los valores deben ser positivos.\n");
        return 1;
    }

    // --- memoria -------------------------------------------------------------
    std::vector<char> bloque;
    try {
        bloque.resize(static_cast<size_t>(mb) * 1024u * 1024u);
    } catch (const std::bad_alloc&) {
        // Pedir mas memoria de la que hay no debe tumbar el experimento: se
        // avisa y se sigue con la carga de CPU, que es lo que si se pudo.
        std::fprintf(stderr, "Aviso: no se pudieron reservar %ld MB; se sigue sin carga de memoria.\n", mb);
        bloque.clear();
    }

    // Tocar cada pagina para que la memoria se asigne de verdad.
    for (size_t i = 0; i < bloque.size(); i += PASO_PAGINA) bloque[i] = 1;

    // --- CPU ------------------------------------------------------------------
    std::vector<std::thread> equipo;
    equipo.reserve(static_cast<size_t>(hilos));
    for (long i = 0; i < hilos; ++i) equipo.emplace_back(quemarCpu);

    if (!silencioso) {
        std::printf("Carga activa: %ld MB tocados, %ld hilos de CPU, %ld segundos.\n",
                    static_cast<long>(bloque.size() / (1024 * 1024)), hilos, segundos);
        std::fflush(stdout);
    }

    std::this_thread::sleep_for(std::chrono::seconds(segundos));
    seguir.store(false, std::memory_order_relaxed);
    for (std::thread& t : equipo) t.join();

    if (!silencioso) std::printf("Carga terminada.\n");
    return 0;
}

// ---------------------------------------------------------------------------
// DEMOSTRACION DE INTERBLOQUEO (DEADLOCK)
// TIIT2007 Sistemas Operativos - Universidad Invenio
//
// Complementa al simulador Productor-Consumidor. Cubre el tema de la Semana 6
// (laminas 6, 7 y 8 de la Unidad III): las cuatro condiciones de Coffman y
// dos tecnicas de prevencion.
//
// Tres modos:
//   --modo deadlock  dos cerrojos tomados en ORDEN INVERSO -> se traba
//   --modo orden     ambos hilos toman en el MISMO ORDEN    -> termina
//   --modo scoped    std::scoped_lock toma los dos a la vez -> termina
//
// El interbloqueo es REAL: no hay sleep() ni esperas artificiales para
// forzarlo. Simplemente se repite la operacion hasta que el planificador
// entrelaza los dos hilos de la forma que lo produce, que ocurre enseguida.
// ---------------------------------------------------------------------------
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>

typedef std::chrono::steady_clock Reloj;

static std::mutex tenedorA;   // "tenedor izquierdo"  (Filosofos Comensales)
static std::mutex tenedorB;   // "tenedor derecho"

static std::atomic<long> trabajoHecho(0);
static std::atomic<int>  hilosTerminados(0);
static std::atomic<int>  estadoHilo1(0);   // 0=inicio 1=tiene el 1ro 2=tiene los dos
static std::atomic<int>  estadoHilo2(0);

// Hilo 1: toma A y luego B.
static void tomaAB(long iteraciones) {
    for (long i = 0; i < iteraciones; i++) {
        std::lock_guard<std::mutex> a(tenedorA);
        estadoHilo1.store(1);
        std::lock_guard<std::mutex> b(tenedorB);   // <-- se traba aca
        estadoHilo1.store(2);
        trabajoHecho++;
        estadoHilo1.store(0);
    }
    hilosTerminados++;
}

// Hilo 2: toma B y luego A. EL ORDEN INVERTIDO ES TODO EL PROBLEMA.
static void tomaBA(long iteraciones) {
    for (long i = 0; i < iteraciones; i++) {
        std::lock_guard<std::mutex> b(tenedorB);
        estadoHilo2.store(1);
        std::lock_guard<std::mutex> a(tenedorA);   // <-- se traba aca
        estadoHilo2.store(2);
        trabajoHecho++;
        estadoHilo2.store(0);
    }
    hilosTerminados++;
}

// Prevencion 1: los dos hilos toman SIEMPRE en el mismo orden (A y despues
// B). Rompe la condicion de ESPERA CIRCULAR: ya no puede existir un ciclo.
static void ordenado(long iteraciones) {
    for (long i = 0; i < iteraciones; i++) {
        std::lock_guard<std::mutex> a(tenedorA);
        std::lock_guard<std::mutex> b(tenedorB);
        trabajoHecho++;
    }
    hilosTerminados++;
}

// Prevencion 2: scoped_lock adquiere AMBOS cerrojos como una sola operacion
// atomica (internamente reintenta hasta lograr los dos). Rompe la condicion
// de RETENCION Y ESPERA: nunca se queda con uno mientras espera el otro.
static void conScopedLock(long iteraciones) {
    for (long i = 0; i < iteraciones; i++) {
        std::scoped_lock cerrojos(tenedorA, tenedorB);
        trabajoHecho++;
    }
    hilosTerminados++;
}

static void ayuda(const char* prog) {
    printf("Uso: %s [--modo deadlock|orden|scoped] [--iteraciones N]\n", prog);
    printf("                [--espera N] [--sin-color]\n");
    printf("  --modo deadlock   orden inverso de adquisicion (se traba)\n");
    printf("  --modo orden      mismo orden en ambos hilos (previene)\n");
    printf("  --modo scoped     std::scoped_lock (previene)\n");
    printf("  --iteraciones N   repeticiones por hilo (100000)\n");
    printf("  --espera N        segundos antes de declarar interbloqueo (3)\n");
}

int main(int argc, char** argv) {
    std::string modo = "deadlock";
    long iteraciones = 100000;
    int  espera = 3;
    bool color = true;

    for (int i = 1; i < argc; i++) {
        const std::string a = argv[i];
        const bool hay = (i + 1 < argc);
        if      (a == "--modo"        && hay) modo        = argv[++i];
        else if (a == "--iteraciones" && hay) iteraciones = std::atol(argv[++i]);
        else if (a == "--espera"      && hay) espera      = std::atoi(argv[++i]);
        else if (a == "--sin-color")          color       = false;
        else if (a == "--ayuda" || a == "-h") { ayuda(argv[0]); return 0; }
        else { fprintf(stderr, "Opcion no reconocida: %s\n", a.c_str()); ayuda(argv[0]); return 1; }
    }
    if (modo != "deadlock" && modo != "orden" && modo != "scoped") {
        fprintf(stderr, "Modo no reconocido: %s\n", modo.c_str());
        ayuda(argv[0]);
        return 1;
    }

    const char* ROJO  = color ? "\033[31m" : "";
    const char* VERDE = color ? "\033[32m" : "";
    const char* GRIS  = color ? "\033[90m" : "";
    const char* NEG   = color ? "\033[1m"  : "";
    const char* FIN   = color ? "\033[0m"  : "";

    printf("\n%s+--------------------------------------------------------+%s\n", GRIS, FIN);
    printf("%s|%s %sDEMOSTRACION DE INTERBLOQUEO%s  modo: %-8s            %s|%s\n",
           GRIS, FIN, NEG, FIN, modo.c_str(), GRIS, FIN);
    printf("%s+--------------------------------------------------------+%s\n", GRIS, FIN);
    printf("  Dos hilos, dos cerrojos (tenedorA y tenedorB), %ld iteraciones.\n", iteraciones);

    if (modo == "deadlock") {
        printf("  Hilo 1 toma %sA -> B%s ... Hilo 2 toma %sB -> A%s  (orden invertido)\n\n",
               NEG, FIN, ROJO, FIN);
    } else if (modo == "orden") {
        printf("  Ambos hilos toman %sA -> B%s  (mismo orden)\n\n", VERDE, FIN);
    } else {
        printf("  Ambos hilos usan %sstd::scoped_lock(A, B)%s\n\n", VERDE, FIN);
    }

    const Reloj::time_point t0 = Reloj::now();

    std::thread h1, h2;
    if (modo == "deadlock") {
        h1 = std::thread(tomaAB, iteraciones);
        h2 = std::thread(tomaBA, iteraciones);
    } else if (modo == "orden") {
        h1 = std::thread(ordenado, iteraciones);
        h2 = std::thread(ordenado, iteraciones);
    } else {
        h1 = std::thread(conScopedLock, iteraciones);
        h2 = std::thread(conScopedLock, iteraciones);
    }

    // Un hilo interbloqueado no se puede unir jamas: join() colgaria el
    // programa para siempre. Por eso se los desacopla y se vigila desde
    // afuera con un limite de tiempo. Esto es exactamente la estrategia de
    // DETECCION de la lamina 8: se deja que el interbloqueo ocurra y se lo
    // descubre observando que el sistema dejo de progresar.
    h1.detach();
    h2.detach();

    const Reloj::time_point limite = Reloj::now() + std::chrono::seconds(espera);
    long ultimoTrabajo = -1;
    while (hilosTerminados.load() < 2 && Reloj::now() < limite) {
        ultimoTrabajo = trabajoHecho.load();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    const double seg = std::chrono::duration<double>(Reloj::now() - t0).count();
    const long   hecho = trabajoHecho.load();

    if (hilosTerminados.load() == 2) {
        printf("  %sTERMINO SIN BLOQUEARSE%s\n", VERDE, FIN);
        printf("  Operaciones completadas: %ld de %ld\n", hecho, iteraciones * 2);
        printf("  Tiempo: %.4f s\n\n", seg);
        return 0;
    }

    // No progreso -> interbloqueo.
    printf("  %s%sINTERBLOQUEO DETECTADO%s tras %.1f s sin progreso\n", ROJO, NEG, FIN, seg);
    printf("  Operaciones completadas: %s%ld%s de %ld (%.1f%%)\n",
           ROJO, hecho, FIN, iteraciones * 2,
           100.0 * static_cast<double>(hecho) / static_cast<double>(iteraciones * 2));
    printf("  El contador quedo congelado en %ld.\n\n", ultimoTrabajo);

    printf("  %sEstado de los hilos%s\n", NEG, FIN);
    const char* est[] = {"sin cerrojos", "tiene el primero, espera el segundo", "tiene los dos"};
    printf("    Hilo 1 (A->B): %s\n", est[estadoHilo1.load() % 3]);
    printf("    Hilo 2 (B->A): %s\n\n", est[estadoHilo2.load() % 3]);

    printf("  %sLas cuatro condiciones de Coffman, todas presentes%s\n", NEG, FIN);
    printf("    1. Exclusion mutua .... un mutex no se puede compartir\n");
    printf("    2. Retencion y espera . cada hilo retiene uno y espera el otro\n");
    printf("    3. Sin apropiacion .... nadie puede quitarle el cerrojo al otro\n");
    printf("    4. Espera circular .... H1 espera a H2 y H2 espera a H1\n\n");
    printf("  %sBasta con romper UNA para que el interbloqueo sea imposible:%s\n", NEG, FIN);
    printf("    --modo orden    rompe la 4 (ordenando la adquisicion)\n");
    printf("    --modo scoped   rompe la 2 (tomando ambos de una vez)\n\n");

    fflush(stdout);
    // Salida inmediata: los dos hilos siguen trabados y no hay forma de
    // recuperarlos. _Exit no corre destructores estaticos, que podrian
    // quedarse esperando esos mismos cerrojos.
    std::_Exit(0);
}

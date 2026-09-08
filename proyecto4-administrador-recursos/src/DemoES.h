// ---------------------------------------------------------------------------
// DemoES.h - Polling, delegacion e hibrido: las tres formas de esperar a un
//            dispositivo (Unidad VI)
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Mide, en la maquina donde se ejecuta, las tres estrategias con las que un
//   programa puede esperar a que un dispositivo termine su trabajo. Es la
//   Unidad VI hecha experimento en vez de teoria.
//
// POR QUE ESTA EN EL PROYECTO SI EL ENUNCIADO NO LO PIDE
//   En la clase de la Semana 10, justo despues de explicar polling,
//   interrupciones y el esquema hibrido tipo NAPI, el profesor dijo:
//   "adivinen que pueden implementar en el proyecto cuatro final... esto le
//   sube la nota en la defensa". El enunciado escrito no lo pide; la clase si
//   lo sugirio. Es el mismo patron que en el Proyecto 1, donde se pregunto por
//   SJF y prioridades aunque el enunciado solo pidiera FCFS y Round Robin.
//
// EL ERROR QUE ESTA MEDICION EVITA
//   Lo intuitivo seria cronometrar cuanto tarda cada estrategia en enterarse de
//   que el dispositivo termino. Y ahi no se ve nada: las tres tardan
//   practicamente lo mismo de reloj, porque el dispositivo tarda lo que tarda.
//   La diferencia no esta en el TIEMPO DE RELOJ sino en el TIEMPO DE CPU: el
//   polling quema un nucleo entero mientras espera; dormir no consume nada.
//   Por eso aca se mide lo que consume cada una, no solo cuanto dura.
//
// COMO SE SIMULA EL DISPOSITIVO, Y UNA MEDICION QUE HUBO QUE REHACER
//   El dispositivo es un PLAZO: esta listo cuando el reloj pasa cierto
//   instante. Consultar si ya termino es leer el reloj, que es lo mismo que
//   hace un controlador real cuando el programa consulta su registro de estado.
//
//   La primera version usaba un hilo por espera, que dormia y despues marcaba
//   una bandera atomica. Parecia mas fiel, y daba numeros imposibles:
//   'delegacion' consumia 45% de un nucleo -dormir no consume nada- y 'polling'
//   mostraba 553 us de latencia, cuando por definicion se entera al instante.
//
//   La causa era que crear y destruir 400 hilos cuesta CPU de verdad, y ese
//   costo, identico en las tres estrategias, tapaba justo la diferencia que se
//   queria mostrar. La medicion estaba cronometrando la creacion de hilos, no
//   la espera. Con el plazo no hay hilos de por medio y queda lo que importa.
//
// LAS TRES ESTRATEGIAS
//   1. POLLING (espera activa). El bucle pregunta sin parar si ya termino.
//      Se entera al instante, y para eso quema un nucleo completo.
//      En un SO real: E/S programada, el 'while (status != listo);' de OSTEP.
//
//   2. DELEGACION (dormir). El proceso se duerme y deja el nucleo libre.
//      No consume CPU, pero se entera tarde: solo despierta cada tanto.
//      En un SO real: bloquearse y que la interrupcion del dispositivo lo
//      despierte.
//
//   3. HIBRIDO. Hace polling un rato corto y, si en ese rato no termino, se
//      duerme. Si el dispositivo es rapido, gana la latencia del polling sin
//      pagarla mucho; si es lento, degrada a delegacion.
//      En un SO real: es NAPI en las tarjetas de red de Linux, y es la razon de
//      que los mutex modernos hagan un breve giro antes de bloquearse.
// ---------------------------------------------------------------------------
#ifndef DEMOES_H
#define DEMOES_H

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#include "Consola.h"
#include "MonitorProcesos.h"

namespace demoes {

using Reloj = std::chrono::steady_clock;

struct Resultado {
    std::string estrategia;
    std::string mecanismoReal;   // a que corresponde en un SO de verdad
    double msReloj    = 0.0;     // tiempo total de reloj
    double msCpu      = 0.0;     // CPU consumida (usuario + sistema)
    double usLatencia = 0.0;     // cuanto tardo en NOTAR que ya estaba listo

    // Fraccion de un nucleo que la estrategia mantuvo ocupado. 1.0 significa
    // que quemo un nucleo entero durante toda la espera.
    double ocupacion() const { return msReloj > 0.0 ? msCpu / msReloj : 0.0; }
};

// El dispositivo esta listo cuando el reloj pasa 'listoEn'. Consultar esto es
// el equivalente de leer el registro de estado del controlador.
inline bool yaTermino(Reloj::time_point listoEn) { return Reloj::now() >= listoEn; }

// ESPERA ACTIVA: pregunta sin parar hasta que el dispositivo responde.
// Es literalmente el 'while (busy);' del protocolo de E/S programada: la CPU no
// hace nada util, pero tampoco la suelta.
inline void esperarPolling(Reloj::time_point listoEn) {
    while (!yaTermino(listoEn)) { }
}

// DELEGACION: se duerme a intervalos y deja el nucleo libre entre uno y otro.
// No consume CPU mientras duerme, pero solo se entera al despertar.
inline void esperarDurmiendo(Reloj::time_point listoEn,
                             std::chrono::microseconds paso) {
    while (!yaTermino(listoEn)) std::this_thread::sleep_for(paso);
}

// HIBRIDO: gira un rato corto y, si el dispositivo no respondio, se duerme.
inline void esperarHibrido(Reloj::time_point listoEn,
                           std::chrono::microseconds ventanaGiro,
                           std::chrono::microseconds paso) {
    const auto finDelGiro = Reloj::now() + ventanaGiro;
    while (!yaTermino(listoEn)) {
        if (Reloj::now() >= finDelGiro) {
            // Se acabo la paciencia: el dispositivo resulto lento, asi que se
            // deja de girar y se pasa a dormir. Este es el punto exacto donde
            // NAPI decide cambiar de modo.
            while (!yaTermino(listoEn)) std::this_thread::sleep_for(paso);
            return;
        }
    }
}

// Corre una estrategia 'repeticiones' veces contra un dispositivo que tarda
// 'retraso' en responder, y devuelve lo que consumio.
//
// Se repite muchas veces a proposito: /proc informa el tiempo de CPU en tics de
// reloj de 10 ms, asi que una sola espera de 2 ms quedaria por debajo de la
// resolucion y se leeria como cero. Acumulando cientos de esperas, el consumo
// se vuelve medible.
//
// La latencia se calcula contra el instante EXACTO en que el dispositivo
// quedo listo, que se conoce de antemano porque es un plazo. Asi la latencia
// mide solo lo que tardo la estrategia en notarlo, sin mezclar nada mas.
template <typename Espera>
Resultado medir(const std::string& nombre, const std::string& mecanismo,
                int repeticiones, std::chrono::microseconds retraso, Espera espera) {
    Resultado r;
    r.estrategia    = nombre;
    r.mecanismoReal = mecanismo;

    const ConsumoPropio antes = sistema::consumoPropio();
    const auto t0 = Reloj::now();
    double latenciaTotal = 0.0;

    for (int i = 0; i < repeticiones; ++i) {
        const auto listoEn = Reloj::now() + retraso;
        espera(listoEn);
        const auto notado = Reloj::now();
        latenciaTotal += std::chrono::duration<double, std::micro>(notado - listoEn).count();
    }

    const auto t1 = Reloj::now();
    const ConsumoPropio despues = sistema::consumoPropio();

    r.msReloj    = std::chrono::duration<double, std::milli>(t1 - t0).count();
    r.msCpu      = ((despues.tiempoUsuario - antes.tiempoUsuario) +
                    (despues.tiempoSistema - antes.tiempoSistema)) * 1000.0;
    r.usLatencia = latenciaTotal / repeticiones;
    return r;
}

// ---------------------------------------------------------------------------
// Vista
// ---------------------------------------------------------------------------

// Mide cuanto duerme de verdad this_thread::sleep_for cuando se le pide un
// intervalo corto. El numero importa: explica por que la latencia de la
// estrategia que duerme se mide en milisegundos aunque se le pidan pasos de
// microsegundos. El kernel no puede despertar a un proceso con mas precision
// que la de su temporizador, y ademas hay que esperar a que el planificador lo
// vuelva a poner en un nucleo.
inline double granularidadRealDeDormir(std::chrono::microseconds pedido, int muestras) {
    const auto t0 = Reloj::now();
    for (int i = 0; i < muestras; ++i) std::this_thread::sleep_for(pedido);
    const auto t1 = Reloj::now();
    return std::chrono::duration<double, std::micro>(t1 - t0).count() / muestras;
}

inline void ejecutar(const Estilo& e, int repeticiones, int retrasoUs) {
    consola::titulo(e, "POLLING, DELEGACION E HIBRIDO (Unidad VI)");

    const std::chrono::microseconds paso(200);
    const std::chrono::microseconds ventana(500);   // cuanto gira el hibrido

    std::printf("  %sTres formas de esperar a que un dispositivo termine, medidas en%s\n",
                e.gris(), e.fin());
    std::printf("  %sesta maquina. Lo que se compara no es cuanto tardan -el dispositivo%s\n",
                e.gris(), e.fin());
    std::printf("  %starda lo que tarda- sino CUANTA CPU consumen mientras esperan.%s\n\n",
                e.gris(), e.fin());

    // --- granularidad real de dormir ---------------------------------------
    const double real = granularidadRealDeDormir(paso, 200);
    std::printf("  %sPrimero, un dato que explica todo lo demas:%s\n", e.negrita(), e.fin());
    std::printf("  se pide dormir %ld us y el sistema duerme %s%.0f us%s de verdad.\n",
                static_cast<long>(paso.count()), e.ambar(), real, e.fin());
    std::printf("  %sEl kernel no despierta a un proceso con mas precision que la de su%s\n",
                e.gris(), e.fin());
    std::printf("  %stemporizador, y ademas hay que esperar a que el planificador lo%s\n",
                e.gris(), e.fin());
    std::printf("  %svuelva a poner en un nucleo. Por eso dormir cuesta latencia.%s\n\n",
                e.gris(), e.fin());

    // --- tres velocidades de dispositivo ------------------------------------
    // El hibrido solo se puede juzgar comparandolo contra dispositivos de
    // distinta velocidad: su razon de ser es adaptarse a cual toco.
    struct Caso { const char* nombre; int us; };
    const Caso casos[3] = {
        { "RAPIDO  (200 us, entra en la ventana de giro)", 200 },
        { "MEDIO   (2 ms, no entra)",                      retrasoUs },
        { "LENTO   (10 ms, muy por fuera)",                10000 }
    };

    for (const Caso& c : casos) {
        // Con dispositivos lentos hacen falta menos repeticiones para juntar
        // tiempo de CPU medible, y asi la demo no se eterniza.
        const int reps = c.us >= 10000 ? repeticiones / 4
                       : (c.us >= 2000 ? repeticiones / 2 : repeticiones);
        const std::chrono::microseconds retraso(c.us);

        std::printf("  %sDispositivo %s%s   %s(%d esperas)%s\n",
                    e.negrita(), c.nombre, e.fin(), e.gris(), reps, e.fin());
        std::printf("  %s%-12s %10s %10s %11s %11s%s\n", e.negrita(),
                    "Estrategia", "Reloj(ms)", "CPU(ms)", "Ocupacion", "Latencia", e.fin());
        consola::regla(e, 62);

        std::vector<Resultado> resultados;
        resultados.push_back(medir("Polling", "E/S programada", reps, retraso,
            [](Reloj::time_point f) { esperarPolling(f); }));
        resultados.push_back(medir("Delegacion", "bloqueo + interrupcion", reps, retraso,
            [&](Reloj::time_point f) { esperarDurmiendo(f, paso); }));
        resultados.push_back(medir("Hibrido", "NAPI / mutex adaptativo", reps, retraso,
            [&](Reloj::time_point f) { esperarHibrido(f, ventana, paso); }));

        for (const Resultado& r : resultados) {
            const char* col = r.ocupacion() < 0.20 ? e.verde()
                            : (r.ocupacion() < 0.80 ? e.ambar() : e.rojo());
            std::printf("  %-12s %10.1f %10.1f %s%10.0f%%%s %8.0f us\n",
                        r.estrategia.c_str(), r.msReloj, r.msCpu,
                        col, r.ocupacion() * 100.0, e.fin(), r.usLatencia);
        }
        std::printf("\n");
    }

    // --- lectura ------------------------------------------------------------
    std::printf("  %sQue mirar%s\n", e.negrita(), e.fin());
    std::printf("  %sOcupacion%s es la fraccion de un nucleo que la estrategia mantuvo\n",
                e.negrita(), e.fin());
    std::printf("  secuestrado. El polling ronda el 100%% en los tres casos: espere lo que\n");
    std::printf("  espere, no suelta la CPU. Dormir se queda muy abajo, y paga esa\n");
    std::printf("  economia en latencia.\n\n");

    std::printf("  %sPuede pasar del 100%%, y no es un error:%s /proc informa el tiempo de\n",
                e.gris(), e.fin());
    std::printf("  %sCPU en tics de 10 ms, asi que en una medicion de 85 ms el redondeo%s\n",
                e.gris(), e.fin());
    std::printf("  %sal tic mas cercano puede empujar el cociente por encima de uno.%s\n",
                e.gris(), e.fin());

    std::printf("  %sEl hibrido cambia de comportamiento segun el dispositivo%s, que es\n",
                e.negrita(), e.fin());
    std::printf("  exactamente su proposito: con el dispositivo rapido lo alcanza girando\n");
    std::printf("  y consigue la latencia del polling; con el lento se rinde y duerme,\n");
    std::printf("  quedando cerca de la delegacion. Ninguna de las dos estrategias puras\n");
    std::printf("  sirve para las dos velocidades, y por eso existe la tercera.\n\n");

    std::printf("  %sEn un sistema operativo real esto es NAPI: cuando llegan pocos%s\n",
                e.gris(), e.fin());
    std::printf("  %spaquetes, la tarjeta de red interrumpe; cuando llega una avalancha,%s\n",
                e.gris(), e.fin());
    std::printf("  %sel driver apaga las interrupciones y pasa a consultar en bucle,%s\n",
                e.gris(), e.fin());
    std::printf("  %sporque atender una interrupcion por paquete costaria mas que mirar.%s\n",
                e.gris(), e.fin());
    std::printf("  %sEs tambien la razon de que un mutex moderno gire un instante antes%s\n",
                e.gris(), e.fin());
    std::printf("  %sde bloquearse: si el dueno va a soltarlo enseguida, dormirse y%s\n",
                e.gris(), e.fin());
    std::printf("  %sdespertarse cuesta mas que la espera misma.%s\n", e.gris(), e.fin());
}

} // namespace demoes

#endif // DEMOES_H

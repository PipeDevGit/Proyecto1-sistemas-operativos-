// ---------------------------------------------------------------------------
// VistasSistema.h - Presentacion del monitoreo de procesos y memoria
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Como se PINTAN los datos que devuelven MonitorProcesos y MonitorMemoria.
//   Se separo de main.cpp porque main ya carga con el bucle del menu, la linea
//   de comandos y el despacho, y porque estas tres vistas son las que mas
//   trabajo de formato tienen del proyecto.
//
// POR QUE ESTAS VISTAS SE CUIDAN TANTO
//   El requisito funcional 4 hace de la interfaz un entregable, y Usabilidad es
//   una de las cinco caracteristicas ISO evaluadas. Ademas, en la clase de la
//   Semana 10 el profesor mostro la herramienta que espera ver: un listado de
//   procesos, la memoria con su porcentaje de uso, y el detalle de un proceso
//   con su tiempo de usuario y su tiempo de sistema. Eso es exactamente lo que
//   se reproduce aca.
//
// Este archivo no tiene ningun #ifdef: consume las interfaces y no sabe de que
// plataforma vienen los datos.
// ---------------------------------------------------------------------------
#ifndef VISTASSISTEMA_H
#define VISTASSISTEMA_H

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "Consola.h"
#include "MonitorMemoria.h"
#include "MonitorProcesos.h"

namespace vistas {

// Recorta un texto a lo ancho de su columna. Sin esto, un nombre de proceso
// largo empuja el resto de la fila y descuadra la tabla entera.
inline std::string recortar(const std::string& s, size_t ancho) {
    if (s.size() <= ancho) return s;
    return s.substr(0, ancho - 1) + "~";
}

// -------------------------------------------------------------------------
// Memoria del sistema (requisito funcional 3)
// -------------------------------------------------------------------------
inline void memoria(const Estilo& e) {
    consola::titulo(e, "MEMORIA DEL SISTEMA ANFITRION");

    const MemoriaInfo m = sistema::leerMemoria();
    if (!m.ok) {
        consola::errorConSugerencia(e, "no se pudo leer la memoria: " + m.problema,
                                    "probá la opcion de fuentes para ver que mecanismos hay");
        return;
    }

    std::printf("  %-14s %14s\n", "Total:",      consola::formatearBytes(m.total).c_str());
    std::printf("  %-14s %14s\n", "Usada:",      consola::formatearBytes(m.usada).c_str());
    std::printf("  %-14s %14s\n", "Disponible:", consola::formatearBytes(m.disponible).c_str());

    std::printf("\n  ");
    consola::barra(e, m.fraccionUsada(), 34);
    std::printf("\n");

    if (m.haySwap) {
        const double frac = m.swapTotal == 0 ? 0.0
            : static_cast<double>(m.swapUsada) / static_cast<double>(m.swapTotal);
        std::printf("\n  %-14s %14s de %s\n", "Intercambio:",
                    consola::formatearBytes(m.swapUsada).c_str(),
                    consola::formatearBytes(m.swapTotal).c_str());
        std::printf("  ");
        consola::barra(e, frac, 34);
        std::printf("\n");
    }

    std::printf("\n  %sFuente: %s%s\n", e.gris(), m.fuente.c_str(), e.fin());
    std::printf("  %s'Disponible' es la estimacion del kernel de cuanta memoria puede%s\n",
                e.gris(), e.fin());
    std::printf("  %sentregar sin recurrir al area de intercambio. No es lo mismo que%s\n",
                e.gris(), e.fin());
    std::printf("  %s'libre': la cache de disco cuenta como usada pero se cede al instante.%s\n",
                e.gris(), e.fin());
}

// -------------------------------------------------------------------------
// Procesos activos (requisito funcional 2)
//
// Se ordenan por memoria residente descendente porque es el criterio del
// laboratorio 6 ('ps aux --sort=-%mem | head -5') y porque el proceso que mas
// memoria consume es casi siempre el que se esta buscando.
// -------------------------------------------------------------------------
inline void procesos(const Estilo& e, size_t cuantos, const std::string& preferido) {
    consola::titulo(e, "PROCESOS ACTIVOS DEL SISTEMA ANFITRION");

    std::vector<ProcesoInfo> lista;
    std::string fuente;
    const std::string err = sistema::listarProcesos(lista, fuente, preferido);

    if (!err.empty()) {
        consola::errorConSugerencia(e, "no se pudieron listar los procesos: " + err,
                                    "la herramienta probo todas sus fuentes y ninguna respondio");
        return;
    }

    const size_t total = lista.size();

    std::sort(lista.begin(), lista.end(),
              [](const ProcesoInfo& a, const ProcesoInfo& b) {
                  return a.memResidente > b.memResidente;
              });
    if (lista.size() > cuantos) lista.resize(cuantos);

    std::printf("  %s%8s %-22s %-3s %8s %8s %12s%s\n", e.negrita(),
                "PID", "Nombre", "Est", "%CPU", "%MEM", "Residente", e.fin());
    consola::regla(e, 74);

    for (const ProcesoInfo& p : lista) {
        // El estado se colorea porque es lo que primero se busca al diagnosticar:
        // R corriendo, Z zombi (el padre no recogio su salida), D bloqueado en
        // una espera de E/S no interrumpible.
        const char* col = (p.estado == 'R') ? e.verde()
                        : (p.estado == 'Z') ? e.rojo()
                        : (p.estado == 'D') ? e.ambar() : e.gris();

        std::printf("  %8ld %-22s %s%-3c%s %8.1f %8.1f %12s\n",
                    p.pid, recortar(p.nombre, 22).c_str(),
                    col, p.estado, e.fin(),
                    p.cpuPorcentaje, p.memPorcentaje,
                    consola::formatearBytes(p.memResidente).c_str());
    }

    consola::regla(e, 74);
    std::printf("  %s%zu procesos en total; se muestran los %zu de mayor memoria residente.%s\n",
                e.gris(), total, lista.size(), e.fin());
    std::printf("  %sFuente: %s%s\n", e.gris(), fuente.c_str(), e.fin());
    std::printf("  %sEstados: %sR%s corriendo  %sS%s durmiendo  %sD%s espera de E/S  %sZ%s zombi%s\n",
                e.gris(), e.verde(), e.gris(), e.gris(), e.gris(),
                e.ambar(), e.gris(), e.rojo(), e.gris(), e.fin());
}

// -------------------------------------------------------------------------
// Detalle de un proceso
//
// Es la vista que el profesor mostro en clase: memoria del proceso, tiempo de
// usuario y tiempo de sistema. La distincion entre los dos tiempos es
// justamente el limite entre modo usuario y modo kernel de la Unidad I.
// -------------------------------------------------------------------------
inline void detalleProceso(const Estilo& e, long pid) {
    std::vector<ProcesoInfo> lista;
    std::string fuente;
    const std::string err = sistema::listarProcesos(lista, fuente);
    if (!err.empty()) {
        consola::error(e, "no se pudo consultar el proceso: " + err);
        return;
    }

    // Se busca en la foto ya tomada en vez de volver a consultar: entre listar
    // y consultar el proceso puede haber terminado, y esta es la forma barata
    // de que el detalle corresponda al mismo instante que el listado.
    for (const ProcesoInfo& p : lista) {
        if (p.pid != pid) continue;

        std::printf("\n");
        std::printf("  %-22s %ld\n", "PID:", p.pid);
        std::printf("  %-22s %s\n", "Nombre:", p.nombre.c_str());
        std::printf("  %-22s %c\n", "Estado:", p.estado);
        std::printf("  %-22s %s (%.1f%% del total)\n", "Memoria residente:",
                    consola::formatearBytes(p.memResidente).c_str(), p.memPorcentaje);
        std::printf("  %-22s %.4f s\n", "Tiempo de usuario:", p.tiempoUsuario);
        std::printf("  %-22s %.4f s\n", "Tiempo de sistema:", p.tiempoSistema);
        std::printf("  %-22s %.4f s\n", "Tiempo de CPU total:",
                    p.tiempoUsuario + p.tiempoSistema);
        std::printf("  %-22s %.1f%%\n", "Uso de CPU promedio:", p.cpuPorcentaje);

        std::printf("\n  %sEl tiempo de usuario es el que el proceso paso ejecutando su propio%s\n",
                    e.gris(), e.fin());
        std::printf("  %scodigo; el de sistema, el que el kernel gasto atendiendo sus llamadas%s\n",
                    e.gris(), e.fin());
        std::printf("  %sal sistema. Un proceso con mucho tiempo de sistema hace mucha E/S.%s\n",
                    e.gris(), e.fin());
        std::printf("  %sEl uso de CPU es el promedio desde que arranco, igual que en ps.%s\n",
                    e.gris(), e.fin());
        return;
    }

    consola::errorConSugerencia(e, "no hay ningun proceso con PID " + std::to_string(pid),
                                "puede haber terminado entre el listado y esta consulta");
}

// -------------------------------------------------------------------------
// Consumo de la propia herramienta (resultado experimental obligatorio)
// -------------------------------------------------------------------------
inline void consumoPropio(const Estilo& e) {
    consola::titulo(e, "CONSUMO DE ESTA HERRAMIENTA");

    const ConsumoPropio c = sistema::consumoPropio();
    if (!c.ok) {
        consola::error(e, "no se pudo medir el consumo propio: " + c.problema);
        return;
    }

    std::printf("  %-24s %14s\n", "Memoria residente:",
                consola::formatearBytes(c.memResidente).c_str());
    std::printf("  %-24s %14s\n", "Memoria virtual:",
                consola::formatearBytes(c.memVirtual).c_str());
    std::printf("  %-24s %11.4f s\n", "Tiempo de usuario:", c.tiempoUsuario);
    std::printf("  %-24s %11.4f s\n", "Tiempo de sistema:", c.tiempoSistema);

    std::printf("\n  %sFuente: %s%s\n", e.gris(), c.fuente.c_str(), e.fin());
    std::printf("  %sLa residente es lo que ocupa en RAM de verdad; la virtual es el espacio%s\n",
                e.gris(), e.fin());
    std::printf("  %sde direcciones reservado, que incluye lo mapeado pero nunca tocado. Es la%s\n",
                e.gris(), e.fin());
    std::printf("  %sdistincion RSS contra VSZ de la Unidad IV. En esta herramienta las dos%s\n",
                e.gris(), e.fin());
    std::printf("  %scifras quedan cerca porque casi todo lo que mapea lo usa; la brecha se%s\n",
                e.gris(), e.fin());
    std::printf("  %sabre en programas que reservan mucho mas de lo que llegan a tocar.%s\n",
                e.gris(), e.fin());
    std::printf("\n  %sLos tiempos salen en cero cuando la herramienta lleva menos de un tic de%s\n",
                e.gris(), e.fin());
    std::printf("  %sreloj de CPU: /proc los informa en tics, y aca cada tic son 10 ms. No es%s\n",
                e.gris(), e.fin());
    std::printf("  %sque no haya consumido nada, es que esta por debajo de la resolucion.%s\n",
                e.gris(), e.fin());
}

// -------------------------------------------------------------------------
// De donde sale cada dato
//
// El enunciado obliga a declarar si se usan comandos del sistema o APIs. Esta
// vista lo muestra en vivo, y de paso deja ver la cadena de respaldo: si una
// fuente no esta disponible, aparece marcada.
// -------------------------------------------------------------------------
inline void fuentes(const Estilo& e) {
    consola::titulo(e, "FUENTES DE DATOS DE ESTA PLATAFORMA");
    std::printf("  %s%s%s\n\n", e.gris(), sistema::descripcionPlataforma().c_str(), e.fin());

    std::printf("  %s%-30s %-22s %s%s\n", e.negrita(),
                "Mecanismo", "Tipo", "Disponible", e.fin());
    consola::regla(e, 74);

    for (const IProveedorMemoria* p : sistema::proveedoresMemoria()) {
        const bool d = p->disponible();
        std::printf("  %-30s %-22s %s%s%s\n", p->nombre(), p->mecanismo(),
                    d ? e.verde() : e.rojo(), d ? "si" : "NO", e.fin());
    }
    for (const IProveedorProcesos* p : sistema::proveedoresProcesos()) {
        const bool d = p->disponible();
        std::printf("  %-30s %-22s %s%s%s\n", p->nombre(), p->mecanismo(),
                    d ? e.verde() : e.rojo(), d ? "si" : "NO", e.fin());
    }

    consola::regla(e, 74);
    std::printf("  %sSe prueban en ese orden hasta que una responda. Por eso el error%s\n",
                e.gris(), e.fin());
    std::printf("  %s\"comando no disponible\" no deja a la herramienta sin respuesta.%s\n",
                e.gris(), e.fin());
}

} // namespace vistas

#endif // VISTASSISTEMA_H

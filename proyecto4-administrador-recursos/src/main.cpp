// ---------------------------------------------------------------------------
// main.cpp - Interfaz de consola y despacho del menu
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   El requisito funcional 4 del enunciado: "interfaz de consola con menu de
//   navegacion entre las tres funciones". Aca vive el bucle del menu, la
//   lectura de la linea de comandos y el despacho; ninguna logica de archivos,
//   procesos ni memoria se implementa en este archivo.
//
// POR QUE LA LECTURA DE ENTRADA ES TAN DEFENSIVA
//   La Rubrica ISO/IEC 25010 evalua Usabilidad como "interfaz clara, mensajes
//   de ayuda/error comprensibles". Un menu que se rompe cuando el usuario
//   escribe una letra, o que entra en un bucle infinito al recibir EOF, es
//   justamente lo que esa fila castiga. Los tres casos -texto no numerico,
//   linea vacia y fin de entrada (Ctrl+D / Ctrl+Z)- se manejan explicitamente.
//
// ESTADO
//   Fase 6 de 8. Los cuatro requisitos funcionales estan implementados,
//   mas la auditoria de permisos (Unidad V) y la demostracion de E/S
//   (Unidad VI). Falta la verificacion en Windows y el documento IEEE.
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "Consola.h"
#include "DemoES.h"
#include "GestorArchivos.h"
#include "Permisos.h"
#include "MonitorMemoria.h"
#include "MonitorProcesos.h"
#include "Sandbox.h"
#include "VistasSistema.h"

namespace {

struct Config {
    std::string base  = "data/workspace";
    bool        color = true;
    bool        ascii = false;

    // Cuantos procesos se muestran en el listado. 15 entra en una pantalla sin
    // hacer scroll; el laboratorio 6 usa 5, y el total siempre se informa.
    size_t      cuantos = 15;

    // Fuente de procesos forzada. Vacia = probar en orden de preferencia.
    // Existe para poder comparar las dos rutas entre si, y para demostrar el
    // error de "comando no disponible" pidiendo la que no esta.
    std::string fuenteProcesos;

    // Demostracion de E/S. 400 repeticiones de una espera de 2 ms dan casi un
    // segundo de reloj: suficiente para que el consumo de CPU supere la
    // resolucion de 10 ms con que /proc informa los tiempos.
    int repeticionesES = 400;
    int retrasoES      = 2000;   // microsegundos que tarda el dispositivo
};

void ayuda(const char* prog) {
    std::printf("Administrador Simplificado de Recursos - Proyecto 4\n\n");
    std::printf("Uso: %s [opciones]\n\n", prog);
    std::printf("  --base RUTA     directorio de trabajo controlado (data/workspace)\n");
    std::printf("  --sin-color     desactiva los codigos ANSI de color\n");
    std::printf("  --ascii         sustituye los caracteres de dibujo UTF-8 por ASCII\n");
    std::printf("  --procesos N    cuantos procesos listar (15)\n");
    std::printf("  --fuente NOMBRE forzar la fuente de procesos, p. ej. /proc o ps\n");
    std::printf("  --es-repeticiones N  esperas por estrategia en la demo de E/S (400)\n");
    std::printf("  --es-retraso US      cuanto tarda el dispositivo simulado (2000)\n");
    std::printf("  --ayuda         muestra esta ayuda\n\n");
    std::printf("Con --sin-color y --ascii juntos, la salida es ASCII puro.\n");
}

// Devuelve false si hay que terminar (error de argumentos o --ayuda).
bool leerArgumentos(int argc, char** argv, Config& cfg, int& codigoSalida) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];

        if (a == "--ayuda" || a == "-h") {
            ayuda(argv[0]);
            codigoSalida = 0;
            return false;
        } else if (a == "--sin-color") {
            cfg.color = false;
        } else if (a == "--ascii") {
            cfg.ascii = true;
        } else if (a == "--base") {
            // Se distingue "falta el valor" de "opcion desconocida". En el
            // Proyecto 3 tratar los dos casos igual daba un mensaje que no
            // ayudaba a entender que se habia escrito mal.
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: --base necesita una ruta.\n");
                codigoSalida = 1;
                return false;
            }
            cfg.base = argv[++i];
        } else if (a == "--fuente") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: --fuente necesita un nombre.\n");
                codigoSalida = 1;
                return false;
            }
            cfg.fuenteProcesos = argv[++i];
        } else if (a == "--procesos") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: --procesos necesita un numero.\n");
                codigoSalida = 1;
                return false;
            }
            const long n = std::strtol(argv[++i], nullptr, 10);
            if (n <= 0) {
                std::fprintf(stderr, "Error: --procesos necesita un numero positivo.\n");
                codigoSalida = 1;
                return false;
            }
            cfg.cuantos = static_cast<size_t>(n);
        } else if (a == "--es-repeticiones" || a == "--es-retraso") {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "Error: %s necesita un numero.\n", a.c_str());
                codigoSalida = 1;
                return false;
            }
            const long n = std::strtol(argv[++i], nullptr, 10);
            if (n <= 0) {
                std::fprintf(stderr, "Error: %s necesita un numero positivo.\n", a.c_str());
                codigoSalida = 1;
                return false;
            }
            if (a == "--es-repeticiones") cfg.repeticionesES = static_cast<int>(n);
            else                          cfg.retrasoES      = static_cast<int>(n);
        } else {
            std::fprintf(stderr, "Error: opcion desconocida %s. Probá --ayuda.\n", a.c_str());
            codigoSalida = 1;
            return false;
        }
    }
    return true;
}

// Resultado de pedirle una linea al usuario.
enum class Lectura { Dato, Vacia, Fin };

Lectura leerLinea(const std::string& prompt, std::string& destino) {
    std::printf("%s", prompt.c_str());
    std::fflush(stdout);

    if (!std::getline(std::cin, destino)) return Lectura::Fin;   // EOF: Ctrl+D / Ctrl+Z

    // Se recortan espacios y el retorno de carro que deja un archivo con
    // finales de linea de Windows cuando la entrada viene redirigida.
    const std::string sobra = " \t\r\n";
    const size_t ini = destino.find_first_not_of(sobra);
    if (ini == std::string::npos) { destino.clear(); return Lectura::Vacia; }
    const size_t ult = destino.find_last_not_of(sobra);
    destino = destino.substr(ini, ult - ini + 1);
    return Lectura::Dato;
}

// Informa el resultado de una operacion con la marca de estado uniforme.
void reportar(const Estilo& e, const Operacion& op) {
    if (op.ok) consola::ok(e, op.detalle);
    else       consola::errorConSugerencia(e, op.detalle, op.sugerencia);
}

// Color del marcador de riesgo de una entrada.
const char* colorRiesgo(const Estilo& e, Riesgo r) {
    switch (r) {
        case Riesgo::Alto:  return e.rojo();
        case Riesgo::Medio: return e.ambar();
        default:            return "";
    }
}

// ---------------------------------------------------------------------------
// Vistas
// ---------------------------------------------------------------------------

void encabezado(const Estilo& e, const Sandbox& caja) {
    std::printf("\n");
    consola::recuadro(e, "ADMINISTRADOR SIMPLIFICADO DE RECURSOS   -   Proyecto 4");
    std::printf("  %sDirectorio de trabajo:%s %s\n",
                e.gris(), e.fin(), caja.base().string().c_str());
}

void menuPrincipal(const Estilo& e) {
    std::printf("\n");
    std::printf("   %s[1]%s  Gestion de archivos\n", e.cian(), e.fin());
    std::printf("   %s[2]%s  Monitoreo de procesos\n", e.cian(), e.fin());
    std::printf("   %s[3]%s  Monitoreo de memoria\n", e.cian(), e.fin());
    std::printf("   %s[4]%s  Auditoria de permisos\n", e.cian(), e.fin());
    std::printf("   %s[5]%s  Demostracion de E/S (polling vs. delegacion)\n",
                e.cian(), e.fin());
    std::printf("   %s[6]%s  Consumo de esta herramienta\n", e.cian(), e.fin());
    std::printf("   %s[7]%s  Fuentes de datos de esta plataforma\n", e.cian(), e.fin());
    std::printf("   %s[0]%s  Salir\n\n", e.cian(), e.fin());
}

// Listado tabular. Las columnas van con ancho fijo para que los datos queden
// alineados en vertical: una tabla desalineada fue exactamente la observacion
// que le costo un punto al Proyecto 1.
void verListado(const Estilo& e, const Sandbox& caja) {
    std::vector<EntradaArchivo> entradas;
    const Operacion op = gestor::listar(caja, entradas);

    consola::titulo(e, "CONTENIDO DEL DIRECTORIO DE TRABAJO");
    if (!op.ok) { reportar(e, op); return; }

    if (entradas.empty()) {
        std::printf("  %sEl directorio esta vacio.%s\n", e.gris(), e.fin());
        return;
    }

    std::printf("  %s%-5s %-4s %-10s %12s  %-16s %s%s\n", e.negrita(),
                "Tipo", "Oct", "Permisos", "Tamano", "Modificado", "Nombre", e.fin());
    consola::regla(e, 74);

    size_t conRiesgo = 0;
    for (const EntradaArchivo& en : entradas) {
        if (!en.legible) {
            std::printf("  %s%-5s %-4s %-10s %12s  %-16s %s  (%s)%s\n",
                        e.gris(), "?", "---", "---------", "-", "-",
                        en.nombre.c_str(), en.problema.c_str(), e.fin());
            continue;
        }

        // Igual que en la auditoria: donde el modelo de permisos no es el real,
        // no se marca riesgo. El octal se sigue mostrando porque es lo que
        // informa la biblioteca estandar; lo que no se emite es el juicio.
        const Riesgo   rg  = sistema::permisosSonReales()
                           ? permisos::evaluar(en.permisos, en.esDirectorio)
                           : Riesgo::Ninguno;
        const char*    col = colorRiesgo(e, rg);
        if (rg != Riesgo::Ninguno) ++conRiesgo;

        const std::string tam = en.esDirectorio ? "-" : consola::formatearBytes(en.tamano);

        std::printf("  %-5s %s%-4s %-10s%s %12s  %-16s %s%s%s\n",
                    en.esDirectorio ? "dir" : "arch",
                    col, permisos::octalTexto(en.permisos).c_str(),
                    permisos::rwx(en.permisos).c_str(), e.fin(),
                    tam.c_str(), en.fecha.c_str(),
                    en.nombre.c_str(),
                    rg != Riesgo::Ninguno ? "  <-- riesgo" : "", e.fin());
    }

    consola::regla(e, 74);
    std::printf("  %s%zu entrada%s", e.gris(), entradas.size(), entradas.size() == 1 ? "" : "s");
    if (conRiesgo > 0)
        std::printf("  ·  %s%zu con permisos de riesgo%s%s (ver opcion 4 del menu principal)",
                    e.rojo(), conRiesgo, e.fin(), e.gris());
    std::printf("%s\n", e.fin());
}

void verMetadatos(const Estilo& e, const Sandbox& caja) {
    consola::titulo(e, "METADATOS DE UN ARCHIVO");

    std::string nombre;
    if (leerLinea("  Nombre (Enter para volver): ", nombre) != Lectura::Dato) return;

    EntradaArchivo en;
    const Operacion op = gestor::metadatos(caja, nombre, en);
    std::printf("\n");
    if (!op.ok) { reportar(e, op); return; }

    const Riesgo rg = permisos::evaluar(en.permisos, en.esDirectorio);

    std::printf("  %-22s %s\n", "Nombre:", en.nombre.c_str());
    std::printf("  %-22s %s\n", "Tipo:", en.esDirectorio ? "directorio" : "archivo regular");
    if (!en.esDirectorio)
        std::printf("  %-22s %s (%ju bytes)\n", "Tamano:",
                    consola::formatearBytes(en.tamano).c_str(),
                    static_cast<uintmax_t>(en.tamano));
    std::printf("  %-22s %s\n", "Modificado:", en.fecha.c_str());
    std::printf("  %-22s %s%s  %s%s\n", "Permisos:",
                colorRiesgo(e, rg), permisos::octalTexto(en.permisos).c_str(),
                permisos::rwx(en.permisos).c_str(), e.fin());

    if (rg != Riesgo::Ninguno)
        std::printf("  %-22s %s%s%s\n", "Advertencia:", colorRiesgo(e, rg),
                    permisos::explicar(en.permisos, en.esDirectorio).c_str(), e.fin());

    std::printf("  %s%-22s los tres digitos son usuario, grupo y otros;%s\n",
                e.gris(), "", e.fin());
    std::printf("  %s%-22s 4 = leer, 2 = escribir, 1 = ejecutar%s\n", e.gris(), "", e.fin());
}

void hacerCrear(const Estilo& e, const Sandbox& caja) {
    consola::titulo(e, "CREAR ARCHIVO");

    std::string nombre;
    if (leerLinea("  Nombre (Enter para volver): ", nombre) != Lectura::Dato) return;

    std::string contenido;
    leerLinea("  Contenido (opcional): ", contenido);

    std::printf("\n");
    reportar(e, gestor::crear(caja, nombre, contenido));
}

void hacerEliminar(const Estilo& e, const Sandbox& caja) {
    consola::titulo(e, "ELIMINAR ARCHIVO");
    std::printf("  %sSolo se eliminan archivos y directorios vacios; no hay borrado en cascada.%s\n\n",
                e.gris(), e.fin());

    std::string nombre;
    if (leerLinea("  Nombre (Enter para volver): ", nombre) != Lectura::Dato) return;

    std::printf("\n");
    reportar(e, gestor::eliminar(caja, nombre));
}

// Auditoria de permisos: el equivalente explicado de 'find . -perm -o+w'.
void verAuditoria(const Estilo& e, const Sandbox& caja) {
    consola::titulo(e, "AUDITORIA DE PERMISOS");
    std::printf("  %sBusca entradas que cualquier usuario del sistema pueda modificar.%s\n",
                e.gris(), e.fin());
    std::printf("  %sEs el equivalente de  find . -perm -o+w  del laboratorio 5.%s\n\n",
                e.gris(), e.fin());

    // Antes de emitir cualquier veredicto, se comprueba si el modelo de
    // permisos de esta plataforma es el real. Donde no lo es, marcar archivos
    // como peligrosos seria inventar: la traduccion de la ACL a nueve bits deja
    // el bit de escritura para "otros" siempre puesto, y TODOS los archivos
    // normales saldrian marcados. Una alerta que salta siempre no es una alerta.
    if (!sistema::permisosSonReales()) {
        consola::aviso(e, "en esta plataforma no se puede auditar permisos con fiabilidad.");
        std::printf("\n  %sEl sistema usa listas de control de acceso (ACL), donde cada entrada%s\n",
                    e.gris(), e.fin());
        std::printf("  %sda permisos a un usuario o grupo concreto. Eso no cabe en los nueve%s\n",
                    e.gris(), e.fin());
        std::printf("  %sbits del modelo Unix, y std::filesystem::permissions() lo aproxima a%s\n",
                    e.gris(), e.fin());
        std::printf("  %spartir del atributo de solo lectura: devuelve 666 para cualquier%s\n",
                    e.gris(), e.fin());
        std::printf("  %sarchivo escribible y 444 para los de solo lectura.%s\n\n",
                    e.gris(), e.fin());
        std::printf("  %sSe comprobo creando un archivo normal: la herramienta lo veia como%s\n",
                    e.gris(), e.fin());
        std::printf("  %s666 (escribible por cualquiera) mientras icacls mostraba que solo%s\n",
                    e.gris(), e.fin());
        std::printf("  %stenian acceso SYSTEM, Administradores y el propio usuario.%s\n\n",
                    e.gris(), e.fin());
        std::printf("  %sPara auditar de verdad en este sistema:  icacls ARCHIVO%s\n",
                    e.gris(), e.fin());
        std::printf("  %sEl listado (opcion 1) sigue mostrando el octal, que es informacion%s\n",
                    e.gris(), e.fin());
        std::printf("  %sreal de la biblioteca estandar; lo que no se emite es el veredicto.%s\n",
                    e.gris(), e.fin());
        return;
    }

    std::vector<EntradaArchivo> riesgosas;
    size_t revisadas = 0;
    const Operacion op = gestor::auditar(caja, riesgosas, revisadas);
    if (!op.ok) { reportar(e, op); return; }

    if (riesgosas.empty()) {
        consola::ok(e, "ninguna entrada de riesgo entre las " +
                       std::to_string(revisadas) + " revisadas.");
        return;
    }

    std::printf("  %s%-5s %-4s %-10s %-26s %s%s\n", e.negrita(),
                "Tipo", "Oct", "Permisos", "Nombre", "Por que", e.fin());
    consola::regla(e, 74);

    for (const EntradaArchivo& en : riesgosas) {
        const Riesgo rg  = permisos::evaluar(en.permisos, en.esDirectorio);
        const char*  col = colorRiesgo(e, rg);
        std::printf("  %-5s %s%-4s %-10s%s %-26s %s%s%s\n",
                    en.esDirectorio ? "dir" : "arch",
                    col, permisos::octalTexto(en.permisos).c_str(),
                    permisos::rwx(en.permisos).c_str(), e.fin(),
                    en.nombre.c_str(),
                    col, permisos::explicar(en.permisos, en.esDirectorio).c_str(), e.fin());
    }

    consola::regla(e, 74);
    std::printf("  %s%zu de %zu entradas marcadas.%s\n",
                e.gris(), riesgosas.size(), revisadas, e.fin());
    std::printf("  %sPara corregir en Linux:  chmod o-w ARCHIVO%s\n", e.gris(), e.fin());
}

// Diagnostico del directorio controlado. Existe como opcion visible del menu
// -y no solo como test automatizado- porque es lo que permite demostrar en
// vivo, durante la Defensa Tecnica, que la herramienta no puede salirse de su
// directorio de trabajo.
void probarRuta(const Estilo& e, const Sandbox& caja) {
    consola::titulo(e, "VALIDACION DE RUTA CONTRA EL DIRECTORIO CONTROLADO");
    std::printf("  %sEscribí una ruta para ver si la herramienta la aceptaria.%s\n",
                e.gris(), e.fin());
    std::printf("  %sProba por ejemplo:  notas.txt  |  ../../etc/passwd  |  /etc/passwd%s\n\n",
                e.gris(), e.fin());

    std::string entrada;
    if (leerLinea("  Ruta (Enter para volver): ", entrada) != Lectura::Dato) return;

    const Resolucion r = caja.resolver(entrada);
    std::printf("\n");
    if (r.ok) {
        consola::ok(e, "aceptada");
        std::printf("  %sse resuelve a:%s %s\n", e.gris(), e.fin(), r.ruta.string().c_str());
    } else {
        consola::errorConSugerencia(e, "rechazada: " + r.motivo,
                                    "solo se admiten rutas dentro de " + caja.base().string());
    }
}

// Submenu de procesos. El detalle por PID vive aca y no dentro del listado
// porque pedir un PID exige una interaccion mas, y mezclarlas dejaria al
// listado -que es la operacion frecuente- esperando entrada en cada consulta.
void submenuProcesos(const Estilo& e, const Config& cfg) {
    for (;;) {
        vistas::procesos(e, cfg.cuantos, cfg.fuenteProcesos);

        std::printf("\n   %s[1]%s  Ver el detalle de un proceso por PID\n", e.cian(), e.fin());
        std::printf("   %s[2]%s  Volver a consultar\n", e.cian(), e.fin());
        std::printf("   %s[0]%s  Volver\n\n", e.cian(), e.fin());

        std::string opcion;
        const Lectura estado = leerLinea("  Opcion: ", opcion);
        if (estado == Lectura::Fin)   { std::printf("\n"); return; }
        if (estado == Lectura::Vacia) continue;

        if (opcion == "0") return;
        if (opcion == "2") continue;
        if (opcion != "1") {
            consola::errorConSugerencia(e, "opcion " + opcion + " no valida.",
                                        "escribi un numero del 0 al 2.");
            continue;
        }

        std::string texto;
        if (leerLinea("  PID: ", texto) != Lectura::Dato) continue;

        // El PID se valida entero antes de usarlo: escribir una letra aca no
        // debe terminar convertido en una consulta del proceso 0.
        char* fin = nullptr;
        const long pid = std::strtol(texto.c_str(), &fin, 10);
        if (fin == texto.c_str() || *fin != 0 || pid <= 0) {
            consola::errorConSugerencia(e, "'" + texto + "' no es un PID valido.",
                                        "el PID es un entero positivo de la columna PID");
            continue;
        }
        vistas::detalleProceso(e, pid);
    }
}

void submenuArchivos(const Estilo& e, const Sandbox& caja) {
    for (;;) {
        consola::titulo(e, "GESTION DE ARCHIVOS");
        std::printf("   %s[1]%s  Listar\n", e.cian(), e.fin());
        std::printf("   %s[2]%s  Crear\n", e.cian(), e.fin());
        std::printf("   %s[3]%s  Eliminar\n", e.cian(), e.fin());
        std::printf("   %s[4]%s  Ver metadatos\n", e.cian(), e.fin());
        std::printf("   %s[5]%s  Validar una ruta contra el directorio controlado\n",
                    e.cian(), e.fin());
        std::printf("   %s[0]%s  Volver\n\n", e.cian(), e.fin());

        std::string opcion;
        const Lectura estado = leerLinea("  Opcion: ", opcion);
        if (estado == Lectura::Fin)   { std::printf("\n"); return; }
        if (estado == Lectura::Vacia) continue;

        if      (opcion == "0") return;
        else if (opcion == "1") verListado(e, caja);
        else if (opcion == "2") hacerCrear(e, caja);
        else if (opcion == "3") hacerEliminar(e, caja);
        else if (opcion == "4") verMetadatos(e, caja);
        else if (opcion == "5") probarRuta(e, caja);
        else consola::errorConSugerencia(e, "opcion " + opcion + " no valida.",
                                         "escribí un numero del 0 al 5.");
    }
}

} // namespace

int main(int argc, char** argv) {
    Config cfg;
    int codigoSalida = 0;
    if (!leerArgumentos(argc, argv, cfg, codigoSalida)) return codigoSalida;

    Estilo e;
    e.ascii = cfg.ascii;
    // Si la consola no acepta secuencias ANSI se apaga el color solo, en vez
    // de escupir codigos de escape crudos por pantalla.
    e.color = cfg.color && consola::habilitarAnsi();

    Sandbox caja(cfg.base);
    if (!caja.listo()) {
        std::fprintf(stderr, "Error: %s\n", caja.error().c_str());
        return 1;
    }

    for (;;) {
        encabezado(e, caja);
        menuPrincipal(e);

        std::string opcion;
        const Lectura estado = leerLinea("  Opcion: ", opcion);

        // EOF (Ctrl+D en Linux, Ctrl+Z en Windows) se trata como salir. Sin
        // esto getline devuelve error indefinidamente y el bucle no termina
        // nunca: es el clasico bucle infinito al redirigir la entrada.
        if (estado == Lectura::Fin) {
            std::printf("\n");
            consola::aviso(e, "fin de la entrada, se cierra la herramienta.");
            return 0;
        }
        if (estado == Lectura::Vacia) continue;

        if (opcion == "0") {
            consola::ok(e, "hasta luego.");
            return 0;
        } else if (opcion == "1") {
            submenuArchivos(e, caja);
        } else if (opcion == "2") {
            submenuProcesos(e, cfg);
        } else if (opcion == "3") {
            vistas::memoria(e);
        } else if (opcion == "4") {
            verAuditoria(e, caja);
        } else if (opcion == "6") {
            vistas::consumoPropio(e);
        } else if (opcion == "7") {
            vistas::fuentes(e);
        } else if (opcion == "5") {
            demoes::ejecutar(e, cfg.repeticionesES, cfg.retrasoES);
        } else {
            consola::errorConSugerencia(e, "opcion " + opcion + " no valida.",
                                        "escribí un numero del 0 al 7.");
        }
    }
}

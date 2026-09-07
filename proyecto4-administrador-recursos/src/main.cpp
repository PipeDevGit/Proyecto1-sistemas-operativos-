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
//   Fase 1 de 8. El menu navega y el directorio controlado ya filtra rutas.
//   Las opciones marcadas [pendiente] se implementan en las fases 2, 3 y 6.
// ---------------------------------------------------------------------------
#include <cstdio>
#include <iostream>
#include <string>

#include "Consola.h"
#include "Sandbox.h"

namespace {

struct Config {
    std::string base  = "data/workspace";
    bool        color = true;
    bool        ascii = false;
};

void ayuda(const char* prog) {
    std::printf("Administrador Simplificado de Recursos - Proyecto 4\n\n");
    std::printf("Uso: %s [opciones]\n\n", prog);
    std::printf("  --base RUTA     directorio de trabajo controlado (data/workspace)\n");
    std::printf("  --sin-color     desactiva los codigos ANSI de color\n");
    std::printf("  --ascii         sustituye los caracteres de dibujo UTF-8 por ASCII\n");
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

void encabezado(const Estilo& e, const Sandbox& caja) {
    std::printf("\n");
    consola::recuadro(e, "ADMINISTRADOR SIMPLIFICADO DE RECURSOS   -   Proyecto 4");
    std::printf("  %sDirectorio de trabajo:%s %s\n",
                e.gris(), e.fin(), caja.base().string().c_str());
}

void menuPrincipal(const Estilo& e) {
    std::printf("\n");
    std::printf("   %s[1]%s  Gestion de archivos\n", e.cian(), e.fin());
    std::printf("   %s[2]%s  Monitoreo de procesos        %s[pendiente: fase 3]%s\n",
                e.cian(), e.fin(), e.gris(), e.fin());
    std::printf("   %s[3]%s  Monitoreo de memoria         %s[pendiente: fase 3]%s\n",
                e.cian(), e.fin(), e.gris(), e.fin());
    std::printf("   %s[4]%s  Auditoria de permisos        %s[pendiente: fase 2]%s\n",
                e.cian(), e.fin(), e.gris(), e.fin());
    std::printf("   %s[5]%s  Demostracion de E/S          %s[pendiente: fase 6]%s\n",
                e.cian(), e.fin(), e.gris(), e.fin());
    std::printf("   %s[6]%s  Consumo de esta herramienta  %s[pendiente: fase 3]%s\n",
                e.cian(), e.fin(), e.gris(), e.fin());
    std::printf("   %s[0]%s  Salir\n\n", e.cian(), e.fin());
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

void submenuArchivos(const Estilo& e, const Sandbox& caja) {
    for (;;) {
        consola::titulo(e, "GESTION DE ARCHIVOS");
        std::printf("   %s[1]%s  Listar         %s[pendiente: fase 2]%s\n",
                    e.cian(), e.fin(), e.gris(), e.fin());
        std::printf("   %s[2]%s  Crear          %s[pendiente: fase 2]%s\n",
                    e.cian(), e.fin(), e.gris(), e.fin());
        std::printf("   %s[3]%s  Eliminar       %s[pendiente: fase 2]%s\n",
                    e.cian(), e.fin(), e.gris(), e.fin());
        std::printf("   %s[4]%s  Ver metadatos  %s[pendiente: fase 2]%s\n",
                    e.cian(), e.fin(), e.gris(), e.fin());
        std::printf("   %s[5]%s  Validar una ruta contra el directorio controlado\n",
                    e.cian(), e.fin());
        std::printf("   %s[0]%s  Volver\n\n", e.cian(), e.fin());

        std::string opcion;
        const Lectura estado = leerLinea("  Opcion: ", opcion);
        if (estado == Lectura::Fin)   { std::printf("\n"); return; }
        if (estado == Lectura::Vacia) continue;

        if (opcion == "0") {
            return;
        } else if (opcion == "5") {
            probarRuta(e, caja);
        } else if (opcion == "1" || opcion == "2" || opcion == "3" || opcion == "4") {
            consola::aviso(e, "esta opcion se implementa en la fase 2.");
        } else {
            consola::errorConSugerencia(e, "opcion " + opcion + " no valida.",
                                        "escribí un numero del 0 al 5.");
        }
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
        } else if (opcion == "2" || opcion == "3" || opcion == "4" ||
                   opcion == "5" || opcion == "6") {
            consola::aviso(e, "esta opcion todavia no esta implementada.");
        } else {
            consola::errorConSugerencia(e, "opcion " + opcion + " no valida.",
                                        "escribí un numero del 0 al 6.");
        }
    }
}

// ---------------------------------------------------------------------------
// Consola.h - Presentacion en terminal
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Concentra todo el formato de salida -colores, recuadros, reglas, barras y
//   formato de tamanos- en un solo lugar, para que ningun modulo de logica
//   tenga que saber como se pinta nada.
//
// POR QUE EXISTE COMO MODULO APARTE
//   En el Proyecto 1 la nota de "Resultados experimentales" bajo a 3/4 por la
//   presentacion. Aca la interfaz ademas ES un entregable (requisito funcional
//   4 del enunciado) y "Usabilidad" es una de las cinco caracteristicas
//   evaluadas por la Rubrica ISO/IEC 25010. Separarlo permite invertir en la
//   presentacion sin ensuciar la logica.
//
// ALTERNATIVA DESCARTADA
//   Usar ncurses o una libreria de TUI. Se descarto porque agrega una
//   dependencia externa (el resto del curso se hizo con biblioteca estandar
//   pura), no existe igual en Windows, y el enunciado solo pide un menu de
//   navegacion, no una interfaz de pantalla completa.
//
// DOS MODOS DE DEGRADACION
//   --sin-color  quita los codigos ANSI    (terminal que no los interpreta)
//   --ascii      quita los caracteres UTF-8 (consola clasica de Windows)
//   Con los dos juntos, la salida no contiene un solo byte fuera de ASCII.
// ---------------------------------------------------------------------------
#ifndef CONSOLA_H
#define CONSOLA_H

#include <cstdio>
#include <cstdint>
#include <string>

namespace consola {

// Habilita la interpretacion de secuencias ANSI en la consola.
// En Linux no hace nada (siempre estan activas); en Windows 10+ hay que
// pedirla explicitamente con SetConsoleMode. Se declara aca pero se IMPLEMENTA
// en SistemaInfo.cpp, que es el unico archivo del proyecto con #ifdef.
// Devuelve false si no se pudo, y en ese caso main desactiva el color solo.
bool habilitarAnsi();

} // namespace consola

// ---------------------------------------------------------------------------
// Estilo: los dos interruptores de presentacion, resueltos una sola vez.
//
// Se pasa por referencia constante a todo el que imprima. Los metodos
// devuelven cadenas vacias cuando el modo esta apagado, asi el codigo que
// imprime es identico en los dos modos y no se llena de condicionales.
// ---------------------------------------------------------------------------
struct Estilo {
    bool color = true;   // codigos ANSI de color
    bool ascii = false;  // true = solo ASCII, sin caracteres de dibujo UTF-8

    const char* rojo()    const { return color ? "\033[31m" : ""; }
    const char* verde()   const { return color ? "\033[32m" : ""; }
    const char* ambar()   const { return color ? "\033[33m" : ""; }
    const char* cian()    const { return color ? "\033[36m" : ""; }
    const char* gris()    const { return color ? "\033[90m" : ""; }
    const char* negrita() const { return color ? "\033[1m"  : ""; }
    const char* fin()     const { return color ? "\033[0m"  : ""; }

    // Caracteres de dibujo. En modo ascii se sustituyen por equivalentes de
    // un solo byte para que la consola clasica de Windows no muestre basura.
    const char* horizontal() const { return ascii ? "-" : "\u2500"; }
    const char* lleno()      const { return ascii ? "#" : "\u2588"; }
    const char* vacio()      const { return ascii ? "." : "\u2591"; }
};

namespace consola {

// Repite un caracter n veces. Cuenta REPETICIONES, no bytes: los caracteres
// de dibujo UTF-8 ocupan 3 bytes cada uno pero una sola columna en pantalla,
// asi que contar bytes descuadraria todos los recuadros.
inline void repetir(const char* s, int n) {
    for (int i = 0; i < n; ++i) std::fputs(s, stdout);
}

inline void regla(const Estilo& e, int ancho) {
    std::fputs(e.gris(), stdout);
    repetir(e.horizontal(), ancho);
    std::printf("%s\n", e.fin());
}

// 74 es el ancho de las tablas de listado y auditoria: el subrayado del
// titulo tiene que medir lo mismo que la tabla que encabeza, o la vista
// queda visiblemente descuadrada.
inline void titulo(const Estilo& e, const char* texto, int ancho = 74) {
    std::printf("\n%s%s%s\n", e.negrita(), texto, e.fin());
    regla(e, ancho);
}

// Recuadro de encabezado. El ancho se calcula a partir del contenido real y
// no se fija por constante: en el Proyecto 3 un recuadro de ancho fijo se
// desbordaba cuando la ruta del archivo era larga y la cabecera quedaba
// descuadrada respecto de la tabla de abajo.
inline void recuadro(const Estilo& e, const std::string& texto, int anchoMinimo = 62) {
    int ancho = static_cast<int>(texto.size()) + 2;
    if (ancho < anchoMinimo) ancho = anchoMinimo;

    std::printf("%s+", e.gris());
    repetir("-", ancho);
    std::printf("+%s\n", e.fin());
    std::printf("%s|%s %s%-*s%s%s|%s\n", e.gris(), e.fin(), e.negrita(),
                ancho - 1, texto.c_str(), e.fin(), e.gris(), e.fin());
    std::printf("%s+", e.gris());
    repetir("-", ancho);
    std::printf("+%s\n", e.fin());
}

// Barra de proporcion: [########------] 53.2%
// El color cambia con el nivel porque un porcentaje suelto no comunica
// gravedad: verde hasta 70%, ambar hasta 90%, rojo por encima.
inline void barra(const Estilo& e, double fraccion, int ancho = 30) {
    if (fraccion < 0.0) fraccion = 0.0;
    if (fraccion > 1.0) fraccion = 1.0;

    const int llenos = static_cast<int>(fraccion * ancho + 0.5);
    const char* col = fraccion < 0.70 ? e.verde()
                    : (fraccion < 0.90 ? e.ambar() : e.rojo());

    std::printf("%s[%s%s", e.gris(), e.fin(), col);
    repetir(e.lleno(), llenos);
    std::printf("%s%s", e.fin(), e.gris());
    repetir(e.vacio(), ancho - llenos);
    std::printf("%s]%s %s%5.1f%%%s", e.gris(), e.fin(), col, fraccion * 100.0, e.fin());
}

// Tamanos en unidades legibles. Se usan multiplos de 1024 (KiB, MiB, GiB) y
// no de 1000 porque es lo que reportan /proc/meminfo y GlobalMemoryStatusEx,
// que son las fuentes de este proyecto; mezclarlos daria numeros que no
// cuadran con los de free ni con los del Administrador de tareas.
inline std::string formatearBytes(unsigned long long bytes) {
    const char* unidades[] = {"B", "KB", "MB", "GB", "TB"};
    double valor = static_cast<double>(bytes);
    int u = 0;
    while (valor >= 1024.0 && u < 4) { valor /= 1024.0; ++u; }

    char buf[64];
    std::snprintf(buf, sizeof(buf), (u == 0 ? "%.0f %s" : "%.1f %s"), valor, unidades[u]);
    return std::string(buf);
}

// Marca de estado uniforme para los mensajes al usuario. Tener una sola forma
// de decir "salio bien" / "salio mal" es parte de lo que la rubrica llama
// "mensajes de ayuda/error comprensibles".
inline void ok(const Estilo& e, const std::string& msg) {
    std::printf("  %s[ok]%s %s\n", e.verde(), e.fin(), msg.c_str());
}
inline void aviso(const Estilo& e, const std::string& msg) {
    std::printf("  %s[aviso]%s %s\n", e.ambar(), e.fin(), msg.c_str());
}
inline void error(const Estilo& e, const std::string& msg) {
    std::printf("  %s[error]%s %s\n", e.rojo(), e.fin(), msg.c_str());
}

// Un error del usuario que no dice que hacer es un error a medias. Esta forma
// obliga a acompanar el que paso con una sugerencia concreta.
inline void errorConSugerencia(const Estilo& e, const std::string& msg,
                               const std::string& sugerencia) {
    std::printf("  %s[error]%s %s\n", e.rojo(), e.fin(), msg.c_str());
    std::printf("  %s       %s%s\n", e.gris(), sugerencia.c_str(), e.fin());
}

} // namespace consola

#endif // CONSOLA_H

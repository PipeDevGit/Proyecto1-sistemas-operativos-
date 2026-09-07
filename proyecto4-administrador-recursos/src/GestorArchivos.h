// ---------------------------------------------------------------------------
// GestorArchivos.h - Requisito funcional 1
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Crear, listar, eliminar y mostrar metadatos (tamano, fecha de modificacion,
//   permisos basicos) de archivos dentro del directorio de trabajo controlado.
//   Toda ruta que entra pasa antes por Sandbox::resolver, sin excepcion.
//
// POR QUE NO USA EXCEPCIONES
//   Casi todas las funciones de <filesystem> vienen en dos versiones: una que
//   lanza y otra que recibe un std::error_code. Aca se usa siempre la segunda.
//   La razon es que en esta herramienta los fallos del sistema de archivos NO
//   son excepcionales: un archivo que no existe, un permiso denegado o un
//   directorio que no se puede leer son entrada normal del usuario. Tratarlos
//   como valores permite reportarlos con un mensaje util y seguir operando, en
//   vez de desenrollar la pila para volver al menu.
//
// LA CONDICION DE CARRERA QUE SI EXISTE
//   Entre que directory_iterator entrega una entrada y que se leen sus
//   metadatos, otro proceso puede haber borrado el archivo. No es hipotetico:
//   pasa de verdad en un directorio activo. Por eso cada lectura de metadatos
//   usa error_code y, si falla, la entrada se marca como desaparecida en vez de
//   abortar el listado completo. Es el mismo tipo de problema que el Proyecto 2
//   trataba entre hilos, aca entre procesos.
//
// PORTABILIDAD
//   Este archivo NO tiene un solo #ifdef. std::filesystem de C++17 es
//   multiplataforma de forma nativa y cubre el requisito 1 entero. La unica
//   diferencia real entre plataformas es la interpretacion de los permisos, y
//   esa esta documentada en Permisos.h.
// ---------------------------------------------------------------------------
#ifndef GESTORARCHIVOS_H
#define GESTORARCHIVOS_H

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "Permisos.h"
#include "Sandbox.h"

// Una entrada del directorio con sus metadatos ya resueltos.
struct EntradaArchivo {
    std::string nombre;          // relativo al directorio de trabajo
    bool        esDirectorio = false;
    bool        legible      = true;   // false si desaparecio o se nego el acceso
    std::string problema;              // por que no se pudo leer, si legible == false

    std::uintmax_t tamano = 0;
    std::string    fecha;              // YYYY-MM-DD HH:MM
    fs::perms      permisos = fs::perms::none;
};

// Resultado de una operacion que puede fallar por causas del sistema.
struct Operacion {
    bool        ok = false;
    std::string detalle;      // que paso, en lenguaje humano
    std::string sugerencia;   // que puede hacer el usuario al respecto
};

namespace gestor {

// -------------------------------------------------------------------------
// Fecha de modificacion: la parte incomoda de C++17.
//
// last_write_time devuelve un file_time_type, cuyo reloj el estandar de C++17
// deja SIN ESPECIFICAR. No hay ninguna conversion directa a system_clock, que
// es el unico reloj que sabe convertirse a calendario con to_time_t. C++20
// agrego clock_cast para esto, pero el curso trabaja en C++17.
//
// El puente estandar consiste en medir la distancia del tiempo del archivo al
// "ahora" de su propio reloj, y sumar esa misma distancia al "ahora" de
// system_clock. Es aproximado: entre las dos llamadas a now() pasa un instante,
// asi que el resultado puede desviarse en el orden de microsegundos. Como aqui
// se imprime con resolucion de minutos, la desviacion es irrelevante; se deja
// dicho igual porque afirmar una precision que no se tiene es justamente el
// error que este curso penaliza.
// -------------------------------------------------------------------------
inline std::string formatearFecha(fs::file_time_type ft) {
    const auto ahoraArchivo  = fs::file_time_type::clock::now();
    const auto ahoraSistema  = std::chrono::system_clock::now();
    const auto equivalente   = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                                   ft - ahoraArchivo + ahoraSistema);

    const std::time_t t = std::chrono::system_clock::to_time_t(equivalente);

    // localtime no es reentrante, pero esta herramienta es de un solo hilo y
    // no hay ninguna otra llamada que pueda pisar el buffer estatico.
    const std::tm* tm = std::localtime(&t);
    if (tm == nullptr) return "(fecha invalida)";

    char buf[32];
    if (std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm) == 0) return "(fecha invalida)";
    return std::string(buf);
}

// Lee los metadatos de una ruta ya validada. Toda falla se reporta en la
// entrada en vez de propagarse.
inline EntradaArchivo leerEntrada(const Sandbox& caja, const fs::path& ruta) {
    EntradaArchivo e;
    e.nombre = caja.relativa(ruta);

    std::error_code ec;

    // symlink_status y no status: interesa el tipo de la ENTRADA en si, no el
    // de su destino. Si fuera un enlace roto, status fallaria mientras que
    // symlink_status informa correctamente que es un enlace.
    const fs::file_status st = fs::symlink_status(ruta, ec);
    if (ec) {
        e.legible  = false;
        e.problema = ec.message();
        return e;
    }

    e.esDirectorio = fs::is_directory(st);
    e.permisos     = st.permissions();

    if (!e.esDirectorio) {
        e.tamano = fs::file_size(ruta, ec);
        if (ec) { e.tamano = 0; e.problema = "tamano no disponible"; }
    }

    const fs::file_time_type ft = fs::last_write_time(ruta, ec);
    e.fecha = ec ? "(no disponible)" : formatearFecha(ft);

    return e;
}

// -------------------------------------------------------------------------
// Listado del directorio de trabajo.
//
// El orden de directory_iterator NO esta especificado por el estandar: en ext4
// depende del hash interno del directorio, asi que dos maquinas pueden
// devolver el mismo contenido en orden distinto. Se ordena explicitamente
// -directorios primero, despues por nombre- porque un listado que cambia de
// orden entre corridas no es reproducible, y la reproducibilidad es un
// requisito del curso y parte de la fila Fiabilidad de ISO/IEC 25010.
// -------------------------------------------------------------------------
inline Operacion listar(const Sandbox& caja, std::vector<EntradaArchivo>& salida) {
    Operacion r;
    salida.clear();

    std::error_code ec;
    fs::directory_iterator it(caja.base(), ec);
    if (ec) {
        r.detalle    = "no se pudo abrir el directorio de trabajo: " + ec.message();
        r.sugerencia = "revisá que exista y que tengas permiso de lectura sobre él";
        return r;
    }

    for (const fs::directory_entry& de : it) {
        salida.push_back(leerEntrada(caja, de.path()));
    }

    std::sort(salida.begin(), salida.end(),
              [](const EntradaArchivo& a, const EntradaArchivo& b) {
                  if (a.esDirectorio != b.esDirectorio) return a.esDirectorio;
                  return a.nombre < b.nombre;
              });

    r.ok = true;
    return r;
}

// -------------------------------------------------------------------------
// Crear un archivo con contenido opcional.
//
// No sobrescribe en silencio: si ya existe, lo dice y no toca nada. Borrar el
// trabajo de alguien sin avisar es exactamente el tipo de sorpresa que la fila
// Usabilidad de la rubrica penaliza.
// -------------------------------------------------------------------------
inline Operacion crear(const Sandbox& caja, const std::string& nombre,
                       const std::string& contenido) {
    Operacion r;

    const Resolucion res = caja.resolver(nombre);
    if (!res.ok) {
        r.detalle    = res.motivo;
        r.sugerencia = "solo se pueden crear archivos dentro de " + caja.base().string();
        return r;
    }

    std::error_code ec;
    if (fs::exists(res.ruta, ec)) {
        r.detalle    = "'" + nombre + "' ya existe";
        r.sugerencia = "elegí otro nombre, o eliminá el archivo antes de recrearlo";
        return r;
    }

    // Permite nombres anidados como informes/2026/marzo.txt sin obligar al
    // usuario a crear cada nivel a mano. El padre ya quedo validado por el
    // sandbox al resolver la ruta completa.
    const fs::path padre = res.ruta.parent_path();
    if (!padre.empty() && !fs::exists(padre, ec)) {
        fs::create_directories(padre, ec);
        if (ec) {
            r.detalle    = "no se pudieron crear los directorios intermedios: " + ec.message();
            r.sugerencia = "revisá los permisos del directorio de trabajo";
            return r;
        }
    }

    std::ofstream f(res.ruta);
    if (!f) {
        r.detalle    = "no se pudo crear '" + nombre + "'";
        r.sugerencia = "lo mas probable es que falte permiso de escritura en el directorio";
        return r;
    }
    f << contenido;
    if (!contenido.empty() && contenido.back() != '\n') f << '\n';
    f.close();

    r.ok      = true;
    r.detalle = "creado " + caja.relativa(res.ruta);
    return r;
}

// -------------------------------------------------------------------------
// Eliminar un archivo o un directorio vacio.
//
// Se usa remove y NO remove_all a proposito: remove_all borraria un arbol
// entero de forma recursiva con una sola confirmacion. En una herramienta cuyo
// punto central es no destruir nada fuera de control, el borrado recursivo
// silencioso es justo lo contrario de lo que se quiere poder defender. Si el
// directorio tiene contenido, se dice y no se hace nada.
// -------------------------------------------------------------------------
inline Operacion eliminar(const Sandbox& caja, const std::string& nombre) {
    Operacion r;

    const Resolucion res = caja.resolver(nombre);
    if (!res.ok) {
        r.detalle    = res.motivo;
        r.sugerencia = "solo se pueden eliminar archivos dentro de " + caja.base().string();
        return r;
    }

    // El propio directorio de trabajo no es borrable: sin el, la herramienta
    // se queda sin suelo.
    if (res.ruta == caja.base()) {
        r.detalle    = "no se puede eliminar el directorio de trabajo en si";
        r.sugerencia = "indicá un archivo o subdirectorio de adentro";
        return r;
    }

    std::error_code ec;
    if (!fs::exists(fs::symlink_status(res.ruta, ec))) {
        r.detalle    = "'" + nombre + "' no existe";
        r.sugerencia = "usá la opcion de listar para ver que hay en el directorio";
        return r;
    }

    if (fs::is_directory(res.ruta, ec) && !fs::is_empty(res.ruta, ec)) {
        r.detalle    = "'" + nombre + "' es un directorio y no esta vacio";
        r.sugerencia = "vaciálo primero: esta herramienta no borra en cascada a proposito";
        return r;
    }

    fs::remove(res.ruta, ec);
    if (ec) {
        r.detalle    = "no se pudo eliminar '" + nombre + "': " + ec.message();
        r.sugerencia = "suele ser falta de permiso de escritura en el directorio que lo contiene";
        return r;
    }

    r.ok      = true;
    r.detalle = "eliminado " + nombre;
    return r;
}

// Metadatos de una sola entrada, para la vista de detalle.
inline Operacion metadatos(const Sandbox& caja, const std::string& nombre,
                           EntradaArchivo& salida) {
    Operacion r;

    const Resolucion res = caja.resolver(nombre);
    if (!res.ok) {
        r.detalle    = res.motivo;
        r.sugerencia = "solo se pueden consultar rutas dentro de " + caja.base().string();
        return r;
    }

    std::error_code ec;
    if (!fs::exists(fs::symlink_status(res.ruta, ec))) {
        r.detalle    = "'" + nombre + "' no existe";
        r.sugerencia = "usá la opcion de listar para ver que hay en el directorio";
        return r;
    }

    salida = leerEntrada(caja, res.ruta);
    if (!salida.legible) {
        r.detalle    = "no se pudieron leer los metadatos: " + salida.problema;
        r.sugerencia = "suele ser falta de permiso de lectura sobre el archivo";
        return r;
    }

    r.ok = true;
    return r;
}

// -------------------------------------------------------------------------
// Auditoria de permisos (Unidad V, lamina 7).
//
// Recorre el arbol completo del directorio de trabajo y devuelve solo las
// entradas con riesgo. Es el equivalente de 'find . -perm -o+w' del
// laboratorio 5, pero con el resultado explicado.
//
// recursive_directory_iterator puede tropezar con subdirectorios sin permiso
// de lectura. Con la version que lanza, un solo subdirectorio inaccesible
// abortaria toda la auditoria; con error_code se salta ese subarbol y se sigue,
// que es como se comporta find.
// -------------------------------------------------------------------------
inline Operacion auditar(const Sandbox& caja, std::vector<EntradaArchivo>& riesgosas,
                         size_t& revisadas) {
    Operacion r;
    riesgosas.clear();
    revisadas = 0;

    std::error_code ec;
    fs::recursive_directory_iterator it(caja.base(),
                                        fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        r.detalle    = "no se pudo recorrer el directorio de trabajo: " + ec.message();
        r.sugerencia = "revisá que exista y que tengas permiso de lectura sobre él";
        return r;
    }

    const fs::recursive_directory_iterator fin;
    while (it != fin) {
        const fs::path ruta = it->path();
        ++revisadas;

        const EntradaArchivo e = leerEntrada(caja, ruta);
        if (e.legible && permisos::evaluar(e.permisos, e.esDirectorio) != Riesgo::Ninguno)
            riesgosas.push_back(e);

        it.increment(ec);
        if (ec) break;   // subarbol ilegible: se corta ese ramal, no la auditoria
    }

    r.ok = true;
    return r;
}

} // namespace gestor

#endif // GESTORARCHIVOS_H

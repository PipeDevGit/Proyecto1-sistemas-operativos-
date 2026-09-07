// ---------------------------------------------------------------------------
// Sandbox.h - Directorio de trabajo controlado
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   El enunciado dice que la gestion de archivos ocurre "dentro de un
//   directorio de trabajo controlado". Este modulo es lo que hace que esa
//   frase sea verdad: traduce lo que escribe el usuario a una ruta absoluta y
//   se niega a devolverla si cae fuera del directorio base. Ninguna operacion
//   destructiva del programa toca el disco sin pasar por aca.
//
// POR QUE ES EL MODULO MAS IMPORTANTE DEL PROYECTO
//   Es la Unidad V aplicada. Una herramienta que lista y borra archivos y que
//   NO valida la ruta es exactamente el agujero que describe la lamina 7 de la
//   Unidad V ("Riesgos de una mala configuracion de permisos"). Un usuario que
//   escriba ../../etc/passwd no deberia poder salirse, y demostrarlo es mas
//   defendible que cualquier otra parte del codigo.
//
// LAS TRES TRAMPAS QUE ESTE ARCHIVO EVITA (cada una tiene su test)
//   1. Escape por ..    ->  data/workspace/../../etc/passwd
//   2. Ruta absoluta    ->  /etc/passwd
//   3. Prefijo de texto ->  un directorio hermano llamado "workspace_malo"
//                           empieza por la misma cadena que "workspace" y
//                           pasaria un filtro hecho con comparacion de texto.
//   Y una cuarta que sale gratis: los enlaces simbolicos que apuntan afuera,
//   porque weakly_canonical los resuelve antes de comparar.
// ---------------------------------------------------------------------------
#ifndef SANDBOX_H
#define SANDBOX_H

#include <algorithm>
#include <filesystem>
#include <string>
#include <system_error>

namespace fs = std::filesystem;

// Resultado de traducir lo que escribio el usuario a una ruta utilizable.
// Se devuelve un objeto y no se lanza excepcion porque una ruta rechazada no
// es un fallo del programa: es entrada normal del usuario que hay que
// reportarle con un mensaje claro y seguir.
struct Resolucion {
    bool        ok = false;
    fs::path    ruta;      // absoluta y ya resuelta; solo valida si ok
    std::string motivo;    // por que se rechazo, en lenguaje humano
};

class Sandbox {
public:
    // Fija el directorio base y lo crea si no existe. Si algo falla, el objeto
    // queda no-listo() y el mensaje concreto se lee con error(): asi main
    // puede avisar y salir con dignidad en vez de reventar en el constructor.
    explicit Sandbox(const fs::path& base) {
        std::error_code ec;

        fs::create_directories(base, ec);
        if (ec && !fs::exists(base)) {
            error_ = "no se pudo crear el directorio de trabajo '" + base.string()
                   + "': " + ec.message();
            return;
        }

        // canonical (no weakly_canonical) a proposito: el directorio BASE si
        // tiene que existir ya en este punto, y canonical resuelve los enlaces
        // simbolicos del propio base. Si el base fuera un enlace y no lo
        // resolvieramos, cada comparacion posterior mediria contra una ruta
        // distinta de la real y todo el filtro quedaria inservible.
        base_ = fs::canonical(base, ec);
        if (ec) {
            error_ = "no se pudo resolver el directorio de trabajo '" + base.string()
                   + "': " + ec.message();
            return;
        }

        base_  = sinBarraFinal(base_);
        listo_ = true;
    }

    bool               listo() const { return listo_; }
    const fs::path&    base()  const { return base_;  }
    const std::string& error() const { return error_; }

    // El corazon del modulo.
    Resolucion resolver(const std::string& entrada) const {
        Resolucion r;

        if (!listo_) { r.motivo = "el directorio de trabajo no esta disponible"; return r; }
        if (entrada.empty()) { r.motivo = "el nombre esta vacio"; return r; }

        const fs::path pedida(entrada);

        // Una ruta absoluta NO se rechaza de entrada, aunque seria mas corto.
        // Se resuelve tal cual y se somete al mismo filtro de contencion que
        // todo lo demas, por dos razones: una ruta absoluta que apunte dentro
        // del directorio de trabajo es legitima, y sobre todo porque asi el
        // rechazo de /etc/passwd lo produce el mecanismo real de contencion y
        // no un atajo. Es lo que se quiere poder demostrar en la defensa.
        const fs::path candidata = pedida.is_absolute() ? pedida : (base_ / pedida);

        std::error_code ec;

        // weakly_canonical y NO canonical: canonical exige que la ruta entera
        // exista y lanza si no, pero al CREAR un archivo el ultimo componente
        // todavia no existe. weakly_canonical resuelve el tramo que si existe
        // -incluidos sus enlaces simbolicos, que es lo que cierra la trampa 4-
        // y deja el resto en forma normal, colapsando los .. de camino.
        fs::path resuelta = fs::weakly_canonical(candidata, ec);
        if (ec) {
            r.motivo = "no se pudo resolver la ruta: " + ec.message();
            return r;
        }

        resuelta = sinBarraFinal(resuelta);

        if (!contenida(base_, resuelta)) {
            r.motivo = "'" + entrada + "' apunta fuera del directorio de trabajo";
            return r;
        }

        r.ok   = true;
        r.ruta = resuelta;
        return r;
    }

    // Ruta mostrada al usuario: relativa al base, para que el listado no
    // repita el prefijo absoluto en cada linea.
    std::string relativa(const fs::path& absoluta) const {
        std::error_code ec;
        const fs::path rel = fs::relative(absoluta, base_, ec);
        return ec ? absoluta.string() : rel.string();
    }

private:
    // "/a/b/" itera como {"/", "a", "b", ""}: la barra final produce un
    // componente vacio que descuadraria la comparacion de la funcion de
    // abajo. Se normaliza quitandola.
    static fs::path sinBarraFinal(fs::path p) {
        if (!p.empty() && p.filename().empty()) p = p.parent_path();
        return p;
    }

    // Verdadero si 'candidata' es 'base' o cuelga de 'base'.
    //
    // Se compara COMPONENTE A COMPONENTE con los iteradores de fs::path, no
    // por prefijo de texto. Con comparacion de texto,
    //     /home/felipe/workspace_malo
    // empieza por
    //     /home/felipe/workspace
    // y pasaria el filtro sin estar dentro. Recorriendo componentes, el tercer
    // componente es "workspace_malo" contra "workspace" y no coinciden.
    //
    // std::mismatch se detiene en el primer par distinto; que el iterador de
    // base haya llegado al final significa que base se agoto sin discrepancias,
    // es decir, que candidata empieza exactamente por base.
    static bool contenida(const fs::path& base, const fs::path& candidata) {
        const auto par = std::mismatch(base.begin(), base.end(),
                                       candidata.begin(), candidata.end());
        return par.first == base.end();
    }

    fs::path    base_;
    bool        listo_ = false;
    std::string error_;
};

#endif // SANDBOX_H

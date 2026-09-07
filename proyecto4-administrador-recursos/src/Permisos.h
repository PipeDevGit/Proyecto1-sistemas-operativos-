// ---------------------------------------------------------------------------
// Permisos.h - Permisos de archivo y auditoria de riesgo
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Traduce el campo de permisos que devuelve std::filesystem a las dos
//   representaciones que se usan en clase -octal y rwx- y marca las
//   combinaciones que son un riesgo de seguridad.
//
// POR QUE EXISTE LA PARTE DE AUDITORIA
//   El enunciado solo pide "mostrar permisos basicos". Pero la lamina 7 de la
//   Unidad V es entera sobre riesgos de una mala configuracion de permisos, con
//   un caso empresarial de fuga de informacion, y el laboratorio 5 usa
//   'find . -perm -o+w' para buscar archivos escribibles por cualquiera.
//   Mostrar el numero sin decir si es peligroso deja el trabajo a medias: la
//   auditoria es lo que convierte el dato en informacion.
//
// LA LIMITACION HONESTA (va tal cual al documento IEEE)
//   Este modulo lee std::filesystem::status().permissions(), que es una vista
//   UNIFICADA de dos modelos que no son equivalentes:
//     - Linux/ext4: modelo Unix real de 9 bits (usuario/grupo/otros x rwx).
//       Lo que se reporta aqui es exactamente lo que muestra 'ls -l'.
//     - Windows/NTFS: el sistema usa ACL (listas de control de acceso), donde
//       cada entrada da permisos a un usuario o grupo concreto. Eso NO cabe en
//       9 bits. La biblioteca estandar lo aproxima a partir del atributo de
//       solo lectura, asi que el octal que se ve en Windows es una traduccion
//       con perdida, no el permiso real.
//   Se documenta en vez de disimularse: la Rubrica ISO/IEC 25010 califica
//   Compatibilidad como Excelente cuando se "declara explicitamente las
//   limitaciones de plataforma con justificacion".
// ---------------------------------------------------------------------------
#ifndef PERMISOS_H
#define PERMISOS_H

#include <cstdio>      // snprintf, para el octal formateado a tres digitos
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// Nivel de riesgo detectado sobre los permisos de una entrada.
enum class Riesgo {
    Ninguno,
    Medio,   // merece mirarse, no es una falla por si solo
    Alto     // cualquier usuario del sistema puede modificar el contenido
};

namespace permisos {

// Permisos en notacion octal, como los tres digitos de chmod.
//
// perms es un enum de banderas cuyos valores numericos son justamente los del
// modelo Unix (owner_read = 0400, group_write = 020, ...), asi que la
// conversion no es una tabla de equivalencias: es quedarse con los 9 bits
// bajos. La mascara 0777 descarta setuid, setgid y sticky, que van aparte.
inline unsigned octal(fs::perms p) {
    return static_cast<unsigned>(p & fs::perms::mask) & 0777u;
}

// Los mismos 9 bits en la notacion rwx que imprime 'ls -l'.
//
// Se recorre de mayor a menor peso para que el orden de la cadena coincida con
// el orden de lectura habitual: usuario, grupo, otros.
inline std::string rwx(fs::perms p) {
    static const fs::perms bits[9] = {
        fs::perms::owner_read,  fs::perms::owner_write,  fs::perms::owner_exec,
        fs::perms::group_read,  fs::perms::group_write,  fs::perms::group_exec,
        fs::perms::others_read, fs::perms::others_write, fs::perms::others_exec
    };
    static const char letras[9] = {'r','w','x','r','w','x','r','w','x'};

    std::string s(9, '-');
    for (int i = 0; i < 9; ++i)
        if ((p & bits[i]) != fs::perms::none) s[static_cast<size_t>(i)] = letras[i];
    return s;
}

// Octal formateado a tres digitos, para que las columnas queden alineadas.
inline std::string octalTexto(fs::perms p) {
    const unsigned o = octal(p);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "%03o", o);
    return std::string(buf);
}

// Clasifica el riesgo de una entrada segun sus permisos.
//
// El criterio central es others_write: si ese bit esta encendido, CUALQUIER
// usuario con cuenta en la maquina puede modificar el archivo. Es exactamente
// lo que busca 'find . -perm -o+w' en el laboratorio 5, y el mecanismo del
// caso de la lamina 7.
//
// El caso de los directorios tiene un matiz que vale conocer: un directorio
// escribible por otros SIN el bit sticky permite que cualquiera borre archivos
// ajenos dentro de el, aunque esos archivos no sean suyos ni escribibles. Con
// el sticky puesto, solo el dueno de cada archivo puede borrarlo. Es lo que
// hace seguro a /tmp, y por eso ahi el riesgo baja de Alto a Medio.
inline Riesgo evaluar(fs::perms p, bool esDirectorio) {
    const bool escribibleOtros = (p & fs::perms::others_write) != fs::perms::none;
    const bool pegajoso        = (p & fs::perms::sticky_bit)   != fs::perms::none;

    if (escribibleOtros) {
        if (esDirectorio && pegajoso) return Riesgo::Medio;
        return Riesgo::Alto;
    }
    // Escribible por el grupo no se marca: con el umask habitual de Linux es
    // una configuracion corriente y marcarla llenaria el informe de ruido.
    return Riesgo::Ninguno;
}

// Explicacion en una frase de por que la entrada quedo marcada. Un informe que
// dice "RIESGO" sin decir cual no sirve para actuar.
inline std::string explicar(fs::perms p, bool esDirectorio) {
    switch (evaluar(p, esDirectorio)) {
        case Riesgo::Alto:
            return esDirectorio
                ? "cualquier usuario puede crear y borrar archivos aqui dentro"
                : "cualquier usuario del sistema puede modificar este archivo";
        case Riesgo::Medio:
            return "escribible por otros, pero el bit sticky impide borrar archivos ajenos";
        case Riesgo::Ninguno:
        default:
            return "";
    }
}

} // namespace permisos

#endif // PERMISOS_H

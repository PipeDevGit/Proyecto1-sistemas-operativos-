// ---------------------------------------------------------------------------
// MonitorProcesos.h - Requisito funcional 2
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Declara COMO se pide la lista de procesos activos del sistema anfitrion.
//   Igual que MonitorMemoria.h, es solo el contrato: las implementaciones
//   viven en SistemaInfo.cpp, el unico archivo con #ifdef.
//
// LAS DOS RUTAS, Y POR QUE ESTAN LAS DOS
//   El enunciado dice que se puede "invocar comandos del SO (ps/tasklist) y
//   parsear su salida, o usar APIs del sistema", y exige declarar cual. Se
//   implementan ambas porque tenerlas juntas da tres cosas que por separado no
//   se consiguen:
//     1. El error "comando no disponible" que el enunciado obliga a manejar se
//        puede DEMOSTRAR de verdad, alterando el PATH y viendo actuar el
//        respaldo, en vez de solo afirmarse.
//     2. Se pueden medir. Leer /proc no crea ningun proceso; invocar 'ps'
//        implica fork + exec + un interprete de comandos + parseo de texto. La
//        diferencia en microsegundos es la comparacion cuantitativa explicita
//        que pide la Rubrica 1.
//     3. La pregunta de defensa "de donde saca tu programa la informacion de
//        procesos" se responde mostrando las dos y explicando el costo de cada
//        una, en vez de con una sola frase.
//
// SOBRE EL PORCENTAJE DE CPU
//   Es el promedio desde que arranco el proceso, no el instantaneo: tiempo de
//   CPU acumulado dividido por el tiempo que lleva vivo. Es exactamente lo que
//   reporta la columna %CPU de 'ps aux', y por eso los dos numeros se pueden
//   contrastar. El instantaneo exigiria dos muestras separadas en el tiempo.
//   Un valor mayor a 100 no es un error: en una maquina con varios nucleos, un
//   proceso con varios hilos puede acumular mas segundos de CPU que de reloj.
// ---------------------------------------------------------------------------
#ifndef MONITORPROCESOS_H
#define MONITORPROCESOS_H

#include <string>
#include <vector>

// Un proceso del sistema anfitrion.
struct ProcesoInfo {
    long        pid = 0;
    std::string nombre;

    // Estado en la convencion de Linux, que es la que se usa en clase:
    //   R corriendo   S durmiendo   D durmiendo sin interrumpir (espera de E/S)
    //   Z zombi       T detenido    ? desconocido o no informado por la fuente
    char estado = '?';

    double cpuPorcentaje  = 0.0;   // promedio desde el arranque del proceso
    double memPorcentaje  = 0.0;   // residente sobre el total del sistema

    unsigned long long memResidente = 0;   // bytes realmente en RAM (RSS)
    double             tiempoUsuario = 0.0; // segundos de CPU en modo usuario
    double             tiempoSistema = 0.0; // segundos de CPU dentro del kernel
};

// Consumo de la propia herramienta mientras opera. Es un resultado
// experimental obligatorio del enunciado, y se mide con el mismo mecanismo que
// usa el monitoreo del resto del proyecto: en Linux, leyendo /proc.
struct ConsumoPropio {
    bool        ok = false;
    std::string problema;
    std::string fuente;

    unsigned long long memResidente = 0;   // lo que ocupa en RAM ahora mismo

    // El segundo numero NO significa lo mismo en las dos plataformas, y por eso
    // viene con su etiqueta: en Linux es VmSize, el espacio de direcciones
    // reservado; en Windows es PrivateUsage, el compromiso de memoria del
    // proceso. Son parientes, no equivalentes. Llamarlos igual a los dos e
    // imprimir "memoria virtual" en ambos casos seria afirmar algo falso.
    unsigned long long memSegunda    = 0;
    std::string        etiquetaSegunda;    // como se llama ese numero aqui
    std::string        explicaSegunda;     // que mide, en una frase

    double             tiempoUsuario = 0.0;
    double             tiempoSistema = 0.0;
};

// Contrato que cumple cada forma de listar los procesos.
class IProveedorProcesos {
public:
    virtual ~IProveedorProcesos() {}

    // Nombre exacto del mecanismo, tal como se declara en el IEEE:
    // "/proc", "ps", "CreateToolhelp32Snapshot", "tasklist".
    virtual const char* nombre() const = 0;

    // "archivo del sistema", "comando externo" o "API del sistema".
    virtual const char* mecanismo() const = 0;

    virtual bool disponible() const = 0;

    // Llena 'salida' y devuelve cadena vacia si todo fue bien; si no, el
    // motivo del fallo. No se usan excepciones por la misma razon que en
    // GestorArchivos: que un comando no este instalado es una condicion
    // esperable, no excepcional.
    virtual std::string listar(std::vector<ProcesoInfo>& salida) const = 0;
};

namespace sistema {

// Proveedores de procesos de ESTA plataforma, en orden de preferencia.
// Los punteros son a objetos estaticos: no hay que liberarlos.
const std::vector<const IProveedorProcesos*>& proveedoresProcesos();

// Lista los procesos probando los proveedores en orden hasta que uno responda.
// Si 'preferido' no esta vacio, se intenta primero el que tenga ese nombre.
// 'fuenteUsada' recibe el nombre del proveedor que finalmente funciono.
std::string listarProcesos(std::vector<ProcesoInfo>& salida,
                           std::string& fuenteUsada,
                           const std::string& preferido = "");

// Consumo de recursos de esta misma herramienta.
ConsumoPropio consumoPropio();

// Si el modelo de permisos de esta plataforma se corresponde con lo que
// devuelve std::filesystem::permissions(), o si esa vista es una traduccion
// con perdida en la que no se puede basar un juicio de seguridad.
//
// En Linux es cierto: los nueve bits son los reales, los mismos que muestra
// 'ls -l'. En Windows es falso: el sistema usa ACL y la biblioteca estandar
// devuelve 666 para cualquier archivo escribible y 444 para los de solo
// lectura, derivandolo del atributo de solo lectura sin mirar la ACL.
bool permisosSonReales();

// PID de este mismo proceso. Existe en la interfaz -y no se llama a getpid()
// donde haga falta- para que ningun otro archivo del proyecto necesite un
// #ifdef: getpid() es de <unistd.h> y en Windows el equivalente es
// GetCurrentProcessId(), de <windows.h>.
long pidPropio();

// Nombre legible del sistema operativo y del compilador con que se construyo.
// Sirve para que el informe experimental declare su entorno sin que el usuario
// tenga que transcribirlo a mano.
std::string descripcionPlataforma();

} // namespace sistema

#endif // MONITORPROCESOS_H

// ---------------------------------------------------------------------------
// MonitorMemoria.h - Requisito funcional 3
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Declara COMO se pide la memoria del sistema anfitrion, sin decir de donde
//   sale. La interfaz IProveedorMemoria es el contrato; cada plataforma aporta
//   sus implementaciones en SistemaInfo.cpp, que es el unico archivo del
//   proyecto con #ifdef.
//
// POR QUE UNA INTERFAZ Y NO UNA FUNCION SUELTA
//   Es el patron Strategy: una interfaz, varias implementaciones
//   intercambiables en tiempo de ejecucion. Aporta tres cosas concretas:
//     1. La misma plataforma puede ofrecer VARIAS rutas. En Linux la memoria se
//        puede leer de /proc/meminfo (un archivo) o de la salida de 'free -b'
//        (un comando externo). El enunciado autoriza las dos y pide declarar
//        cual se usa; teniendo ambas se puede declarar y ademas comparar.
//     2. Da un plan de respaldo real. Si la ruta preferida falla -por ejemplo
//        porque el comando no esta instalado, que es uno de los tres errores
//        que el enunciado exige manejar- se pasa a la siguiente y la
//        herramienta sigue respondiendo.
//     3. Agregar una plataforma es escribir una implementacion mas, sin tocar
//        ni el menu ni la presentacion.
//
// ADVERTENCIA SOBRE "MEMORIA USADA"
//   No hay una sola definicion. En Linux, 'total - libre' da un numero enorme y
//   enganoso, porque el kernel usa como cache de disco toda la RAM que nadie
//   reclama, y esa cache se libera al instante si un proceso la necesita. Por
//   eso se usa MemAvailable, que es la estimacion del propio kernel de cuanta
//   memoria puede entregarse sin tocar el area de intercambio. Es el mismo
//   numero que reporta la columna 'available' de 'free'.
// ---------------------------------------------------------------------------
#ifndef MONITORMEMORIA_H
#define MONITORMEMORIA_H

#include <string>
#include <vector>

// Fotografia del estado de la memoria del sistema, en bytes.
struct MemoriaInfo {
    bool        ok = false;
    std::string problema;    // por que fallo, si ok == false
    std::string fuente;      // de donde salio el dato; va al informe

    unsigned long long total      = 0;
    unsigned long long disponible = 0;
    unsigned long long usada      = 0;   // total - disponible

    bool               haySwap    = false;
    unsigned long long swapTotal  = 0;
    unsigned long long swapUsada  = 0;

    // Fraccion de memoria usada, entre 0 y 1. Se calcula aca y no en la vista
    // para que el informe y la pantalla no puedan discrepar.
    double fraccionUsada() const {
        return total == 0 ? 0.0 : static_cast<double>(usada) / static_cast<double>(total);
    }
};

// Contrato que cumple cada forma de averiguar la memoria del sistema.
class IProveedorMemoria {
public:
    virtual ~IProveedorMemoria() {}

    // Nombre exacto del mecanismo, tal como debe declararse en el documento
    // IEEE: "/proc/meminfo", "free -b", "GlobalMemoryStatusEx".
    virtual const char* nombre() const = 0;

    // Categoria del mecanismo: archivo del sistema, comando externo o API.
    // El enunciado pide declarar cual se usa, y esto lo hace explicito.
    virtual const char* mecanismo() const = 0;

    // Si este proveedor puede usarse en la maquina actual. Un comando puede
    // no estar instalado; un archivo de /proc puede no existir.
    virtual bool disponible() const = 0;

    virtual MemoriaInfo leer() const = 0;
};

namespace sistema {

// Proveedores de memoria de ESTA plataforma, en orden de preferencia.
// Implementado en SistemaInfo.cpp. Los punteros apuntan a objetos estaticos
// con duracion de todo el programa: quien los recibe no debe liberarlos.
const std::vector<const IProveedorMemoria*>& proveedoresMemoria();

// Lee la memoria probando los proveedores en orden hasta que uno responda.
// Si 'preferido' no esta vacio, se intenta primero el que tenga ese nombre.
MemoriaInfo leerMemoria(const std::string& preferido = "");

} // namespace sistema

#endif // MONITORMEMORIA_H

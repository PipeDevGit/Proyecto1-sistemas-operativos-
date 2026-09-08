// ---------------------------------------------------------------------------
// SistemaInfo.cpp - EL UNICO ARCHIVO DEL PROYECTO CON #ifdef
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   C++ no tiene API estandar para consultar los procesos ni la memoria del
//   sistema, asi que esa parte no puede escribirse de forma portable. Todo lo
//   que depende del sistema operativo vive aca dentro y se expone al resto del
//   programa por las interfaces de MonitorProcesos.h y MonitorMemoria.h, que
//   no contienen un solo condicional de plataforma.
//
// POR QUE UN SOLO ARCHIVO Y NO #ifdef REPARTIDOS
//   1. Es lo que la Rubrica 1 llama "codigo modular", y es facil de defender:
//      se puede senalar un archivo y decir "la portabilidad esta toda aqui".
//   2. <windows.h> define macros muy invasivas (entre ellas min y max) que
//      rompen codigo de la biblioteca estandar. Manteniendolo en una unica
//      unidad de traduccion, esa contaminacion no sale de este archivo.
//   3. Anadir una tercera plataforma seria tocar solo este archivo.
//
// MAPA DEL ARCHIVO
//   1. Utilidades comunes a las dos plataformas (ejecutar un comando, partir
//      texto en campos).
//   2. Implementaciones de Linux: /proc/meminfo, free, /proc/[pid]/stat, ps.
//   3. Implementaciones de Windows: GlobalMemoryStatusEx,
//      CreateToolhelp32Snapshot, tasklist.
//   4. Las fabricas que el resto del programa consume.
//
// NOTA SOBRE habilitarAnsi()
//   Encender el modo VT de la consola es un asunto de presentacion y
//   conceptualmente pertenece a Consola.h. Vive aqui igual porque es codigo
//   especifico de Windows, y la regla de que ningun otro archivo tenga #ifdef
//   pesa mas que la pureza tematica.
// ---------------------------------------------------------------------------
#include "AnalisisProcFS.h"
#include "Consola.h"
#include "MonitorMemoria.h"
#include "MonitorProcesos.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
  // WIN32_LEAN_AND_MEAN recorta lo que arrastra <windows.h>; NOMINMAX evita que
  // defina min y max como macros, que rompen la biblioteca estandar.
  //
  // Los dos van con guarda porque la biblioteca estandar de MSYS2 ya define
  // NOMINMAX por su cuenta en os_defines.h: definirlo a secas compila igual
  // pero emite una advertencia de redefinicion, y el proyecto se entrega sin
  // ninguna. Solo se ve compilando en Windows.
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <psapi.h>
  #include <tlhelp32.h>
  #define ABRIR_TUBERIA  _popen
  #define CERRAR_TUBERIA _pclose
#else
  #include <sys/utsname.h>
  #include <unistd.h>
  #define ABRIR_TUBERIA  popen
  #define CERRAR_TUBERIA pclose
#endif

// ===========================================================================
// 1. UTILIDADES COMUNES
// ===========================================================================
namespace {

struct SalidaComando {
    bool        ok = false;
    std::string texto;
    int         estado = -1;    // codigo de salida del comando
};

// Ejecuta un comando y captura su salida estandar.
//
// popen crea un proceso hijo con una tuberia hacia su salida. Es la ruta que
// el enunciado autoriza como alternativa a las APIs del sistema, y tiene un
// costo que conviene tener presente: fork + exec de un interprete de comandos,
// mas el parseo del texto que devuelve. Leer /proc, en cambio, no crea ningun
// proceso. Esa diferencia se mide en la fase de experimentos.
//
// El detalle que importa para el manejo de errores: si el comando no existe,
// popen NO falla. El interprete de comandos arranca igual, no encuentra el
// programa y termina con codigo 127. Por eso no alcanza con comprobar que el
// puntero no sea nulo: hay que mirar el estado que devuelve pclose.
SalidaComando ejecutar(const std::string& comando) {
    SalidaComando r;

    FILE* tuberia = ABRIR_TUBERIA(comando.c_str(), "r");
    if (tuberia == nullptr) {
        r.texto = "no se pudo crear el proceso hijo";
        return r;
    }

    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), tuberia) != nullptr) r.texto += buffer;

    const int cierre = CERRAR_TUBERIA(tuberia);
#ifdef _WIN32
    r.estado = cierre;
#else
    // pclose devuelve el estado de espera completo, no el codigo de salida.
    // WEXITSTATUS extrae el codigo real de los bits que corresponden.
    r.estado = (cierre == -1) ? -1 : WEXITSTATUS(cierre);
#endif

    r.ok = (r.estado == 0);
    if (!r.ok && r.estado == 127) r.texto = "el comando no esta disponible en el sistema";
    return r;
}

// Comprueba si un programa existe y es ejecutable, sin correrlo de verdad.
// El resultado se recuerda porque no cambia durante la vida del proceso y
// consultarlo implica crear un proceso hijo.
bool comandoExiste(const char* programa) {
#ifdef _WIN32
    const std::string sonda = std::string("where ") + programa + " >nul 2>&1";
#else
    const std::string sonda = std::string("command -v ") + programa + " >/dev/null 2>&1";
#endif
    return ejecutar(sonda).ok;
}

// Convierte a numero sin lanzar. Las fuentes de este archivo son texto que
// produce el sistema operativo, y un campo inesperado no debe tumbar la
// herramienta: se descarta esa entrada y se sigue con las demas.
//
// Esta si la usan las dos plataformas: Linux para /proc y Windows para la
// salida de tasklist.
bool aEntero(const std::string& s, unsigned long long& destino) {
    if (s.empty()) return false;
    char* fin = nullptr;
    const unsigned long long v = std::strtoull(s.c_str(), &fin, 10);
    if (fin == s.c_str()) return false;
    destino = v;
    return true;
}

} // namespace

// ===========================================================================
// 2. IMPLEMENTACIONES DE LINUX
// ===========================================================================
#ifndef _WIN32
namespace {

// Parte una linea en campos separados por espacios en blanco.
//
// Vive aqui dentro y no en las utilidades comunes porque solo la usan los
// proveedores de Linux: en la seccion comun quedaba definida y sin usar al
// compilar en Windows, y -Wunused-function la reportaba. Se descubrio al
// compilar por primera vez con g++ 16.2.0 en Windows; desde Linux es invisible.
std::vector<std::string> campos(const std::string& linea) {
    std::vector<std::string> v;
    std::istringstream ss(linea);
    std::string c;
    while (ss >> c) v.push_back(c);
    return v;
}

// Igual que campos(): solo la usa el proveedor 'ps', que es de Linux.
bool aReal(const std::string& s, double& destino) {
    if (s.empty()) return false;
    char* fin = nullptr;
    const double v = std::strtod(s.c_str(), &fin);
    if (fin == s.c_str()) return false;
    destino = v;
    return true;
}

// -------------------------------------------------------------------------
// Memoria por /proc/meminfo
//
// /proc es un sistema de archivos VIRTUAL: no ocupa disco. Cada lectura la
// atiende el kernel generando el texto en el momento, asi que lo que se lee es
// el estado real de ese instante. Por eso leerlo es tan barato comparado con
// invocar un comando: no hay proceso nuevo, solo una llamada de lectura.
//
// El formato son lineas "Clave:  valor kB". Los valores vienen en kibibytes,
// de ahi la multiplicacion por 1024.
// -------------------------------------------------------------------------
class ProveedorMeminfo : public IProveedorMemoria {
public:
    const char* nombre()    const override { return "/proc/meminfo"; }
    const char* mecanismo() const override { return "archivo del sistema"; }

    bool disponible() const override {
        std::ifstream f("/proc/meminfo");
        return f.good();
    }

    MemoriaInfo leer() const override {
        MemoriaInfo m;
        m.fuente = nombre();

        std::ifstream f("/proc/meminfo");
        if (!f) {
            m.problema = "no se pudo abrir /proc/meminfo";
            return m;
        }

        unsigned long long swapTotal = 0, swapLibre = 0;
        bool vioTotal = false, vioDisponible = false;

        std::string linea;
        while (std::getline(f, linea)) {
            const std::vector<std::string> c = campos(linea);
            if (c.size() < 2) continue;

            unsigned long long valor = 0;
            if (!aEntero(c[1], valor)) continue;
            valor *= 1024ULL;   // /proc/meminfo informa en kB

            if      (c[0] == "MemTotal:")     { m.total = valor;      vioTotal = true; }
            else if (c[0] == "MemAvailable:") { m.disponible = valor; vioDisponible = true; }
            else if (c[0] == "SwapTotal:")    { swapTotal = valor; }
            else if (c[0] == "SwapFree:")     { swapLibre = valor; }
        }

        if (!vioTotal) {
            m.problema = "/proc/meminfo no informo MemTotal";
            return m;
        }
        if (!vioDisponible) {
            // MemAvailable existe desde Linux 3.14. En un kernel mas viejo no
            // hay estimacion del kernel y habria que aproximarla; se dice en
            // vez de inventar un numero.
            m.problema = "/proc/meminfo no informo MemAvailable (kernel anterior a 3.14)";
            return m;
        }

        m.usada     = (m.total > m.disponible) ? m.total - m.disponible : 0;
        m.haySwap   = swapTotal > 0;
        m.swapTotal = swapTotal;
        m.swapUsada = (swapTotal > swapLibre) ? swapTotal - swapLibre : 0;
        m.ok        = true;
        return m;
    }
};

// -------------------------------------------------------------------------
// Memoria por el comando free
//
// Es la ruta de respaldo. free lee exactamente el mismo /proc/meminfo, asi que
// los numeros deben coincidir: sirve como verificacion cruzada y como plan B
// si /proc no estuviera montado.
//
// La salida de free esta traducida al idioma del sistema, asi que la etiqueta
// puede ser "Mem:" o "Mem.:". Se compara solo el prefijo, en minusculas, para
// no depender del idioma de la maquina.
// -------------------------------------------------------------------------
class ProveedorFree : public IProveedorMemoria {
public:
    const char* nombre()    const override { return "free -b"; }
    const char* mecanismo() const override { return "comando externo"; }
    bool        disponible() const override { return comandoExiste("free"); }

    MemoriaInfo leer() const override {
        MemoriaInfo m;
        m.fuente = nombre();

        const SalidaComando s = ejecutar("free -b 2>/dev/null");
        if (!s.ok) {
            m.problema = s.estado == 127 ? "el comando free no esta disponible"
                                         : "free fallo con codigo " + std::to_string(s.estado);
            return m;
        }

        std::istringstream ss(s.texto);
        std::string linea;
        while (std::getline(ss, linea)) {
            std::vector<std::string> c = campos(linea);
            if (c.empty()) continue;

            std::string etiqueta = c[0];
            std::transform(etiqueta.begin(), etiqueta.end(), etiqueta.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

            // Fila de memoria: total, usada, libre, compartida, buff/cache, disponible
            if (etiqueta.rfind("mem", 0) == 0 && c.size() >= 7) {
                aEntero(c[1], m.total);
                aEntero(c[6], m.disponible);
                m.usada = (m.total > m.disponible) ? m.total - m.disponible : 0;
                m.ok = m.total > 0;
            }
            // Fila de intercambio: total, usada, libre
            else if (etiqueta.rfind("swap", 0) == 0 && c.size() >= 4) {
                aEntero(c[1], m.swapTotal);
                aEntero(c[2], m.swapUsada);
                m.haySwap = m.swapTotal > 0;
            }
        }

        if (!m.ok) m.problema = "no se pudo interpretar la salida de free";
        return m;
    }
};

// Segundos transcurridos desde que arranco el sistema. Hace falta para calcular
// el porcentaje de CPU de cada proceso.
double tiempoDeArranque() {
    std::ifstream f("/proc/uptime");
    double segundos = 0.0;
    if (f >> segundos) return segundos;
    return 0.0;
}

// -------------------------------------------------------------------------
// Procesos leyendo /proc directamente
//
// Cada proceso vivo tiene un directorio /proc/<pid>. El archivo stat de adentro
// trae sus contadores en una sola linea de campos separados por espacios.
//
// LA TRAMPA DEL SEGUNDO CAMPO
//   El campo 2 es el nombre del ejecutable ENTRE PARENTESIS, y puede contener
//   espacios y hasta parentesis: un programa llamado "mi (prog) raro" produce
//   "1234 (mi (prog) raro) S 1 ...". Partir la linea por espacios rompe el
//   analisis y desplaza todos los campos siguientes.
//   La forma correcta, y la que usa el propio codigo del kernel, es buscar el
//   ULTIMO parentesis de cierre de la linea: el nombre es lo que hay entre el
//   primer '(' y ese ultimo ')', y los campos numericos empiezan despues.
//
// UNIDADES
//   utime y stime vienen en tics de reloj, no en segundos: hay que dividir por
//   sysconf(_SC_CLK_TCK), que en Linux suele ser 100. rss viene en paginas, no
//   en bytes: hay que multiplicar por sysconf(_SC_PAGESIZE), que suele ser
//   4096. Usar las constantes 100 y 4096 a mano funcionaria en esta maquina y
//   daria numeros equivocados en otra.
// -------------------------------------------------------------------------
class ProveedorProcFS : public IProveedorProcesos {
public:
    const char* nombre()    const override { return "/proc"; }
    const char* mecanismo() const override { return "archivo del sistema"; }

    bool disponible() const override {
        std::ifstream f("/proc/self/stat");
        return f.good();
    }

    std::string listar(std::vector<ProcesoInfo>& salida) const override {
        salida.clear();

        const double hercios      = static_cast<double>(sysconf(_SC_CLK_TCK));
        const double bytesPorPag  = static_cast<double>(sysconf(_SC_PAGESIZE));
        const double arranque     = tiempoDeArranque();

        // El total de memoria hace falta para el porcentaje por proceso.
        const MemoriaInfo mem = ProveedorMeminfo().leer();
        const double totalRam = mem.ok ? static_cast<double>(mem.total) : 0.0;

        // Se enumera con directory_iterator y NO invocando 'ls'. Es el punto
        // entero de esta ruta: recorrer /proc no crea ni un solo proceso nuevo.
        // Usar un comando aqui la convertiria en la otra ruta disfrazada, y la
        // comparacion de costo entre ambas dejaria de significar nada.
        std::error_code ec;
        std::filesystem::directory_iterator it("/proc", ec);
        if (ec) return "no se pudo abrir /proc: " + ec.message();

        for (const std::filesystem::directory_entry& de : it) {
            // Solo interesan los directorios cuyo nombre es un numero: esos son
            // los procesos. El resto de /proc son archivos del sistema.
            const std::string entrada = de.path().filename().string();
            if (entrada.empty() ||
                entrada.find_first_not_of("0123456789") != std::string::npos) continue;

            ProcesoInfo p;
            if (leerProceso(de.path().string(), hercios, bytesPorPag, arranque, totalRam, p))
                salida.push_back(p);
            // Si falla, el proceso murio entre el listado y la lectura. Es la
            // condicion de carrera que el enunciado y el banco de preguntas
            // mencionan: se descarta esa entrada en silencio y se sigue, que es
            // lo mismo que hace ps.
        }

        if (salida.empty()) return "no se pudo leer ningun proceso de /proc";
        return "";
    }

private:
    // Esta funcion se ocupa de LEER; el analisis del texto vive en
    // AnalisisProcFS.h, separado a proposito para que se pueda probar con
    // lineas sinteticas. Una prueba de mutacion mostro que, mientras el
    // analisis estaba embebido aqui, romperlo no hacia fallar ningun test:
    // los procesos reales de la maquina casi nunca tienen nombres raros.
    static bool leerProceso(const std::string& dir, double hercios, double bytesPorPag,
                            double arranque, double totalRam, ProcesoInfo& p) {
        std::ifstream f(dir + "/stat");
        if (!f) return false;

        std::string linea;
        if (!std::getline(f, linea)) return false;

        const procfs::CamposStat c = procfs::analizarLineaStat(linea);
        if (!c.ok) return false;

        p.pid    = c.pid;
        p.nombre = c.nombre;
        p.estado = c.estado;

        // Aca si hace falta la plataforma: los tics por segundo y el tamano de
        // pagina se piden a sysconf y no se codifican a mano, porque 100 y 4096
        // son los valores de ESTA maquina, no los del formato.
        p.tiempoUsuario = static_cast<double>(c.utime) / hercios;
        p.tiempoSistema = static_cast<double>(c.stime) / hercios;
        p.memResidente  = static_cast<unsigned long long>(
                              static_cast<double>(c.paginasResidentes) * bytesPorPag);

        // %CPU al estilo de ps: tiempo de CPU acumulado sobre tiempo de vida.
        const double vivo = arranque - (static_cast<double>(c.starttime) / hercios);
        if (vivo > 0.0)
            p.cpuPorcentaje = 100.0 * (p.tiempoUsuario + p.tiempoSistema) / vivo;

        if (totalRam > 0.0)
            p.memPorcentaje = 100.0 * static_cast<double>(p.memResidente) / totalRam;

        return true;
    }
};

// -------------------------------------------------------------------------
// Procesos por el comando ps
//
// Ruta de respaldo y punto de comparacion. Los sufijos '=' de cada columna
// suprimen la cabecera, asi que no hay que saltarse ninguna linea. comm va al
// final a proposito: es el unico campo que puede contener espacios, y
// dejandolo ultimo el resto se parte sin ambiguedad.
// -------------------------------------------------------------------------
class ProveedorPs : public IProveedorProcesos {
public:
    const char* nombre()    const override { return "ps"; }
    const char* mecanismo() const override { return "comando externo"; }
    bool        disponible() const override { return comandoExiste("ps"); }

    std::string listar(std::vector<ProcesoInfo>& salida) const override {
        salida.clear();

        const SalidaComando s = ejecutar("ps -eo pid=,stat=,pcpu=,pmem=,rss=,comm= 2>/dev/null");
        if (!s.ok) {
            if (s.estado == 127) return "el comando ps no esta disponible en este sistema";
            return "ps fallo con codigo " + std::to_string(s.estado);
        }

        std::istringstream ss(s.texto);
        std::string linea;
        while (std::getline(ss, linea)) {
            std::istringstream l(linea);
            std::string sPid, sEstado, sCpu, sMem, sRss, sNombre;
            if (!(l >> sPid >> sEstado >> sCpu >> sMem >> sRss)) continue;
            std::getline(l >> std::ws, sNombre);

            ProcesoInfo p;
            unsigned long long pid = 0, rss = 0;
            if (!aEntero(sPid, pid)) continue;
            p.pid    = static_cast<long>(pid);
            p.nombre = sNombre;
            p.estado = sEstado.empty() ? '?' : sEstado[0];
            aReal(sCpu, p.cpuPorcentaje);
            aReal(sMem, p.memPorcentaje);
            if (aEntero(sRss, rss)) p.memResidente = rss * 1024ULL;  // ps informa en kB

            // ps con estas columnas no da el desglose usuario/sistema. Queda en
            // cero y se declara: la vista de detalle usa /proc para eso.
            salida.push_back(p);
        }

        if (salida.empty()) return "no se pudo interpretar la salida de ps";
        return "";
    }
};

} // namespace

// -------------------------------------------------------------------------
// Consumo de la propia herramienta, por /proc/self
//
// /proc/self es un enlace al directorio del proceso que lo lee, asi que no hay
// que averiguar el propio PID. Se usa el mismo mecanismo que el monitoreo del
// resto del proyecto, que es lo mas coherente y lo mas facil de defender.
//
//   VmRSS  memoria realmente en RAM (residente)
//   VmSize espacio de direcciones virtual reservado; siempre mucho mayor,
//          porque incluye lo mapeado pero nunca tocado
// -------------------------------------------------------------------------
ConsumoPropio sistema::consumoPropio() {
    ConsumoPropio c;
    c.fuente = "/proc/self/status y /proc/self/stat";

    std::ifstream f("/proc/self/status");
    if (!f) {
        c.problema = "no se pudo abrir /proc/self/status";
        return c;
    }

    std::string linea;
    while (std::getline(f, linea)) {
        const std::vector<std::string> campo = campos(linea);
        if (campo.size() < 2) continue;
        unsigned long long v = 0;
        if (campo[0] == "VmRSS:"  && aEntero(campo[1], v)) c.memResidente = v * 1024ULL;
        if (campo[0] == "VmSize:" && aEntero(campo[1], v)) c.memSegunda   = v * 1024ULL;
    }
    c.etiquetaSegunda = "Memoria virtual (VmSize)";
    c.explicaSegunda  = "espacio de direcciones reservado, incluido lo mapeado pero nunca tocado";

    std::ifstream g("/proc/self/stat");
    if (g) {
        std::string s;
        if (std::getline(g, s)) {
            const size_t cierra = s.rfind(')');
            if (cierra != std::string::npos) {
                const std::vector<std::string> campo = campos(s.substr(cierra + 1));
                if (campo.size() >= 13) {
                    const double hercios = static_cast<double>(sysconf(_SC_CLK_TCK));
                    unsigned long long u = 0, st = 0;
                    aEntero(campo[11], u);
                    aEntero(campo[12], st);
                    c.tiempoUsuario = static_cast<double>(u)  / hercios;
                    c.tiempoSistema = static_cast<double>(st) / hercios;
                }
            }
        }
    }

    c.ok = c.memResidente > 0;
    if (!c.ok) c.problema = "no se encontro VmRSS en /proc/self/status";
    return c;
}

long sistema::pidPropio() {
    return static_cast<long>(getpid());
}

bool sistema::permisosSonReales() { return true; }

std::string sistema::descripcionPlataforma() {
    std::string s = "Linux";
    struct utsname u;
    if (uname(&u) == 0)
        s = std::string(u.sysname) + " " + u.release + " (" + u.machine + ")";
#ifdef __VERSION__
    s += ", g++ " + std::string(__VERSION__).substr(0, 6);
#endif
    return s;
}

const std::vector<const IProveedorMemoria*>& sistema::proveedoresMemoria() {
    // Estaticos con duracion de todo el programa: los punteros que se entregan
    // siguen siendo validos hasta que el proceso termina.
    static const ProveedorMeminfo meminfo;
    static const ProveedorFree    libre;
    static const std::vector<const IProveedorMemoria*> lista = { &meminfo, &libre };
    return lista;
}

const std::vector<const IProveedorProcesos*>& sistema::proveedoresProcesos() {
    static const ProveedorProcFS procfs;
    static const ProveedorPs     ps;
    static const std::vector<const IProveedorProcesos*> lista = { &procfs, &ps };
    return lista;
}

// ===========================================================================
// 3. IMPLEMENTACIONES DE WINDOWS
// ===========================================================================
#else
namespace {

// -------------------------------------------------------------------------
// Memoria por GlobalMemoryStatusEx
//
// Es la API nativa. A diferencia de Linux, aca no hay un archivo que leer: la
// informacion se pide al kernel por una llamada. El campo dwLength hay que
// rellenarlo ANTES de llamar, porque es como la API distingue versiones de la
// estructura; olvidarlo hace que la llamada falle.
// -------------------------------------------------------------------------
class ProveedorGlobalMemory : public IProveedorMemoria {
public:
    const char* nombre()    const override { return "GlobalMemoryStatusEx"; }
    const char* mecanismo() const override { return "API del sistema"; }
    bool        disponible() const override { return true; }   // siempre presente

    MemoriaInfo leer() const override {
        MemoriaInfo m;
        m.fuente = nombre();

        MEMORYSTATUSEX estado;
        std::memset(&estado, 0, sizeof(estado));
        estado.dwLength = sizeof(estado);

        if (!GlobalMemoryStatusEx(&estado)) {
            m.problema = "GlobalMemoryStatusEx fallo";
            return m;
        }

        m.total      = estado.ullTotalPhys;
        m.disponible = estado.ullAvailPhys;
        m.usada      = (m.total > m.disponible) ? m.total - m.disponible : 0;

        // ullTotalPageFile incluye la RAM mas el archivo de paginacion, asi que
        // el archivo por si solo es la diferencia. No es exactamente el swap de
        // Linux, y por eso se informa con ese nombre.
        if (estado.ullTotalPageFile > estado.ullTotalPhys) {
            m.haySwap   = true;
            m.swapTotal = estado.ullTotalPageFile - estado.ullTotalPhys;
            const unsigned long long libre =
                (estado.ullAvailPageFile > estado.ullAvailPhys)
                    ? estado.ullAvailPageFile - estado.ullAvailPhys : 0;
            m.swapUsada = (m.swapTotal > libre) ? m.swapTotal - libre : 0;
        }

        m.ok = m.total > 0;
        return m;
    }
};

// -------------------------------------------------------------------------
// Procesos por CreateToolhelp32Snapshot
//
// Toolhelp toma una FOTO del conjunto de procesos y despues se recorre. Eso
// evita a medias la condicion de carrera de /proc -la lista no cambia mientras
// se recorre-, pero no del todo: un proceso de la foto puede haber terminado
// para cuando se le piden sus detalles con OpenProcess.
//
// OpenProcess falla legitimamente en los procesos protegidos del sistema y en
// los de otros usuarios sin privilegios de administrador. No es un error de la
// herramienta: el proceso se informa igual, con sus contadores en cero.
// -------------------------------------------------------------------------
class ProveedorToolhelp : public IProveedorProcesos {
public:
    const char* nombre()    const override { return "CreateToolhelp32Snapshot"; }
    const char* mecanismo() const override { return "API del sistema"; }
    bool        disponible() const override { return true; }

    std::string listar(std::vector<ProcesoInfo>& salida) const override {
        salida.clear();

        const HANDLE foto = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (foto == INVALID_HANDLE_VALUE) return "no se pudo tomar la foto de procesos";

        MEMORYSTATUSEX mem;
        std::memset(&mem, 0, sizeof(mem));
        mem.dwLength = sizeof(mem);
        const double totalRam = GlobalMemoryStatusEx(&mem)
                                ? static_cast<double>(mem.ullTotalPhys) : 0.0;

        PROCESSENTRY32 entrada;
        std::memset(&entrada, 0, sizeof(entrada));
        entrada.dwSize = sizeof(entrada);   // igual que dwLength: obligatorio antes de llamar

        if (!Process32First(foto, &entrada)) {
            CloseHandle(foto);
            return "no se pudo leer la primera entrada de la foto";
        }

        do {
            ProcesoInfo p;
            p.pid    = static_cast<long>(entrada.th32ProcessID);
            p.nombre = entrada.szExeFile;
            p.estado = 'R';   // Windows no expone un estado por proceso como Unix

            detallar(p, totalRam);
            salida.push_back(p);
        } while (Process32Next(foto, &entrada));

        CloseHandle(foto);
        if (salida.empty()) return "la foto de procesos vino vacia";
        return "";
    }

private:
    // Rellena memoria y tiempos de CPU. Silencioso ante fallos de permiso.
    static void detallar(ProcesoInfo& p, double totalRam) {
        const HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ,
                                     FALSE, static_cast<DWORD>(p.pid));
        if (h == nullptr) return;   // protegido, de otro usuario, o ya termino

        PROCESS_MEMORY_COUNTERS contadores;
        std::memset(&contadores, 0, sizeof(contadores));
        contadores.cb = sizeof(contadores);
        if (GetProcessMemoryInfo(h, &contadores, sizeof(contadores))) {
            p.memResidente = contadores.WorkingSetSize;
            if (totalRam > 0.0)
                p.memPorcentaje = 100.0 * static_cast<double>(p.memResidente) / totalRam;
        }

        FILETIME creacion, salida, kernel, usuario;
        if (GetProcessTimes(h, &creacion, &salida, &kernel, &usuario)) {
            p.tiempoSistema = aSegundos(kernel);
            p.tiempoUsuario = aSegundos(usuario);

            const double vivo = segundosDesde(creacion);
            if (vivo > 0.0)
                p.cpuPorcentaje = 100.0 * (p.tiempoUsuario + p.tiempoSistema) / vivo;
        }

        CloseHandle(h);
    }

    // FILETIME cuenta intervalos de 100 nanosegundos repartidos en dos mitades
    // de 32 bits. Hay que recomponerlo antes de dividir; leer solo dwLowDateTime
    // daria numeros que se reinician cada siete minutos.
    static unsigned long long aEnteroFT(const FILETIME& ft) {
        ULARGE_INTEGER u;
        u.LowPart  = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return u.QuadPart;
    }

    static double aSegundos(const FILETIME& ft) {
        return static_cast<double>(aEnteroFT(ft)) / 1e7;   // 10^7 intervalos por segundo
    }

    static double segundosDesde(const FILETIME& inicio) {
        FILETIME ahora;
        GetSystemTimeAsFileTime(&ahora);
        const unsigned long long a = aEnteroFT(ahora), i = aEnteroFT(inicio);
        return (a > i) ? static_cast<double>(a - i) / 1e7 : 0.0;
    }
};

// -------------------------------------------------------------------------
// Procesos por el comando tasklist
//
// Ruta de respaldo, equivalente a ps en Linux. El formato CSV se pide a
// proposito: la salida de tabla por defecto alinea con espacios y se rompe con
// los nombres largos. /NH suprime la cabecera.
//
// Formato de cada fila:
//   "nombre.exe","1234","Console","1","12,345 K"
// El uso de memoria trae separador de miles y sufijo " K", asi que hay que
// limpiarlo antes de convertirlo.
// -------------------------------------------------------------------------
class ProveedorTasklist : public IProveedorProcesos {
public:
    const char* nombre()    const override { return "tasklist"; }
    const char* mecanismo() const override { return "comando externo"; }
    bool        disponible() const override { return comandoExiste("tasklist"); }

    std::string listar(std::vector<ProcesoInfo>& salida) const override {
        salida.clear();

        const SalidaComando s = ejecutar("tasklist /FO CSV /NH 2>nul");
        if (!s.ok) {
            if (s.estado == 127) return "el comando tasklist no esta disponible";
            return "tasklist fallo con codigo " + std::to_string(s.estado);
        }

        MEMORYSTATUSEX mem;
        std::memset(&mem, 0, sizeof(mem));
        mem.dwLength = sizeof(mem);
        const double totalRam = GlobalMemoryStatusEx(&mem)
                                ? static_cast<double>(mem.ullTotalPhys) : 0.0;

        std::istringstream ss(s.texto);
        std::string linea;
        while (std::getline(ss, linea)) {
            const std::vector<std::string> c = partirCsv(linea);
            if (c.size() < 5) continue;

            ProcesoInfo p;
            unsigned long long pid = 0;
            if (!aEntero(c[1], pid)) continue;
            p.pid    = static_cast<long>(pid);
            p.nombre = c[0];
            p.estado = 'R';

            unsigned long long kb = 0;
            if (aEntero(soloDigitos(c[4]), kb)) {
                p.memResidente = kb * 1024ULL;
                if (totalRam > 0.0)
                    p.memPorcentaje = 100.0 * static_cast<double>(p.memResidente) / totalRam;
            }
            // tasklist no informa tiempos de CPU: quedan en cero y se declara.
            salida.push_back(p);
        }

        if (salida.empty()) return "no se pudo interpretar la salida de tasklist";
        return "";
    }

private:
    static std::vector<std::string> partirCsv(const std::string& linea) {
        std::vector<std::string> v;
        std::string actual;
        bool dentro = false;
        for (char ch : linea) {
            if (ch == '"')                 dentro = !dentro;
            else if (ch == ',' && !dentro) { v.push_back(actual); actual.clear(); }
            else                           actual += ch;
        }
        v.push_back(actual);
        return v;
    }

    // "12,345 K" -> "12345"
    static std::string soloDigitos(const std::string& s) {
        std::string r;
        for (char ch : s) if (ch >= '0' && ch <= '9') r += ch;
        return r;
    }
};

} // namespace

// Consumo propio en Windows: el equivalente de /proc/self, por API.
ConsumoPropio sistema::consumoPropio() {
    ConsumoPropio c;
    c.fuente = "GetProcessMemoryInfo y GetProcessTimes";

    const HANDLE yo = GetCurrentProcess();   // pseudo-handle, no hay que cerrarlo

    // La variante EX trae PrivateUsage, que es el compromiso de memoria del
    // proceso. Es lo mas cercano al VmSize de Linux que expone Windows por esta
    // via. La version corriente de la estructura solo llega hasta
    // PeakWorkingSetSize, que es el pico de memoria RESIDENTE y no tiene nada
    // que ver con el espacio de direcciones: usarlo como "memoria virtual"
    // producia un numero identico al residente y una explicacion falsa.
    PROCESS_MEMORY_COUNTERS_EX contadores;
    std::memset(&contadores, 0, sizeof(contadores));
    contadores.cb = sizeof(contadores);
    if (GetProcessMemoryInfo(yo, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&contadores),
                             sizeof(contadores))) {
        c.memResidente = contadores.WorkingSetSize;
        c.memSegunda   = contadores.PrivateUsage;
    }
    c.etiquetaSegunda = "Compromiso de memoria (PrivateUsage)";
    c.explicaSegunda  = "memoria que el sistema se comprometio a darle, en RAM o en el archivo de paginacion";

    FILETIME creacion, fin, kernel, usuario;
    if (GetProcessTimes(yo, &creacion, &fin, &kernel, &usuario)) {
        ULARGE_INTEGER k, u;
        k.LowPart = kernel.dwLowDateTime;   k.HighPart = kernel.dwHighDateTime;
        u.LowPart = usuario.dwLowDateTime;  u.HighPart = usuario.dwHighDateTime;
        c.tiempoSistema = static_cast<double>(k.QuadPart) / 1e7;
        c.tiempoUsuario = static_cast<double>(u.QuadPart) / 1e7;
    }

    c.ok = c.memResidente > 0;
    if (!c.ok) c.problema = "GetProcessMemoryInfo no devolvio datos";
    return c;
}

long sistema::pidPropio() {
    return static_cast<long>(GetCurrentProcessId());
}

// Falso a proposito. Se comprobo creando un archivo normal y comparando:
//   la herramienta reportaba  666 rw-rw-rw-
//   icacls reportaba          solo SYSTEM, Administradores y el propio usuario
// El bit de escritura para "otros" esta siempre puesto, asi que un juicio de
// riesgo basado en el seria un falso positivo en todos los archivos.
bool sistema::permisosSonReales() { return false; }

std::string sistema::descripcionPlataforma() {
    std::string s = "Windows";
    // GetVersionEx esta obsoleta y sin manifiesto miente la version, asi que
    // no se usa: se declara la familia y se deja el detalle exacto al README,
    // que si se verifica a mano. Afirmar una version que no se comprobo seria
    // justo el tipo de dato inventado que este curso penaliza.
#ifdef __VERSION__
    s += ", g++ " + std::string(__VERSION__).substr(0, 6);
#endif
    return s;
}

const std::vector<const IProveedorMemoria*>& sistema::proveedoresMemoria() {
    static const ProveedorGlobalMemory api;
    static const std::vector<const IProveedorMemoria*> lista = { &api };
    return lista;
}

const std::vector<const IProveedorProcesos*>& sistema::proveedoresProcesos() {
    static const ProveedorToolhelp toolhelp;
    static const ProveedorTasklist tasklist;
    static const std::vector<const IProveedorProcesos*> lista = { &toolhelp, &tasklist };
    return lista;
}

#endif // _WIN32

// ===========================================================================
// 4. FABRICAS COMUNES  (sin un solo #ifdef: se apoyan en lo de arriba)
// ===========================================================================

MemoriaInfo sistema::leerMemoria(const std::string& preferido) {
    const std::vector<const IProveedorMemoria*>& lista = proveedoresMemoria();

    // Primero el pedido explicitamente, si esta disponible.
    if (!preferido.empty()) {
        for (const IProveedorMemoria* p : lista)
            if (preferido == p->nombre() && p->disponible()) return p->leer();
    }

    // Si no, el primero que responda. Esta cadena de respaldo es lo que hace
    // que "comando no disponible" no deje a la herramienta sin respuesta.
    MemoriaInfo ultima;
    for (const IProveedorMemoria* p : lista) {
        if (!p->disponible()) {
            ultima.problema = std::string(p->nombre()) + " no esta disponible";
            continue;
        }
        const MemoriaInfo m = p->leer();
        if (m.ok) return m;
        ultima = m;
    }

    if (ultima.problema.empty()) ultima.problema = "ningun proveedor de memoria respondio";
    return ultima;
}

std::string sistema::listarProcesos(std::vector<ProcesoInfo>& salida,
                                    std::string& fuenteUsada,
                                    const std::string& preferido) {
    const std::vector<const IProveedorProcesos*>& lista = proveedoresProcesos();
    fuenteUsada.clear();

    if (!preferido.empty()) {
        for (const IProveedorProcesos* p : lista) {
            if (preferido != p->nombre()) continue;
            if (!p->disponible())
                return std::string(p->nombre()) + " no esta disponible en este sistema";
            const std::string err = p->listar(salida);
            if (err.empty()) fuenteUsada = p->nombre();
            return err;
        }
        return "no existe una fuente de procesos llamada '" + preferido + "'";
    }

    std::string ultimoError;
    for (const IProveedorProcesos* p : lista) {
        if (!p->disponible()) {
            ultimoError = std::string(p->nombre()) + " no esta disponible";
            continue;
        }
        const std::string err = p->listar(salida);
        if (err.empty()) { fuenteUsada = p->nombre(); return ""; }
        ultimoError = err;
    }

    return ultimoError.empty() ? "ningun proveedor de procesos respondio" : ultimoError;
}

// ===========================================================================
// Presentacion: encender las secuencias ANSI de la consola
// ===========================================================================
namespace consola {

bool habilitarAnsi() {
#ifdef _WIN32
    // La consola de Windows interpreta secuencias ANSI desde Windows 10, pero
    // hay que pedirlo: por compatibilidad con programas antiguos viene apagado.
    // Sin esto, los colores salen como texto crudo del tipo <-[31m.
    HANDLE salida = GetStdHandle(STD_OUTPUT_HANDLE);
    if (salida == INVALID_HANDLE_VALUE) return false;

    DWORD modo = 0;
    if (!GetConsoleMode(salida, &modo)) return false;   // redirigido a archivo o tuberia

    modo |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(salida, modo) != 0;
#else
    // Linux y macOS: cualquier terminal moderna interpreta ANSI sin pedir nada.
    return true;
#endif
}

} // namespace consola

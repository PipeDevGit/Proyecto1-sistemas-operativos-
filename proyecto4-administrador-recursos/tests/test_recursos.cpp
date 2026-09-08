// ---------------------------------------------------------------------------
// test_recursos.cpp - Suite de pruebas del Proyecto 4
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Comprueba automaticamente lo que de otro modo habria que volver a probar a
//   mano en cada cambio. Se concentra en tres cosas:
//     1. La contencion del directorio de trabajo, que es lo mas importante del
//        proyecto desde el punto de vista conceptual. Cada prueba es un ATAQUE
//        concreto con su explicacion.
//     2. Los tres errores que el enunciado nombra explicitamente.
//     3. Verificaciones cruzadas: que dos fuentes independientes de la misma
//        informacion coincidan.
//
// POR QUE SIN BIBLIOTECA DE PRUEBAS
//   El resto del curso se hizo con biblioteca estandar pura, sin dependencias
//   externas, y agregar una para esto obligaria a instalarla en las dos
//   plataformas antes de poder compilar. El arnes de abajo son treinta lineas.
//
// COMO SE CORRE
//   make tests
//   Devuelve 0 si todo paso y 1 si algo fallo, para que se pueda encadenar.
//
// SOBRE LAS PRUEBAS OMITIDAS
//   Las que crean enlaces simbolicos se omiten en Windows, donde hacerlo exige
//   privilegios de administrador o el modo desarrollador. Se informan como
//   OMITIDA y no como pasada: contarlas como exito seria afirmar una
//   verificacion que no ocurrio.
// ---------------------------------------------------------------------------
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "AnalisisProcFS.h"
#include "GestorArchivos.h"
#include "MonitorMemoria.h"
#include "MonitorProcesos.h"
#include "Permisos.h"
#include "Sandbox.h"

namespace fs = std::filesystem;

// ===========================================================================
// Arnes minimo
// ===========================================================================
namespace {

int pasadas = 0, fallidas = 0, omitidas = 0;
bool color = true;

const char* VERDE() { return color ? "\033[32m" : ""; }
const char* ROJO()  { return color ? "\033[31m" : ""; }
const char* AMBAR() { return color ? "\033[33m" : ""; }
const char* GRIS()  { return color ? "\033[90m" : ""; }
const char* NEG()   { return color ? "\033[1m"  : ""; }
const char* FIN()   { return color ? "\033[0m"  : ""; }

void seccion(const char* titulo) {
    std::printf("\n  %s%s%s\n", NEG(), titulo, FIN());
}

// Cada prueba lleva, ademas del nombre, POR QUE deberia pasar. Un test que
// falla y solo dice "test 7" obliga a leer el codigo para entender que se
// rompio; diciendo que se esperaba, el mensaje se explica solo.
void revisar(bool condicion, const std::string& nombre, const std::string& porque) {
    if (condicion) {
        ++pasadas;
        std::printf("  %s[ ok ]%s %-52s\n", VERDE(), FIN(), nombre.c_str());
    } else {
        ++fallidas;
        std::printf("  %s[FALLA]%s %-52s\n", ROJO(), FIN(), nombre.c_str());
        std::printf("         %sse esperaba: %s%s\n", GRIS(), porque.c_str(), FIN());
    }
}

void omitir(const std::string& nombre, const std::string& motivo) {
    ++omitidas;
    std::printf("  %s[omit]%s %-52s %s(%s)%s\n",
                AMBAR(), FIN(), nombre.c_str(), GRIS(), motivo.c_str(), FIN());
}

// Directorio de trabajo temporal, distinto en cada corrida para que dos
// ejecuciones simultaneas no se pisen.
fs::path raizTemporal() {
    static const fs::path r = fs::temp_directory_path() /
        ("p4_pruebas_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count() % 1000000));
    return r;
}

} // namespace

// ===========================================================================
// 1. CONTENCION DEL DIRECTORIO DE TRABAJO
//
// Es el nucleo conceptual del proyecto: la Unidad V aplicada. Cada prueba de
// esta seccion es un intento concreto de salirse.
// ===========================================================================
static void pruebasSandbox(const fs::path& base) {
    seccion("Contencion del directorio de trabajo (Unidad V)");

    const Sandbox caja(base);
    revisar(caja.listo(),
            "1. el directorio de trabajo se crea y se resuelve",
            "que Sandbox quede listo() sobre un directorio nuevo");

    // --- Ataques que deben ser rechazados ---------------------------------
    revisar(!caja.resolver("../../etc/passwd").ok,
            "2. rechaza escapar con ..",
            "que ../../etc/passwd caiga fuera y se rechace");

    revisar(!caja.resolver("/etc/passwd").ok,
            "3. rechaza una ruta absoluta de fuera",
            "que /etc/passwd no pase el filtro de contencion");

    revisar(!caja.resolver("..").ok,
            "4. rechaza el directorio padre",
            "que .. resuelva fuera del base y se rechace");

    // El prefijo trampa: un directorio hermano cuyo nombre EMPIEZA igual que
    // el base. Con una comparacion de texto pasaria; con comparacion por
    // componentes de ruta, no.
    std::error_code ec;
    const fs::path hermano = fs::path(base.string() + "_malo");
    fs::create_directories(hermano, ec);
    std::ofstream(hermano / "nomina.txt") << "confidencial\n";

    revisar(!caja.resolver("../" + hermano.filename().string() + "/nomina.txt").ok,
            "5. rechaza el hermano con prefijo de texto igual",
            "que workspace_malo no cuente como dentro de workspace");

    // --- Rutas legitimas que deben ser aceptadas --------------------------
    revisar(caja.resolver("notas.txt").ok,
            "6. acepta un nombre simple",
            "que un archivo del directorio de trabajo se acepte");

    revisar(caja.resolver("informes/2026/marzo.txt").ok,
            "7. acepta una ruta anidada",
            "que los subdirectorios del base se acepten");

    revisar(caja.resolver("sub/../notas.txt").ok,
            "8. acepta un .. que no sale del base",
            "que sub/../notas.txt resuelva dentro y se acepte");

    revisar(caja.resolver("./bien.txt").ok,
            "9. acepta ./ al principio",
            "que el ./ redundante no moleste");

    revisar(!caja.resolver("").ok,
            "10. rechaza un nombre vacio",
            "que la cadena vacia no resuelva al propio directorio base");

    // Una ruta absoluta que apunta ADENTRO si es legitima. Esto comprueba que
    // el rechazo de /etc/passwd no viene de un atajo del tipo "toda ruta
    // absoluta es mala", sino del mecanismo real de contencion.
    revisar(caja.resolver((base / "adentro.txt").string()).ok,
            "11. acepta una ruta absoluta que apunta adentro",
            "que el filtro mida contencion y no la forma de la ruta");

    // --- Enlaces simbolicos ------------------------------------------------
    // weakly_canonical los resuelve antes de comparar, asi que estos dos casos
    // se bloquean sin una sola linea de codigo dedicada a ellos.
    const fs::path aArchivo = base / "enlace_archivo";
    const fs::path aCarpeta = base / "enlace_carpeta";
    fs::remove(aArchivo, ec);
    fs::remove(aCarpeta, ec);

    fs::create_symlink("/etc/passwd", aArchivo, ec);
    if (ec) {
        omitir("12. rechaza un enlace simbolico a un archivo de fuera",
               "crear enlaces requiere privilegios en esta plataforma");
    } else {
        revisar(!caja.resolver("enlace_archivo").ok,
                "12. rechaza un enlace simbolico a un archivo de fuera",
                "que weakly_canonical resuelva el enlace y detecte la salida");
    }

    fs::create_directory_symlink("/etc", aCarpeta, ec);
    if (ec) {
        omitir("13. rechaza atravesar un enlace a un directorio de fuera",
               "crear enlaces requiere privilegios en esta plataforma");
    } else {
        revisar(!caja.resolver("enlace_carpeta/passwd").ok,
                "13. rechaza atravesar un enlace a un directorio de fuera",
                "que el enlace se resuelva antes de comparar la contencion");
    }

    fs::remove(aArchivo, ec);
    fs::remove(aCarpeta, ec);
    fs::remove_all(hermano, ec);
}

// ===========================================================================
// 2. EL CONTRAEJEMPLO QUE JUSTIFICA LA IMPLEMENTACION
//
// No comprueba el programa: comprueba que la alternativa mas corta ESTA MAL.
// Sin esta prueba, la decision de comparar por componentes es una opinion;
// con ella, es un hecho verificable en cada corrida.
// ===========================================================================
static void pruebaContraejemplo() {
    seccion("Por que la comparacion se hace por componentes y no por texto");

    const fs::path base      = "/home/felipe/data/workspace";
    const fs::path deAfuera  = "/home/felipe/data/workspace_malo/nomina.txt";

    // La version ingenua: "empieza por".
    const bool porTexto = deAfuera.string().rfind(base.string(), 0) == 0;

    // La version real, la misma idea que usa Sandbox::contenida.
    const auto par = std::mismatch(base.begin(), base.end(),
                                   deAfuera.begin(), deAfuera.end());
    const bool porComponentes = (par.first == base.end());

    revisar(porTexto,
            "14. la comparacion por texto SI acepta el hermano (el agujero)",
            "que rfind(base,0)==0 devuelva true para workspace_malo");

    revisar(!porComponentes,
            "15. la comparacion por componentes lo rechaza",
            "que std::mismatch se detenga en workspace vs workspace_malo");
}

// ===========================================================================
// 3. GESTION DE ARCHIVOS Y LOS ERRORES DEL ENUNCIADO
// ===========================================================================
static void pruebasGestor(const fs::path& base) {
    seccion("Gestion de archivos y manejo de errores");

    const Sandbox caja(base);
    std::error_code ec;

    revisar(gestor::crear(caja, "prueba.txt", "contenido").ok,
            "16. crea un archivo",
            "que crear() devuelva ok y el archivo exista");

    revisar(fs::exists(caja.base() / "prueba.txt", ec),
            "17. el archivo creado existe en disco",
            "que el archivo este realmente escrito");

    revisar(!gestor::crear(caja, "prueba.txt", "otro").ok,
            "18. no sobrescribe en silencio",
            "que crear() sobre un nombre existente falle en vez de pisar");

    revisar(gestor::crear(caja, "carpeta/hondo/dato.txt", "x").ok,
            "19. crea los directorios intermedios",
            "que una ruta anidada cree su camino");

    // --- Error 1 del enunciado: archivo inexistente -----------------------
    EntradaArchivo meta;
    revisar(!gestor::metadatos(caja, "no_existe.txt", meta).ok,
            "20. ERROR DEL ENUNCIADO: archivo inexistente",
            "que pedir metadatos de algo que no existe falle con mensaje");

    revisar(!gestor::eliminar(caja, "no_existe.txt").ok,
            "21. eliminar algo inexistente falla con mensaje",
            "que eliminar() lo reporte en vez de callarse");

    // --- Borrado no recursivo ---------------------------------------------
    revisar(!gestor::eliminar(caja, "carpeta").ok,
            "22. no borra un directorio con contenido",
            "que eliminar() se niegue: no hay borrado en cascada");

    revisar(gestor::eliminar(caja, "prueba.txt").ok,
            "23. elimina un archivo",
            "que eliminar() funcione sobre un archivo normal");

    revisar(!fs::exists(caja.base() / "prueba.txt", ec),
            "24. el archivo eliminado ya no existe",
            "que el borrado llegue al disco");

    // --- El propio directorio de trabajo no es borrable --------------------
    revisar(!gestor::eliminar(caja, ".").ok,
            "25. no permite borrar el propio directorio de trabajo",
            "que la herramienta no se quede sin suelo");

    // --- Metadatos y orden del listado ------------------------------------
    revisar(gestor::metadatos(caja, "carpeta", meta).ok && meta.esDirectorio,
            "26. reconoce un directorio como tal",
            "que metadatos() marque esDirectorio en una carpeta");

    std::vector<EntradaArchivo> lista;
    revisar(gestor::listar(caja, lista).ok && !lista.empty(),
            "27. lista el directorio de trabajo",
            "que listar() devuelva al menos la carpeta creada");

    // El orden importa: directory_iterator no lo garantiza, asi que si no se
    // ordenara, dos maquinas darian listados distintos del mismo contenido.
    bool ordenado = true;
    for (size_t i = 1; i < lista.size(); ++i) {
        const EntradaArchivo& a = lista[i - 1];
        const EntradaArchivo& b = lista[i];
        if (a.esDirectorio != b.esDirectorio) { if (!a.esDirectorio) ordenado = false; }
        else if (a.nombre > b.nombre) ordenado = false;
    }
    revisar(ordenado,
            "28. el listado sale ordenado y es reproducible",
            "directorios primero y despues por nombre");
}

// ===========================================================================
// 4. PERMISOS Y AUDITORIA
// ===========================================================================
static void pruebasPermisos() {
    seccion("Permisos: octal, rwx y clasificacion de riesgo");

    const fs::perms p644 = fs::perms::owner_read  | fs::perms::owner_write |
                           fs::perms::group_read  | fs::perms::others_read;
    const fs::perms p666 = p644 | fs::perms::group_write | fs::perms::others_write;
    const fs::perms p750 = fs::perms::owner_all |
                           fs::perms::group_read | fs::perms::group_exec;

    revisar(permisos::octal(p644) == 0644u,
            "29. traduce los permisos a octal",
            "que rw-r--r-- de 0644");

    revisar(permisos::rwx(p644) == "rw-r--r--",
            "30. traduce los permisos a rwx",
            "que 0644 de la cadena rw-r--r--");

    revisar(permisos::octalTexto(p750) == "750",
            "31. formatea el octal a tres digitos",
            "que 0750 se imprima como 750 y no como 750 sin relleno");

    revisar(permisos::evaluar(p644, false) == Riesgo::Ninguno,
            "32. 0644 no se marca como riesgo",
            "que un permiso corriente no llene el informe de ruido");

    revisar(permisos::evaluar(p666, false) == Riesgo::Alto,
            "33. escribible por otros es riesgo Alto",
            "que others_write dispare la alerta: es el find -perm -o+w del lab 5");

    // El bit sticky cambia el veredicto en directorios: sin el, cualquiera
    // borra archivos ajenos; con el, solo su dueno. Es lo que hace seguro /tmp.
    const fs::perms dir777         = fs::perms::all;
    const fs::perms dir777Pegajoso = dir777 | fs::perms::sticky_bit;

    revisar(permisos::evaluar(dir777, true) == Riesgo::Alto,
            "34. un directorio 777 sin sticky es riesgo Alto",
            "que cualquiera pueda borrar archivos ajenos ahi dentro");

    revisar(permisos::evaluar(dir777Pegajoso, true) == Riesgo::Medio,
            "35. el bit sticky baja el riesgo a Medio",
            "que el sticky impida borrar archivos de otros, como en /tmp");

    revisar(!permisos::explicar(p666, false).empty(),
            "36. toda marca de riesgo trae su explicacion",
            "que un informe que dice RIESGO diga tambien por que");
}

// ===========================================================================
// 5. MONITOREO: VERIFICACIONES CRUZADAS
//
// Estas pruebas no comparan contra un valor fijo -la memoria y los procesos
// cambian a cada instante- sino que comprueban COHERENCIA: que dos fuentes
// independientes de la misma informacion se parezcan, y que los invariantes
// se cumplan.
// ===========================================================================
static void pruebasMonitoreo() {
    seccion("Monitoreo de memoria y procesos");

    const MemoriaInfo m = sistema::leerMemoria();
    revisar(m.ok && m.total > 0,
            "37. lee la memoria del sistema",
            "que alguna fuente responda con un total mayor que cero");

    revisar(m.disponible <= m.total,
            "38. la memoria disponible no supera al total",
            "invariante: no se puede tener disponible mas de lo que hay");

    revisar(m.usada + m.disponible == m.total,
            "39. usada + disponible = total",
            "invariante de coherencia del calculo");

    // Verificacion cruzada real: si la plataforma ofrece mas de una fuente de
    // memoria, las dos deben coincidir. En Linux son /proc/meminfo y free, y
    // free lee justamente /proc/meminfo, asi que el total debe ser identico.
    const std::vector<const IProveedorMemoria*>& fuentes = sistema::proveedoresMemoria();
    std::vector<unsigned long long> totales;
    for (const IProveedorMemoria* f : fuentes) {
        if (!f->disponible()) continue;
        const MemoriaInfo x = f->leer();
        if (x.ok) totales.push_back(x.total);
    }

    if (totales.size() < 2) {
        omitir("40. las dos fuentes de memoria coinciden en el total",
               "esta plataforma solo expone una fuente");
    } else {
        const unsigned long long a = *std::min_element(totales.begin(), totales.end());
        const unsigned long long b = *std::max_element(totales.begin(), totales.end());
        revisar(a == b,
                "40. las dos fuentes de memoria coinciden en el total",
                "que /proc/meminfo y free informen el mismo total");
    }

    std::vector<ProcesoInfo> procesos;
    std::string fuente;
    const std::string err = sistema::listarProcesos(procesos, fuente);

    revisar(err.empty() && !procesos.empty(),
            "41. lista los procesos del sistema",
            "que alguna fuente devuelva al menos un proceso");

    revisar(!fuente.empty(),
            "42. informa de que fuente salieron los procesos",
            "el enunciado obliga a declarar si se uso comando o API");

    // La herramienta tiene que verse a si misma: si no aparece su propio PID,
    // el listado esta incompleto o el parseo esta mal.
    //
    // El PID propio se pide a sistema::pidPropio() y no a getpid() directo,
    // justamente para que este archivo no necesite un #ifdef. La regla del
    // proyecto es que SistemaInfo.cpp sea el unico con condicionales de
    // plataforma, y una prueba no es excusa para romperla.
    const long propio = sistema::pidPropio();
    bool meVeo = false;
    for (const ProcesoInfo& p : procesos) if (p.pid == propio) meVeo = true;
    revisar(meVeo,
            "43. el listado incluye el propio proceso de pruebas",
            "que el PID de este binario aparezca entre los listados");

    bool pidsValidos = true, nombresValidos = true;
    for (const ProcesoInfo& p : procesos) {
        if (p.pid <= 0)        pidsValidos = false;
        if (p.nombre.empty())  nombresValidos = false;
    }
    revisar(pidsValidos,
            "44. todos los PID son positivos",
            "un PID cero o negativo indicaria un parseo desalineado");

    revisar(nombresValidos,
            "45. ningun proceso queda sin nombre",
            "un nombre vacio indicaria que fallo la extraccion del campo comm");

    const ConsumoPropio c = sistema::consumoPropio();
    revisar(c.ok && c.memResidente > 0,
            "46. mide el consumo de la propia herramienta",
            "resultado experimental obligatorio del enunciado");

    revisar(!sistema::descripcionPlataforma().empty(),
            "47. informa la plataforma y el compilador",
            "para que el informe declare su entorno sin transcribirlo a mano");
}


// ===========================================================================
// 6. ANALISIS DE /proc/[pid]/stat CON LINEAS SINTETICAS
//
// Estas pruebas existen por un hueco que encontro una prueba de mutacion: con
// el analisis embebido dentro de la funcion que abre el archivo, cambiar rfind
// por find -el error clasico de este formato- no hacia fallar NINGUNA de las
// 47 pruebas, porque los procesos reales de la maquina casi nunca tienen
// parentesis en el nombre. Dandole lineas sinteticas, el hueco se cierra.
// ===========================================================================
static void pruebasAnalisisStat() {
    seccion("Analisis de /proc/[pid]/stat (lineas sinteticas)");

    // Linea normal, con los campos hasta el 24. Los valores estan elegidos
    // para poder reconocerlos: utime 111, stime 222, starttime 333, rss 444.
    const std::string base =
        " R 1 100 100 0 -1 4194304 500 0 0 0 111 222 0 0 20 0 1 0 333 99999 444";

    {
        const procfs::CamposStat c = procfs::analizarLineaStat("1234 (bash)" + base);
        revisar(c.ok && c.pid == 1234 && c.nombre == "bash" && c.estado == 'R',
                "48. analiza una linea normal",
                "pid 1234, nombre bash, estado R");
        revisar(c.utime == 111 && c.stime == 222 &&
                c.starttime == 333 && c.paginasResidentes == 444,
                "49. los campos numericos caen en su posicion",
                "utime 111, stime 222, starttime 333, rss 444");
    }

    {
        // LA TRAMPA: nombre con espacios Y parentesis adentro.
        const procfs::CamposStat c =
            procfs::analizarLineaStat("2731 (mi (prog) raro)" + base);
        revisar(c.ok && c.nombre == "mi (prog) raro",
                "50. extrae un nombre con espacios y parentesis",
                "que el nombre sea 'mi (prog) raro' completo");
        revisar(c.estado == 'R' && c.utime == 111 && c.paginasResidentes == 444,
                "51. los campos NO se corren con un nombre asi",
                "que buscar el ultimo ) mantenga alineados estado, utime y rss");
    }

    {
        // Solo espacios, sin parentesis internos: caso intermedio.
        const procfs::CamposStat c =
            procfs::analizarLineaStat("99 (Web Content)" + base);
        revisar(c.ok && c.nombre == "Web Content" && c.utime == 111,
                "52. tolera un nombre con espacios",
                "que 'Web Content' se lea entero y no corra los campos");
    }

    {
        const procfs::CamposStat c = procfs::analizarLineaStat("777 (corto) R 1 2 3");
        revisar(!c.ok,
                "53. rechaza una linea truncada",
                "que una linea sin los 24 campos no se de por buena");
    }

    {
        const procfs::CamposStat c = procfs::analizarLineaStat("basura sin formato");
        revisar(!c.ok,
                "54. rechaza una linea sin parentesis",
                "que no se invente un proceso a partir de texto cualquiera");
    }
}

// ===========================================================================
int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--sin-color") color = false;

    std::printf("\n%sSUITE DE PRUEBAS - Proyecto 4, Administrador de Recursos%s\n",
                NEG(), FIN());
    std::printf("%s%s%s\n", GRIS(), sistema::descripcionPlataforma().c_str(), FIN());

    const fs::path raiz = raizTemporal();
    const fs::path base = raiz / "workspace";

    std::error_code ec;
    fs::remove_all(raiz, ec);

    pruebasSandbox(base);
    pruebaContraejemplo();
    pruebasGestor(base);
    pruebasPermisos();
    pruebasMonitoreo();
    pruebasAnalisisStat();

    // Limpieza. Se hace con error_code para que un fallo de limpieza no se
    // confunda con un fallo de las pruebas.
    fs::remove_all(raiz, ec);
    fs::remove_all(fs::path(base.string() + "_malo"), ec);

    const int total = pasadas + fallidas;
    std::printf("\n  %s", GRIS());
    for (int i = 0; i < 62; ++i) std::printf("-");
    std::printf("%s\n", FIN());

    const char* col = (fallidas == 0) ? VERDE() : ROJO();
    std::printf("  %s%s%d de %d pruebas pasaron%s", NEG(), col, pasadas, total, FIN());
    if (omitidas > 0)
        std::printf("  %s(%d omitidas por la plataforma)%s", AMBAR(), omitidas, FIN());
    std::printf("\n\n");

    return fallidas == 0 ? 0 : 1;
}

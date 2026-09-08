// ---------------------------------------------------------------------------
// reporte.cpp - Tablas y graficos desde los CSV de mediciones
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   Convierte los CSV crudos del benchmark en las tablas y graficos que van al
//   documento IEEE.
//
// POR QUE EXISTE, SI LAS TABLAS SE PODRIAN ESCRIBIR A MANO
//   Porque una cifra transcrita a mano es una cifra que puede no coincidir con
//   la evidencia. En el borrador del Proyecto 2 se citaron numeros que NO
//   estaban en el log committeado, y hubo que corregirlos antes de entregar.
//   Generando el informe desde los datos crudos, cualquier cifra se puede
//   regenerar con un comando y ninguna depende de que alguien copie bien.
//
// QUE ESTADISTICO SE REPORTA
//   Mediana y rango (minimo-maximo), no promedio y desviacion. La razon es el
//   tipo de ruido: en una VM compartida, una corrida puede salir varias veces
//   mas lenta porque otro proceso tomo la CPU en ese momento. Ese valor
//   arrastra el promedio pero no mueve la mediana. El rango se informa igual,
//   porque esconder la dispersion seria presentar los datos como mas limpios de
//   lo que son.
//
// USO
//   ./herramientas/reporte results/benchmark.csv [mas.csv ...] [--ascii] [--sin-color]
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Fila {
    std::string escenario, operacion;
    int    n = 0, repeticion = 0;
    bool   cacheFria = false;
    double us = 0.0;
};

struct Estilo {
    bool color = true, ascii = false;
    const char* rojo()  const { return color ? "\033[31m" : ""; }
    const char* verde() const { return color ? "\033[32m" : ""; }
    const char* ambar() const { return color ? "\033[33m" : ""; }
    const char* cian()  const { return color ? "\033[36m" : ""; }
    const char* gris()  const { return color ? "\033[90m" : ""; }
    const char* neg()   const { return color ? "\033[1m"  : ""; }
    const char* fin()   const { return color ? "\033[0m"  : ""; }
    const char* barra() const { return ascii ? "#" : "█"; }
    const char* linea() const { return ascii ? "-" : "─"; }
};

void regla(const Estilo& e, int n) {
    std::printf("%s", e.gris());
    for (int i = 0; i < n; ++i) std::printf("%s", e.linea());
    std::printf("%s\n", e.fin());
}

void titulo(const Estilo& e, const char* t) {
    std::printf("\n%s%s%s\n", e.neg(), t, e.fin());
    regla(e, 78);
}

double mediana(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t m = v.size() / 2;
    return (v.size() % 2 == 1) ? v[m] : (v[m - 1] + v[m]) / 2.0;
}

bool leerCsv(const std::string& ruta, std::vector<Fila>& salida) {
    std::ifstream f(ruta.c_str());
    if (!f) {
        std::fprintf(stderr, "Aviso: no se pudo abrir %s, se omite.\n", ruta.c_str());
        return false;
    }

    std::string linea;
    if (!std::getline(f, linea)) return false;

    // Se localizan las columnas por NOMBRE y no por posicion, para que agregar
    // una columna al benchmark no rompa el reporte en silencio.
    std::map<std::string, int> col;
    {
        std::stringstream ss(linea);
        std::string campo;
        int i = 0;
        while (std::getline(ss, campo, ',')) col[campo] = i++;
    }
    if (col.find("operacion") == col.end() || col.find("microsegundos") == col.end()) {
        std::fprintf(stderr, "Aviso: %s no tiene el formato esperado, se omite.\n", ruta.c_str());
        return false;
    }

    int n = 0;
    while (std::getline(f, linea)) {
        if (linea.empty()) continue;
        std::vector<std::string> v;
        std::stringstream ss(linea);
        std::string campo;
        while (std::getline(ss, campo, ',')) v.push_back(campo);

        auto texto = [&](const char* k) -> std::string {
            auto it = col.find(k);
            if (it == col.end() || it->second >= static_cast<int>(v.size())) return "";
            return v[static_cast<size_t>(it->second)];
        };

        Fila fl;
        fl.escenario  = texto("escenario");
        fl.operacion  = texto("operacion");
        fl.n          = std::atoi(texto("n").c_str());
        fl.repeticion = std::atoi(texto("repeticion").c_str());
        fl.cacheFria  = texto("cache_fria") == "1";
        fl.us         = std::atof(texto("microsegundos").c_str());
        salida.push_back(fl);
        ++n;
    }
    std::printf("  %s: %d mediciones\n", ruta.c_str(), n);
    return true;
}

// Estadisticos de un grupo de mediciones tibias (sin la de cache fria).
struct Resumen {
    double med = 0.0, minimo = 0.0, maximo = 0.0, fria = 0.0;
    size_t corridas = 0;
    bool   hay = false;
};

Resumen resumir(const std::vector<Fila>& filas, const std::string& esc,
                const std::string& op, int n) {
    std::vector<double> tibias;
    Resumen r;
    for (const Fila& f : filas) {
        if (f.escenario != esc || f.operacion != op || f.n != n) continue;
        if (f.cacheFria) r.fria = f.us; else tibias.push_back(f.us);
    }
    if (tibias.empty()) return r;
    r.hay      = true;
    r.corridas = tibias.size();
    r.med      = mediana(tibias);
    r.minimo   = *std::min_element(tibias.begin(), tibias.end());
    r.maximo   = *std::max_element(tibias.begin(), tibias.end());
    return r;
}

} // namespace

int main(int argc, char** argv) {
    Estilo e;
    std::vector<std::string> rutas;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if      (a == "--ascii")     e.ascii = true;
        else if (a == "--sin-color") e.color = false;
        else if (a == "--ayuda" || a == "-h") {
            std::printf("Uso: %s archivo.csv [mas.csv ...] [--ascii] [--sin-color]\n", argv[0]);
            return 0;
        }
        else rutas.push_back(a);
    }
    if (rutas.empty()) {
        std::fprintf(stderr, "Error: falta al menos un CSV.\n");
        return 1;
    }

    std::printf("\n%sFuentes de datos%s\n", e.neg(), e.fin());
    std::vector<Fila> filas;
    for (const std::string& r : rutas) leerCsv(r, filas);
    if (filas.empty()) {
        std::fprintf(stderr, "Error: no se leyo ninguna medicion.\n");
        return 1;
    }

    std::set<std::string> escenarios, operacionesArchivo, operacionesProceso;
    std::set<int> tamanos;
    for (const Fila& f : filas) {
        escenarios.insert(f.escenario);
        if (f.operacion.rfind("procesos:", 0) == 0) operacionesProceso.insert(f.operacion);
        else { operacionesArchivo.insert(f.operacion); tamanos.insert(f.n); }
    }

    // -----------------------------------------------------------------------
    // TABLA 1 - operaciones de archivo por cantidad de archivos
    // -----------------------------------------------------------------------
    titulo(e, "TABLA 1 - TIEMPO DE LAS OPERACIONES DE ARCHIVO (microsegundos)");
    std::printf("%s  Mediana de las corridas con cache tibia. El rango muestra la dispersion.%s\n",
                e.gris(), e.fin());

    for (const std::string& esc : escenarios) {
        std::printf("\n  %sEscenario: %s%s\n", e.neg(), esc.c_str(), e.fin());
        std::printf("  %s%-12s %8s %12s %10s %10s %12s %8s%s\n", e.neg(),
                    "Operacion", "N", "Mediana", "Minimo", "Maximo",
                    "Cache fria", "us/arch", e.fin());
        regla(e, 78);

        for (const std::string& op : operacionesArchivo) {
            for (int n : tamanos) {
                const Resumen r = resumir(filas, esc, op, n);
                if (!r.hay) continue;
                std::printf("  %-12s %8d %12.1f %10.1f %10.1f %12.1f %8.2f\n",
                            op.c_str(), n, r.med, r.minimo, r.maximo, r.fria,
                            n > 0 ? r.med / n : 0.0);
            }
        }
    }

    // -----------------------------------------------------------------------
    // TABLA 2 - escalamiento
    //
    // La pregunta que esto responde es la del banco de defensa: por que listar
    // 1000 archivos no tarda 100 veces mas que listar 10.
    // -----------------------------------------------------------------------
    titulo(e, "TABLA 2 - ESCALAMIENTO FRENTE AL NUMERO DE ARCHIVOS");
    std::printf("%s  Cuanto se multiplica el tiempo al multiplicar por 10 los archivos.%s\n",
                e.gris(), e.fin());
    std::printf("%s  Un factor de 10 seria escalamiento lineal perfecto.%s\n\n", e.gris(), e.fin());

    for (const std::string& esc : escenarios) {
        std::printf("  %s%s%s\n", e.neg(), esc.c_str(), e.fin());
        std::printf("  %s%-12s %18s %18s%s\n", e.neg(),
                    "Operacion", "factor 10 -> 100", "factor 100 -> 1000", e.fin());
        regla(e, 60);

        for (const std::string& op : operacionesArchivo) {
            const Resumen r10  = resumir(filas, esc, op, 10);
            const Resumen r100 = resumir(filas, esc, op, 100);
            const Resumen r1k  = resumir(filas, esc, op, 1000);
            if (!r10.hay || !r100.hay || !r1k.hay) continue;

            const double f1 = r10.med  > 0 ? r100.med / r10.med  : 0.0;
            const double f2 = r100.med > 0 ? r1k.med  / r100.med : 0.0;
            auto col = [&](double f) { return f < 8.0 ? e.verde() : (f < 12.0 ? e.gris() : e.ambar()); };

            std::printf("  %-12s %s%17.1fx%s %s%17.1fx%s\n",
                        op.c_str(), col(f1), f1, e.fin(), col(f2), f2, e.fin());
        }
        std::printf("\n");
    }

    // -----------------------------------------------------------------------
    // GRAFICO - costo por archivo
    // -----------------------------------------------------------------------
    titulo(e, "GRAFICO - COSTO POR ARCHIVO (microsegundos por archivo)");
    std::printf("%s  Si el costo por archivo baja al crecer N, hay un costo fijo que se reparte.%s\n",
                e.gris(), e.fin());

    for (const std::string& esc : escenarios) {
        double maximo = 0.0;
        for (const std::string& op : operacionesArchivo)
            for (int n : tamanos) {
                const Resumen r = resumir(filas, esc, op, n);
                if (r.hay && n > 0 && r.med / n > maximo) maximo = r.med / n;
            }
        if (maximo <= 0.0) continue;

        std::printf("\n  %s%s%s\n", e.neg(), esc.c_str(), e.fin());
        for (const std::string& op : operacionesArchivo) {
            for (int n : tamanos) {
                const Resumen r = resumir(filas, esc, op, n);
                if (!r.hay || n == 0) continue;
                const double porArchivo = r.med / n;
                const int largo = static_cast<int>(porArchivo / maximo * 40.0 + 0.5);
                std::printf("   %-10s %5d  ", op.c_str(), n);
                std::printf("%s", e.cian());
                for (int k = 0; k < largo; ++k) std::printf("%s", e.barra());
                std::printf("%s %.2f\n", e.fin(), porArchivo);
            }
        }
    }

    // -----------------------------------------------------------------------
    // TABLA 3 - las dos rutas de monitoreo de procesos
    // -----------------------------------------------------------------------
    if (!operacionesProceso.empty()) {
        titulo(e, "TABLA 3 - COSTO DE CADA RUTA DE MONITOREO DE PROCESOS");
        std::printf("%s  Leer /proc no crea procesos; invocar un comando implica fork + exec.%s\n\n",
                    e.gris(), e.fin());

        for (const std::string& esc : escenarios) {
            std::printf("  %s%s%s\n", e.neg(), esc.c_str(), e.fin());
            std::printf("  %s%-34s %12s %10s %10s%s\n", e.neg(),
                        "Ruta", "Mediana(us)", "Minimo", "Maximo", e.fin());
            regla(e, 70);

            double mejor = 0.0;
            std::map<std::string, Resumen> porRuta;
            for (const std::string& op : operacionesProceso) {
                std::vector<double> tibias;
                Resumen r;
                for (const Fila& f : filas) {
                    if (f.escenario != esc || f.operacion != op) continue;
                    if (f.cacheFria) r.fria = f.us; else tibias.push_back(f.us);
                }
                if (tibias.empty()) continue;
                r.hay = true;
                r.med = mediana(tibias);
                r.minimo = *std::min_element(tibias.begin(), tibias.end());
                r.maximo = *std::max_element(tibias.begin(), tibias.end());
                porRuta[op] = r;
                if (mejor == 0.0 || r.med < mejor) mejor = r.med;
            }

            for (const auto& par : porRuta) {
                const bool esMejor = (par.second.med == mejor);
                std::printf("  %s%-34s %12.1f %10.1f %10.1f%s\n",
                            esMejor ? e.verde() : "", par.first.c_str(),
                            par.second.med, par.second.minimo, par.second.maximo, e.fin());
            }

            if (porRuta.size() >= 2 && mejor > 0.0) {
                double peor = 0.0;
                for (const auto& par : porRuta) if (par.second.med > peor) peor = par.second.med;
                std::printf("  %sLa ruta mas rapida es %.1f veces mas barata que la mas lenta.%s\n",
                            e.gris(), peor / mejor, e.fin());
            }
            std::printf("\n");
        }
    }

    // -----------------------------------------------------------------------
    // TABLA 4 - efecto de la carga del sistema
    // -----------------------------------------------------------------------
    if (escenarios.size() >= 2) {
        titulo(e, "TABLA 4 - EFECTO DE LA CARGA DEL SISTEMA");
        std::printf("%s  Cuanto se degrada cada operacion con el sistema bajo presion.%s\n\n",
                    e.gris(), e.fin());

        // La referencia es el sistema EN REPOSO, no el primer escenario en orden
        // alfabetico. Tomar el primero daba "con_carga" como base y los
        // porcentajes salian invertidos: la tabla decia -64.8% donde en realidad
        // la carga habia hecho la operacion casi tres veces mas lenta.
        std::string base = "sin_carga";
        if (escenarios.find(base) == escenarios.end()) base = *escenarios.begin();

        std::printf("  %s%-12s %8s %14s", e.neg(), "Operacion", "N", base.c_str());
        for (const std::string& esc : escenarios)
            if (esc != base) std::printf(" %14s %11s", esc.c_str(), "degradacion");
        std::printf("%s\n", e.fin());
        regla(e, 78);

        for (const std::string& op : operacionesArchivo) {
            for (int n : tamanos) {
                const Resumen rb = resumir(filas, base, op, n);
                if (!rb.hay) continue;
                std::printf("  %-12s %8d %14.1f", op.c_str(), n, rb.med);
                for (const std::string& esc : escenarios) {
                    if (esc == base) continue;
                    const Resumen r = resumir(filas, esc, op, n);
                    if (!r.hay) { std::printf(" %14s %11s", "-", "-"); continue; }
                    // Positivo = la carga lo hizo mas lento, que es lo esperable.
                    const double cambio = rb.med > 0 ? (r.med / rb.med - 1.0) * 100.0 : 0.0;
                    const char* col = cambio > 50.0 ? e.rojo()
                                    : (cambio > 15.0 ? e.ambar() : e.gris());
                    std::printf(" %14.1f %s%+10.1f%%%s", r.med, col, cambio, e.fin());
                }
                std::printf("\n");
            }
        }
        std::printf("\n%s  La referencia es el sistema en reposo. Un porcentaje positivo significa%s\n",
                    e.gris(), e.fin());
        std::printf("%s  que la carga hizo la operacion mas lenta, que es lo esperable. Si alguno%s\n",
                    e.gris(), e.fin());
        std::printf("%s  saliera negativo seria ruido de la maquina, no una mejora real.%s\n",
                    e.gris(), e.fin());
    }

    std::printf("\n");
    return 0;
}

// ---------------------------------------------------------------------------
// GENERADOR DE REPORTES
// TIIT2007 Sistemas Operativos - Universidad Invenio
//
// Lee los CSV producidos por las versiones (--csv) y arma las dos tablas
// obligatorias del protocolo experimental, mas el analisis de overhead.
//
// Se genera DESDE LOS DATOS CRUDOS a proposito: cualquier cifra del informe se
// puede regenerar con un comando y ninguna se transcribe a mano.
//
// Uso: ./reporte archivo.csv [mas.csv ...] [--ascii] [--sin-color]
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct Fila {
    std::string version;
    int  capacidad = 0, productores = 0, consumidores = 0;
    long items_por_productor = 0;
    long producidos = 0, consumidos = 0;
    long suma_producida = 0, suma_consumida = 0;
    long perdidos = 0, duplicados = 0, fantasma = 0, corruptos = 0, entregados_ok = 0;
    int  max_cuenta = 0, excedio = 0, invariante_ok = 0;
    double ms = 0.0;

    std::string config() const {
        char b[48];
        snprintf(b, sizeof(b), "%d,%d,%d", capacidad, productores, consumidores);
        return std::string(b);
    }
};

struct Estilo {
    const char *h, *v, *cruz, *bloque;
    const char *rojo, *verde, *gris, *negrita, *fin;
};

static Estilo estilo(bool ascii, bool color) {
    Estilo e;
    e.h = ascii ? "-" : "─";
    e.v = "|";
    e.cruz = "+";
    e.bloque = ascii ? "#" : "█";
    e.rojo   = color ? "\033[31m" : "";
    e.verde  = color ? "\033[32m" : "";
    e.gris   = color ? "\033[90m" : "";
    e.negrita= color ? "\033[1m"  : "";
    e.fin    = color ? "\033[0m"  : "";
    return e;
}

static void regla(const Estilo& e, int n) {
    printf("%s", e.gris);
    for (int i = 0; i < n; i++) printf("%s", e.h);
    printf("%s\n", e.fin);
}

static void titulo(const Estilo& e, const char* t) {
    printf("\n%s%s%s\n", e.negrita, t, e.fin);
    regla(e, 78);
}

static double media(const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    double s = 0;
    for (size_t i = 0; i < v.size(); i++) s += v[i];
    return s / static_cast<double>(v.size());
}

// Desviacion estandar MUESTRAL (n-1). Con 5 corridas el estimador con n
// subestimaria la dispersion.
static double desviacion(const std::vector<double>& v) {
    if (v.size() < 2) return 0.0;
    const double m = media(v);
    double s = 0;
    for (size_t i = 0; i < v.size(); i++) s += (v[i] - m) * (v[i] - m);
    return std::sqrt(s / static_cast<double>(v.size() - 1));
}

static bool leerCsv(const std::string& ruta, std::vector<Fila>& salida) {
    std::ifstream f(ruta.c_str());
    if (!f.is_open()) {
        std::cerr << "Aviso: no se pudo abrir " << ruta << ", se omite.\n";
        return false;
    }
    std::string linea;
    if (!std::getline(f, linea)) return false;

    // Se indexa por NOMBRE de columna, no por posicion: si el simulador agrega
    // campos nuevos, este lector sigue funcionando.
    std::map<std::string, int> col;
    {
        std::stringstream ss(linea);
        std::string campo; int i = 0;
        while (std::getline(ss, campo, ',')) col[campo] = i++;
    }
    if (col.find("version") == col.end() || col.find("milisegundos") == col.end()) {
        std::cerr << "Aviso: " << ruta << " no tiene el formato esperado, se omite.\n";
        return false;
    }

    int n = 0;
    while (std::getline(f, linea)) {
        if (linea.empty()) continue;
        std::vector<std::string> v;
        std::stringstream ss(linea);
        std::string campo;
        while (std::getline(ss, campo, ',')) v.push_back(campo);

        auto txt = [&](const char* k) -> std::string {
            std::map<std::string, int>::iterator it = col.find(k);
            if (it == col.end() || it->second >= static_cast<int>(v.size())) return "";
            return v[static_cast<size_t>(it->second)];
        };
        auto num = [&](const char* k) -> double { return std::atof(txt(k).c_str()); };

        Fila fl;
        fl.version             = txt("version");
        fl.capacidad           = static_cast<int>(num("capacidad"));
        fl.productores         = static_cast<int>(num("productores"));
        fl.consumidores        = static_cast<int>(num("consumidores"));
        fl.items_por_productor = static_cast<long>(num("items_por_productor"));
        fl.producidos          = static_cast<long>(num("producidos"));
        fl.consumidos          = static_cast<long>(num("consumidos"));
        fl.suma_producida      = static_cast<long>(num("suma_producida"));
        fl.suma_consumida      = static_cast<long>(num("suma_consumida"));
        fl.perdidos            = static_cast<long>(num("perdidos"));
        fl.duplicados          = static_cast<long>(num("duplicados"));
        fl.fantasma            = static_cast<long>(num("fantasma"));
        fl.corruptos           = static_cast<long>(num("corruptos"));
        fl.entregados_ok       = static_cast<long>(num("entregados_ok"));
        fl.max_cuenta          = static_cast<int>(num("max_cuenta"));
        fl.excedio             = static_cast<int>(num("excedio_capacidad"));
        fl.invariante_ok       = static_cast<int>(num("invariante_ok"));
        fl.ms                  = num("milisegundos");
        salida.push_back(fl);
        n++;
    }
    printf("  %s: %d corridas\n", ruta.c_str(), n);
    return true;
}

int main(int argc, char** argv) {
    std::vector<std::string> rutas;
    bool ascii = false, color = true;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if      (a == "--ascii")     ascii = true;
        else if (a == "--sin-color") color = false;
        else if (a == "--ayuda" || a == "-h") {
            printf("Uso: %s archivo.csv [mas.csv ...] [--ascii] [--sin-color]\n", argv[0]);
            return 0;
        }
        else rutas.push_back(a);
    }
    if (rutas.empty()) { std::cerr << "Error: falta al menos un CSV.\n"; return 1; }

    const Estilo e = estilo(ascii, color);

    printf("\n%sFuentes de datos%s\n", e.negrita, e.fin);
    std::vector<Fila> filas;
    for (size_t i = 0; i < rutas.size(); i++) leerCsv(rutas[i], filas);
    if (filas.empty()) { std::cerr << "Error: no se leyo ninguna corrida.\n"; return 1; }

    // ---------------------------------------------------------------
    // TABLA 1 - detalle por corrida (plantilla obligatoria)
    // ---------------------------------------------------------------
    titulo(e, "TABLA 1 - RESULTADOS POR CORRIDA");
    printf("%s %-4s %-4s %-10s %10s %10s %12s %12s %6s %6s %10s%s\n", e.negrita,
           "Corr", "Ver", "Config", "Producidos", "Consumidos",
           "Suma prod.", "Suma cons.", "OcMax", "Inv?", "Tiempo(ms)", e.fin);
    regla(e, 100);

    // Agrupadas por version y configuracion, en orden estable.
    std::map<std::string, std::vector<Fila> > grupos;
    for (size_t i = 0; i < filas.size(); i++)
        grupos[filas[i].config() + "|" + filas[i].version].push_back(filas[i]);

    for (std::map<std::string, std::vector<Fila> >::iterator g = grupos.begin();
         g != grupos.end(); ++g) {
        for (size_t i = 0; i < g->second.size(); i++) {
            const Fila& f = g->second[i];
            const bool ok = f.invariante_ok != 0;
            printf(" %-4d %-4s %-10s %10ld %10ld %12ld %12ld %6d %s%6s%s %10.3f\n",
                   static_cast<int>(i + 1), f.version.c_str(), f.config().c_str(),
                   f.producidos, f.consumidos, f.suma_producida, f.suma_consumida,
                   f.max_cuenta, ok ? e.verde : e.rojo, ok ? "OK" : "VIOL",
                   e.fin, f.ms);
        }
    }

    // ---------------------------------------------------------------
    // TABLA 2 - resumen por version y configuracion
    // ---------------------------------------------------------------
    titulo(e, "TABLA 2 - RESUMEN POR VERSION Y CONFIGURACION");
    printf("%s %-4s %-10s %10s %10s %10s %10s %10s%s\n", e.negrita,
           "Ver", "Config", "Media(ms)", "Desv.est.", "Min", "Max", "Violados", e.fin);
    regla(e, 80);

    std::map<std::string, std::map<std::string, std::vector<Fila> > > porConfig;
    for (size_t i = 0; i < filas.size(); i++)
        porConfig[filas[i].config()][filas[i].version].push_back(filas[i]);

    for (std::map<std::string, std::map<std::string, std::vector<Fila> > >::iterator c
         = porConfig.begin(); c != porConfig.end(); ++c) {
        for (std::map<std::string, std::vector<Fila> >::iterator v = c->second.begin();
             v != c->second.end(); ++v) {
            std::vector<double> t;
            int violados = 0;
            for (size_t i = 0; i < v->second.size(); i++) {
                t.push_back(v->second[i].ms);
                if (v->second[i].invariante_ok == 0) violados++;
            }
            std::sort(t.begin(), t.end());
            const char* c1 = (v->first == "A") ? e.rojo : e.verde;
            printf(" %s%-4s%s %-10s %10.3f %10.3f %10.3f %10.3f %7d/%-2d\n",
                   c1, v->first.c_str(), e.fin, c->first.c_str(),
                   media(t), desviacion(t), t.front(), t.back(),
                   violados, static_cast<int>(t.size()));
        }
    }

    // ---------------------------------------------------------------
    // OVERHEAD
    // ---------------------------------------------------------------
    titulo(e, "OVERHEAD DE LA SINCRONIZACION");
    printf("%s  overhead %% = (T_B - T_A) / T_A x 100%s\n\n", e.gris, e.fin);
    printf("%s %-10s %12s %12s %12s%s\n", e.negrita,
           "Config", "T_A (ms)", "T_B (ms)", "Overhead %", e.fin);
    regla(e, 52);

    double maxAbs = 0;
    std::vector<std::pair<std::string, double> > over;
    for (std::map<std::string, std::map<std::string, std::vector<Fila> > >::iterator c
         = porConfig.begin(); c != porConfig.end(); ++c) {
        if (c->second.find("A") == c->second.end() || c->second.find("B") == c->second.end())
            continue;
        std::vector<double> ta, tb;
        for (size_t i = 0; i < c->second["A"].size(); i++) ta.push_back(c->second["A"][i].ms);
        for (size_t i = 0; i < c->second["B"].size(); i++) tb.push_back(c->second["B"][i].ms);
        const double ma = media(ta), mb = media(tb);
        if (ma <= 0) continue;
        const double ov = (mb - ma) / ma * 100.0;
        over.push_back(std::make_pair(c->first, ov));
        if (std::fabs(ov) > maxAbs) maxAbs = std::fabs(ov);
        printf(" %-10s %12.3f %12.3f %s%11.1f%%%s\n",
               c->first.c_str(), ma, mb, e.rojo, ov, e.fin);
    }

    if (!over.empty()) {
        printf("\n%s  Magnitud relativa del overhead por configuracion%s\n", e.gris, e.fin);
        for (size_t i = 0; i < over.size(); i++) {
            printf("  %-10s ", over[i].first.c_str());
            int n = maxAbs > 0 ? static_cast<int>(std::fabs(over[i].second) / maxAbs * 34 + 0.5) : 0;
            printf("%s", e.rojo);
            for (int k = 0; k < n; k++) printf("%s", e.bloque);
            printf("%s %.1f%%\n", e.fin, over[i].second);
        }
    }

    printf("\n%s  ADVERTENCIA METODOLOGICA: el tiempo de la version A no es una%s\n", e.gris, e.fin);
    printf("%s  referencia valida en sentido estricto, porque corresponde a un%s\n", e.gris, e.fin);
    printf("%s  programa incorrecto: pierde y duplica items, de modo que no realiza%s\n", e.gris, e.fin);
    printf("%s  el mismo trabajo que la version B. El overhead debe leerse como el%s\n", e.gris, e.fin);
    printf("%s  precio de pasar de un resultado incorrecto a uno correcto, no como%s\n", e.gris, e.fin);
    printf("%s  una comparacion entre dos soluciones equivalentes.%s\n\n", e.gris, e.fin);

    return 0;
}

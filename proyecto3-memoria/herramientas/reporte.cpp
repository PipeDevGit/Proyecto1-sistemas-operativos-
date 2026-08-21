// ---------------------------------------------------------------------------
// Generador de reportes - Proyecto 3
// TIIT2007 Sistemas Operativos - Universidad Invenio
//
// Lee los CSV producidos por el simulador (--csv) y arma las tablas y
// graficos del documento IEEE.
//
// Se genera DESDE LOS DATOS CRUDOS a proposito: cualquier cifra del informe
// se puede regenerar con un comando y ninguna se transcribe a mano.
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
#include <set>
#include <sstream>
#include <string>
#include <vector>

struct Fila {
    std::string algoritmo, cadena;
    int    marcos = 0;
    long   referencias = 0, aciertos = 0, fallos = 0;
    double tasa = 0.0, microsegundos = 0.0;
};

struct Estilo {
    const char *h, *bloque, *rojo, *verde, *ambar, *gris, *negrita, *fin;
};

static Estilo estilo(bool ascii, bool color) {
    Estilo e;
    e.h      = ascii ? "-" : "─";
    e.bloque = ascii ? "#" : "█";
    e.rojo    = color ? "\033[31m" : "";
    e.verde   = color ? "\033[32m" : "";
    e.ambar   = color ? "\033[33m" : "";
    e.gris    = color ? "\033[90m" : "";
    e.negrita = color ? "\033[1m"  : "";
    e.fin     = color ? "\033[0m"  : "";
    return e;
}

static void regla(const Estilo& e, int n) {
    printf("%s", e.gris);
    for (int i = 0; i < n; i++) printf("%s", e.h);
    printf("%s\n", e.fin);
}

static void titulo(const Estilo& e, const char* t) {
    printf("\n%s%s%s\n", e.negrita, t, e.fin);
    regla(e, 74);
}

// Nombre corto y legible de la cadena, a partir de la ruta del archivo.
static std::string corto(const std::string& ruta) {
    size_t a = ruta.find_last_of("/\\");
    std::string s = (a == std::string::npos) ? ruta : ruta.substr(a + 1);
    size_t p = s.find_last_of('.');
    if (p != std::string::npos) s = s.substr(0, p);
    return s;
}

static bool leerCsv(const std::string& ruta, std::vector<Fila>& salida) {
    std::ifstream f(ruta.c_str());
    if (!f.is_open()) {
        std::cerr << "Aviso: no se pudo abrir " << ruta << ", se omite.\n";
        return false;
    }
    std::string linea;
    if (!std::getline(f, linea)) return false;

    std::map<std::string, int> col;
    {
        std::stringstream ss(linea);
        std::string campo; int i = 0;
        while (std::getline(ss, campo, ',')) col[campo] = i++;
    }
    if (col.find("algoritmo") == col.end() || col.find("fallos") == col.end()) {
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
            std::map<std::string,int>::iterator it = col.find(k);
            if (it == col.end() || it->second >= static_cast<int>(v.size())) return "";
            return v[static_cast<size_t>(it->second)];
        };
        auto num = [&](const char* k) { return std::atof(txt(k).c_str()); };

        Fila fl;
        fl.algoritmo     = txt("algoritmo");
        fl.cadena        = corto(txt("cadena"));
        fl.marcos        = static_cast<int>(num("marcos"));
        fl.referencias   = static_cast<long>(num("referencias"));
        fl.aciertos      = static_cast<long>(num("aciertos"));
        fl.fallos        = static_cast<long>(num("fallos"));
        fl.tasa          = num("tasa_fallos");
        fl.microsegundos = num("microsegundos");
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

    // Indice: cadena -> marcos -> algoritmo -> fila
    std::map<std::string, std::map<int, std::map<std::string, Fila> > > idx;
    std::set<std::string> algoritmos;
    for (size_t i = 0; i < filas.size(); i++) {
        idx[filas[i].cadena][filas[i].marcos][filas[i].algoritmo] = filas[i];
        algoritmos.insert(filas[i].algoritmo);
    }

    // ---------------------------------------------------------------
    // TABLA 1 - fallos por algoritmo y tamano de marco
    // ---------------------------------------------------------------
    titulo(e, "TABLA 1 - FALLOS DE PAGINA POR ALGORITMO Y TAMANO DE MARCO");

    for (std::map<std::string, std::map<int, std::map<std::string, Fila> > >::iterator c
         = idx.begin(); c != idx.end(); ++c) {
        long refs = 0;
        for (std::map<int, std::map<std::string, Fila> >::iterator m = c->second.begin();
             m != c->second.end(); ++m)
            for (std::map<std::string, Fila>::iterator a = m->second.begin();
                 a != m->second.end(); ++a) refs = a->second.referencias;

        printf("\n  %s%s%s  (%ld referencias)\n", e.negrita, c->first.c_str(), e.fin, refs);
        printf("  %s%-8s", e.negrita, "Marcos");
        for (std::set<std::string>::iterator a = algoritmos.begin(); a != algoritmos.end(); ++a)
            printf("%12s", a->c_str());
        printf("   %-14s%s\n", "Mejor", e.fin);
        regla(e, 8 + 12 * static_cast<int>(algoritmos.size()) + 16);

        for (std::map<int, std::map<std::string, Fila> >::iterator m = c->second.begin();
             m != c->second.end(); ++m) {
            printf("  %-8d", m->first);
            long mejor = -1;
            std::string nombreMejor;
            for (std::set<std::string>::iterator a = algoritmos.begin(); a != algoritmos.end(); ++a) {
                std::map<std::string, Fila>::iterator f = m->second.find(*a);
                if (f == m->second.end()) continue;   // esta pasada solo detecta, no imprime
                if (mejor < 0 || f->second.fallos < mejor) { mejor = f->second.fallos; nombreMejor = *a; }
            }
            for (std::set<std::string>::iterator a = algoritmos.begin(); a != algoritmos.end(); ++a) {
                std::map<std::string, Fila>::iterator f = m->second.find(*a);
                if (f == m->second.end()) { printf("%12s", "-"); continue; }
                const bool esMejor = (f->second.fallos == mejor);
                printf("%s%12ld%s", esMejor ? e.verde : "", f->second.fallos, esMejor ? e.fin : "");
            }
            printf("   %s%-14s%s\n", e.gris, nombreMejor.c_str(), e.fin);
        }
    }

    // ---------------------------------------------------------------
    // TABLA 2 - fallos vs tamano de marco, en barras
    // ---------------------------------------------------------------
    titulo(e, "GRAFICO - FALLOS FRENTE AL TAMANO DE MARCO");
    printf("%s  Cada barra es proporcional a los fallos de esa combinacion.%s\n", e.gris, e.fin);

    for (std::map<std::string, std::map<int, std::map<std::string, Fila> > >::iterator c
         = idx.begin(); c != idx.end(); ++c) {
        long maximo = 0;
        for (std::map<int, std::map<std::string, Fila> >::iterator m = c->second.begin();
             m != c->second.end(); ++m)
            for (std::map<std::string, Fila>::iterator a = m->second.begin();
                 a != m->second.end(); ++a)
                if (a->second.fallos > maximo) maximo = a->second.fallos;
        if (maximo == 0) continue;

        printf("\n  %s%s%s\n", e.negrita, c->first.c_str(), e.fin);
        for (std::map<int, std::map<std::string, Fila> >::iterator m = c->second.begin();
             m != c->second.end(); ++m) {
            for (std::map<std::string, Fila>::iterator a = m->second.begin();
                 a != m->second.end(); ++a) {
                printf("   %2d marcos %-7s ", m->first, a->first.c_str());
                const int n = static_cast<int>(
                    static_cast<double>(a->second.fallos) / static_cast<double>(maximo) * 38.0 + 0.5);
                const char* col = a->first == "FIFO" ? e.ambar
                                 : (a->first == "LRU" ? e.verde : e.gris);
                printf("%s", col);
                for (int k = 0; k < n; k++) printf("%s", e.bloque);
                printf("%s %ld\n", e.fin, a->second.fallos);
            }
        }
    }

    // ---------------------------------------------------------------
    // DETECCION DE LA ANOMALIA DE BELADY
    // ---------------------------------------------------------------
    titulo(e, "ANOMALIA DE BELADY");
    printf("%s  Se marca cuando aumentar el numero de marcos AUMENTA los fallos.%s\n\n", e.gris, e.fin);

    int hallazgos = 0;
    for (std::map<std::string, std::map<int, std::map<std::string, Fila> > >::iterator c
         = idx.begin(); c != idx.end(); ++c) {
        for (std::set<std::string>::iterator a = algoritmos.begin(); a != algoritmos.end(); ++a) {
            long anterior = -1;
            int  marcoAnterior = 0;
            for (std::map<int, std::map<std::string, Fila> >::iterator m = c->second.begin();
                 m != c->second.end(); ++m) {
                std::map<std::string, Fila>::iterator f = m->second.find(*a);
                if (f == m->second.end()) continue;
                if (anterior >= 0 && f->second.fallos > anterior) {
                    printf("  %s%s ANOMALIA%s  %-22s %-7s  %d marcos: %ld fallos"
                           "  ->  %d marcos: %ld fallos\n",
                           e.rojo, e.bloque, e.fin, c->first.c_str(), a->c_str(),
                           marcoAnterior, anterior, m->first, f->second.fallos);
                    hallazgos++;
                }
                anterior = f->second.fallos;
                marcoAnterior = m->first;
            }
        }
    }
    if (hallazgos == 0)
        printf("  %sNo se detecto ninguna anomalia en los datos cargados.%s\n", e.gris, e.fin);
    else
        printf("\n%s  FIFO puede sufrirla porque no distingue paginas utiles de inutiles.%s\n", e.gris, e.fin);
    printf("%s  LRU NO puede sufrirla: pertenece a la familia de algoritmos de pila,%s\n", e.gris, e.fin);
    printf("%s  en los que el conjunto residente con N marcos siempre esta contenido%s\n", e.gris, e.fin);
    printf("%s  en el de N+1 marcos.%s\n", e.gris, e.fin);

    // ---------------------------------------------------------------
    // TIEMPOS
    // ---------------------------------------------------------------
    titulo(e, "TIEMPO DE SIMULACION");
    printf("%s  Microsegundos por corrida. Mide el costo del algoritmo, no del SO.%s\n\n", e.gris, e.fin);
    printf("  %s%-22s %-8s %10s %14s%s\n", e.negrita, "Cadena", "Marcos", "Algoritmo", "Tiempo(us)", e.fin);
    regla(e, 60);
    for (std::map<std::string, std::map<int, std::map<std::string, Fila> > >::iterator c
         = idx.begin(); c != idx.end(); ++c)
        for (std::map<int, std::map<std::string, Fila> >::iterator m = c->second.begin();
             m != c->second.end(); ++m)
            for (std::map<std::string, Fila>::iterator a = m->second.begin();
                 a != m->second.end(); ++a)
                printf("  %-22s %-8d %10s %14.1f\n", c->first.c_str(), m->first,
                       a->first.c_str(), a->second.microsegundos);
    printf("\n");
    return 0;
}

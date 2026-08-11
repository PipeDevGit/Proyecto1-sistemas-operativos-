// ---------------------------------------------------------------------------
// GENERADOR DE REPORTES
// TIIT2007 Sistemas Operativos - Universidad Invenio
//
// Lee uno o mas CSV producidos por el simulador (--csv) y arma tablas y
// graficos de barras para el documento IEEE.
//
// Se genera DESDE LOS DATOS CRUDOS a proposito: cualquier numero del informe
// se puede regenerar con un comando, y no hay tablas escritas a mano que
// puedan desincronizarse de los resultados reales.
//
// Uso:  ./reporte results/pareado_ABC.csv [mas.csv ...] [--ascii] [--sin-color]
// ---------------------------------------------------------------------------
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct Fila {
    char   variante = '?';
    int    capacidad = 0;
    long   producidos = 0, consumidos = 0;
    long   perdidos = 0, duplicados = 0, fantasma = 0, corruptos = 0;
    long   inconsistencias = 0, entregados_ok = 0;
    int    max_cuenta = 0, violo_capacidad = 0;
    long   drenados = 0, timeouts = 0;
    double segundos = 0.0;

    double usPorItem() const {
        return entregados_ok > 0 ? segundos * 1e6 / static_cast<double>(entregados_ok) : 0.0;
    }
    double porcentajeOk() const {
        return producidos > 0 ? 100.0 * static_cast<double>(entregados_ok)
                                      / static_cast<double>(producidos) : 0.0;
    }
};

// --- apariencia -------------------------------------------------------------
struct Estilo {
    const char *esqSI, *esqSD, *esqII, *esqID, *horiz, *vert;
    const char *unionS, *unionI, *unionIzq, *unionDer, *cruz;
    const char *bloque, *medioBloque;
    const char *rojo, *verde, *ambar, *gris, *negrita, *fin;
};

static Estilo estiloUnicode(bool color) {
    Estilo e;
    e.esqSI = "┌"; e.esqSD = "┐"; e.esqII = "└"; e.esqID = "┘";
    e.horiz = "─"; e.vert = "│";
    e.unionS = "┬"; e.unionI = "┴";
    e.unionIzq = "├"; e.unionDer = "┤"; e.cruz = "┼";
    e.bloque = "█"; e.medioBloque = "▌";
    e.rojo = color ? "\033[31m" : ""; e.verde = color ? "\033[32m" : "";
    e.ambar = color ? "\033[33m" : ""; e.gris = color ? "\033[90m" : "";
    e.negrita = color ? "\033[1m" : ""; e.fin = color ? "\033[0m" : "";
    return e;
}

static Estilo estiloAscii(bool color) {
    Estilo e = estiloUnicode(color);
    e.esqSI = "+"; e.esqSD = "+"; e.esqII = "+"; e.esqID = "+";
    e.horiz = "-"; e.vert = "|";
    e.unionS = "+"; e.unionI = "+";
    e.unionIzq = "+"; e.unionDer = "+"; e.cruz = "+";
    e.bloque = "#"; e.medioBloque = "=";
    return e;
}

static const char* nombreVariante(char v) {
    switch (v) {
        case 'A': return "sin sincronizacion";
        case 'B': return "mutex + espera activa";
        case 'C': return "mutex + condition_variable";
        case 'D': return "semaforos";
        default:  return "?";
    }
}

// --- utilidades de dibujo ---------------------------------------------------
static void repetir(const char* s, int n) { for (int i = 0; i < n; i++) printf("%s", s); }

static void linea(const Estilo& e, const std::vector<int>& anchos,
                  const char* izq, const char* medio, const char* der) {
    printf("%s%s", e.gris, izq);
    for (size_t i = 0; i < anchos.size(); i++) {
        repetir(e.horiz, anchos[i] + 2);
        if (i + 1 < anchos.size()) printf("%s", medio);
    }
    printf("%s%s\n", der, e.fin);
}

static void filaTexto(const Estilo& e, const std::vector<int>& anchos,
                      const std::vector<std::string>& celdas,
                      const std::vector<std::string>& colores) {
    for (size_t i = 0; i < celdas.size(); i++) {
        printf("%s%s%s ", e.gris, e.vert, e.fin);
        const std::string& c = celdas[i];
        const std::string& col = i < colores.size() ? colores[i] : std::string();
        // Numerico a la derecha, texto a la izquierda.
        bool numerico = !c.empty() &&
                        (isdigit(static_cast<unsigned char>(c[0])) || c[0] == '-' || c[0] == '~');
        int relleno = anchos[i] - static_cast<int>(c.size());
        if (relleno < 0) relleno = 0;
        if (numerico) { repetir(" ", relleno); printf("%s%s%s ", col.c_str(), c.c_str(), e.fin); }
        else { printf("%s%s%s", col.c_str(), c.c_str(), e.fin); repetir(" ", relleno); printf(" "); }
    }
    printf("%s%s%s\n", e.gris, e.vert, e.fin);
}

static void titulo(const Estilo& e, const char* texto) {
    printf("\n%s%s%s\n", e.negrita, texto, e.fin);
    printf("%s", e.gris); repetir(e.horiz, 62); printf("%s\n", e.fin);
}

static void barra(const Estilo& e, double valor, double maximo, int ancho, const char* color) {
    int llenos = maximo > 0 ? static_cast<int>(valor / maximo * ancho + 0.5) : 0;
    if (llenos > ancho) llenos = ancho;
    if (llenos < 1 && valor > 0) llenos = 1;
    printf("%s", color);
    repetir(e.bloque, llenos);
    printf("%s%s", e.fin, e.gris);
    repetir(" ", ancho - llenos);
    printf("%s", e.fin);
}

// --- lectura de CSV ---------------------------------------------------------
static bool leerCsv(const std::string& ruta, std::vector<Fila>& salida) {
    std::ifstream f(ruta.c_str());
    if (!f.is_open()) {
        std::cerr << "Aviso: no se pudo abrir " << ruta << ", se omite.\n";
        return false;
    }
    std::string linea;
    if (!std::getline(f, linea)) return false;

    // Se indexa por NOMBRE de columna, no por posicion: si el simulador
    // agrega campos nuevos al CSV, este lector sigue funcionando.
    std::map<std::string, int> col;
    {
        std::stringstream ss(linea);
        std::string campo;
        int i = 0;
        while (std::getline(ss, campo, ',')) col[campo] = i++;
    }
    const char* obligatorias[] = {"variante", "capacidad", "entregados_ok", "segundos"};
    for (int i = 0; i < 4; i++) {
        if (col.find(obligatorias[i]) == col.end()) {
            std::cerr << "Aviso: " << ruta << " no tiene la columna '"
                      << obligatorias[i] << "', se omite.\n";
            return false;
        }
    }

    int leidas = 0;
    while (std::getline(f, linea)) {
        if (linea.empty()) continue;
        std::vector<std::string> v;
        std::stringstream ss(linea);
        std::string campo;
        while (std::getline(ss, campo, ',')) v.push_back(campo);

        auto num = [&](const char* nombre) -> double {
            std::map<std::string, int>::iterator it = col.find(nombre);
            if (it == col.end() || it->second >= static_cast<int>(v.size())) return 0.0;
            return std::atof(v[static_cast<size_t>(it->second)].c_str());
        };

        Fila fl;
        int iv = col["variante"];
        if (iv < static_cast<int>(v.size()) && !v[static_cast<size_t>(iv)].empty())
            fl.variante = v[static_cast<size_t>(iv)][0];
        fl.capacidad       = static_cast<int>(num("capacidad"));
        fl.producidos      = static_cast<long>(num("producidos"));
        fl.consumidos      = static_cast<long>(num("consumidos"));
        fl.perdidos        = static_cast<long>(num("perdidos"));
        fl.duplicados      = static_cast<long>(num("duplicados"));
        fl.fantasma        = static_cast<long>(num("fantasma"));
        fl.corruptos       = static_cast<long>(num("corruptos"));
        fl.inconsistencias = static_cast<long>(num("inconsistencias"));
        fl.entregados_ok   = static_cast<long>(num("entregados_ok"));
        fl.max_cuenta      = static_cast<int>(num("max_cuenta"));
        fl.violo_capacidad = static_cast<int>(num("violo_capacidad"));
        fl.drenados        = static_cast<long>(num("drenados"));
        fl.timeouts        = static_cast<long>(num("timeouts"));
        fl.segundos        = num("segundos");
        salida.push_back(fl);
        leidas++;
    }
    printf("%s  %s: %d corridas%s\n", "", ruta.c_str(), leidas, "");
    return true;
}

static double mediana(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return n % 2 ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2.0;
}

static std::string fmt(double x, int dec) {
    char b[64];
    snprintf(b, sizeof(b), "%.*f", dec, x);
    return std::string(b);
}
static std::string fmtL(long x) {
    char b[64];
    snprintf(b, sizeof(b), "%ld", x);
    return std::string(b);
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
            printf("  --ascii      usa +-| en vez de caracteres de recuadro Unicode\n");
            printf("  --sin-color  desactiva los colores ANSI\n");
            return 0;
        }
        else rutas.push_back(a);
    }
    if (rutas.empty()) {
        std::cerr << "Error: falta al menos un archivo CSV. Use --ayuda.\n";
        return 1;
    }

    const Estilo e = ascii ? estiloAscii(color) : estiloUnicode(color);

    printf("\n%sFuentes de datos%s\n", e.negrita, e.fin);
    std::vector<Fila> filas;
    for (size_t i = 0; i < rutas.size(); i++) leerCsv(rutas[i], filas);
    if (filas.empty()) {
        std::cerr << "Error: no se leyo ninguna corrida.\n";
        return 1;
    }

    // Las secciones 2 y 3 comparan variantes entre si, asi que solo tienen
    // sentido a capacidad FIJA: mezclar capacidades distintas en las mismas
    // barras compararia configuraciones que no son equivalentes. Se elige la
    // capacidad con mas corridas y se deja dicho cual es.
    std::map<int, int> cuentaCap;
    for (size_t i = 0; i < filas.size(); i++) cuentaCap[filas[i].capacidad]++;
    int capElegida = filas[0].capacidad;
    for (std::map<int, int>::iterator it = cuentaCap.begin(); it != cuentaCap.end(); ++it)
        if (it->second > cuentaCap[capElegida]) capElegida = it->first;
    const bool variasCaps = cuentaCap.size() > 1;

    std::vector<Fila> fijas;
    for (size_t i = 0; i < filas.size(); i++)
        if (filas[i].capacidad == capElegida) fijas.push_back(filas[i]);

    if (variasCaps) {
        printf("%s  Se detectaron %d capacidades distintas. Las secciones 2 y 3 usan solo%s\n",
               e.ambar, static_cast<int>(cuentaCap.size()), e.fin);
        printf("%s  capacidad = %d (%d corridas); la seccion 4 usa todas.%s\n",
               e.ambar, capElegida, cuentaCap[capElegida], e.fin);
    }

    // ---------------------------------------------------------------------
    // 1. Resumen por variante  (usa TODAS las corridas: la correccion no
    //    depende de la capacidad)
    // ---------------------------------------------------------------------
    titulo(e, "1. CORRECCION POR VARIANTE");

    std::map<char, std::vector<Fila> > porVar;
    for (size_t i = 0; i < filas.size(); i++) porVar[filas[i].variante].push_back(filas[i]);

    std::map<char, std::vector<Fila> > porVarFija;
    for (size_t i = 0; i < fijas.size(); i++) porVarFija[fijas[i].variante].push_back(fijas[i]);

    std::vector<int> anchos;
    anchos.push_back(8); anchos.push_back(26); anchos.push_back(8);
    anchos.push_back(20); anchos.push_back(9);

    linea(e, anchos, e.esqSI, e.unionS, e.esqSD);
    {
        std::vector<std::string> cab, col;
        cab.push_back("Variante"); cab.push_back("Mecanismo"); cab.push_back("Corridas");
        cab.push_back("Inconsistencias"); cab.push_back("OK");
        for (int i = 0; i < 5; i++) col.push_back(e.negrita);
        filaTexto(e, anchos, cab, col);
    }
    linea(e, anchos, e.unionIzq, e.cruz, e.unionDer);

    for (std::map<char, std::vector<Fila> >::iterator it = porVar.begin(); it != porVar.end(); ++it) {
        const std::vector<Fila>& v = it->second;
        long mn = v[0].inconsistencias, mx = v[0].inconsistencias;
        double okSum = 0;
        for (size_t i = 0; i < v.size(); i++) {
            mn = std::min(mn, v[i].inconsistencias);
            mx = std::max(mx, v[i].inconsistencias);
            okSum += v[i].porcentajeOk();
        }
        const bool limpio = (mx == 0);
        std::string rango = (mn == mx) ? fmtL(mn) : (fmtL(mn) + " - " + fmtL(mx));

        std::vector<std::string> c, col;
        c.push_back(std::string(1, it->first));
        c.push_back(nombreVariante(it->first));
        c.push_back(fmtL(static_cast<long>(v.size())));
        c.push_back(rango);
        c.push_back(fmt(okSum / static_cast<double>(v.size()), 0) + "%");
        col.push_back(e.negrita); col.push_back(""); col.push_back("");
        col.push_back(limpio ? e.verde : e.rojo);
        col.push_back(limpio ? e.verde : e.rojo);
        filaTexto(e, anchos, c, col);
    }
    linea(e, anchos, e.esqII, e.unionI, e.esqID);
    printf("%s  OK = porcentaje de items producidos que se entregaron validos y una sola vez.%s\n",
           e.gris, e.fin);

    // ---------------------------------------------------------------------
    // 2. Variabilidad de la version A
    // ---------------------------------------------------------------------
    if (porVarFija.find('A') != porVarFija.end() && porVarFija['A'].size() > 1) {
        titulo(e, "2. VARIABILIDAD DE LA CONDICION DE CARRERA (version A)");
        printf("%s  Capacidad fija = %d%s\n\n", e.gris, capElegida, e.fin);
        const std::vector<Fila>& v = porVarFija['A'];
        long mx = 0;
        for (size_t i = 0; i < v.size(); i++) mx = std::max(mx, v[i].inconsistencias);
        for (size_t i = 0; i < v.size(); i++) {
            printf("  corrida %-2d ", static_cast<int>(i + 1));
            barra(e, static_cast<double>(v[i].inconsistencias), static_cast<double>(mx), 32, e.rojo);
            printf(" %s%8ld%s\n", e.rojo, v[i].inconsistencias, e.fin);
        }
        printf("\n%s  Cada corrida da un numero distinto: eso es lo que prueba que la carrera%s\n", e.gris, e.fin);
        printf("%s  es genuina. Si estuviera fabricada, las barras serian todas iguales.%s\n", e.gris, e.fin);
    }

    // ---------------------------------------------------------------------
    // 3. Costo por item entregado correctamente
    // ---------------------------------------------------------------------
    titulo(e, "3. COSTO POR ITEM ENTREGADO CORRECTAMENTE");
    printf("%s  Capacidad fija = %d%s\n\n", e.gris, capElegida, e.fin);
    {
        std::map<char, double> med;
        double mx = 0;
        for (std::map<char, std::vector<Fila> >::iterator it = porVarFija.begin(); it != porVarFija.end(); ++it) {
            std::vector<double> xs;
            for (size_t i = 0; i < it->second.size(); i++) xs.push_back(it->second[i].usPorItem());
            med[it->first] = mediana(xs);
            mx = std::max(mx, med[it->first]);
        }
        for (std::map<char, double>::iterator it = med.begin(); it != med.end(); ++it) {
            const char* c = it->first == 'A' ? e.rojo : e.verde;
            printf("  %s%c%s %-26s ", e.negrita, it->first, e.fin, nombreVariante(it->first));
            barra(e, it->second, mx, 30, c);
            printf(" %s%8.2f us%s\n", c, it->second, e.fin);
        }
        printf("\n%s  Medianas. La barra de A esta en rojo porque su velocidad viene de NO%s\n", e.gris, e.fin);
        printf("%s  hacer el trabajo completo: pierde y corrompe items. No es comparable%s\n", e.gris, e.fin);
        printf("%s  como \"mas rapida\", es el costo medido de pasar de incorrecto a correcto.%s\n", e.gris, e.fin);
    }

    // ---------------------------------------------------------------------
    // 4. Barrido de capacidad (solo si hay mas de una)
    // ---------------------------------------------------------------------
    std::map<int, std::map<char, std::vector<double> > > porCap;
    for (size_t i = 0; i < filas.size(); i++)
        porCap[filas[i].capacidad][filas[i].variante].push_back(filas[i].usPorItem());

    if (porCap.size() > 1) {
        titulo(e, "4. EFECTO DE LA CONTENCION: ESPERA ACTIVA (B) VS. BLOQUEO (C)");
        printf("%s  Barra izquierda = B (espera activa)   Barra derecha = C (se duerme)%s\n\n", e.gris, e.fin);
        double mx = 0;
        for (std::map<int, std::map<char, std::vector<double> > >::iterator it = porCap.begin();
             it != porCap.end(); ++it) {
            mx = std::max(mx, mediana(it->second['B']));
            mx = std::max(mx, mediana(it->second['C']));
        }
        for (std::map<int, std::map<char, std::vector<double> > >::iterator it = porCap.begin();
             it != porCap.end(); ++it) {
            const double b = mediana(it->second['B']);
            const double c = mediana(it->second['C']);
            if (b <= 0 && c <= 0) continue;
            printf("  cap %-6d B ", it->first);
            barra(e, b, mx, 24, e.ambar);
            printf(" %7.2f\n", b);
            printf("  %-10s C ", "");
            barra(e, c, mx, 24, e.verde);
            printf(" %7.2f", c);
            if (b > 0 && c > 0) {
                const bool ganaB = c > b;
                printf("   %sC/B = %.2fx  %s%s\n", ganaB ? e.ambar : e.verde, c / b,
                       ganaB ? "gana B" : "gana C", e.fin);
            } else {
                printf("\n");
            }
            printf("\n");
        }
        printf("%s  Con buffer chico casi toda operacion espera, y dormirse cuesta una%s\n", e.gris, e.fin);
        printf("%s  llamada al sistema; girar sale mas barato. Con buffer grande casi nunca%s\n", e.gris, e.fin);
        printf("%s  hay que esperar, y entonces quemar CPU girando es puro desperdicio.%s\n", e.gris, e.fin);
    }

    printf("\n");
    return 0;
}

// ---------------------------------------------------------------------------
// Generador de cadenas de referencia sinteticas
// TIIT2007 Sistemas Operativos - Universidad Invenio
//
// Produce las cadenas largas que exige el enunciado (1000+ referencias) con
// tres patrones de acceso distintos. La lamina 8 de la Unidad IV afirma que
// LRU gana con cargas que tienen localidad y que sin localidad puede rendir
// parecido a FIFO; generar los tres patrones permite CONTRASTAR esa
// afirmacion con datos propios en vez de repetirla.
//
// Reproducibilidad: el generador usa mt19937 con SEMILLA FIJA, de modo que
// las cadenas se pueden regenerar identicas en cualquier maquina. Ademas
// quedan versionadas en data/, asi que no hace falta confiar en eso.
//
// Uso: ./generar <patron> <referencias> <paginas> <semilla> > archivo.txt
//        patron: localidad | secuencial | aleatoria
// ---------------------------------------------------------------------------
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 5) {
        std::cerr << "Uso: " << argv[0]
                  << " <localidad|secuencial|aleatoria> <referencias> <paginas> <semilla>\n";
        return 1;
    }
    const std::string patron = argv[1];
    const int  referencias = std::atoi(argv[2]);
    const int  paginas     = std::atoi(argv[3]);
    const unsigned semilla = static_cast<unsigned>(std::atol(argv[4]));

    if (referencias < 1 || paginas < 1) {
        std::cerr << "Error: referencias y paginas deben ser positivos.\n";
        return 1;
    }
    if (patron != "localidad" && patron != "secuencial" && patron != "aleatoria") {
        std::cerr << "Error: patron no reconocido: " << patron << "\n";
        return 1;
    }

    std::mt19937 rng(semilla);
    std::vector<int> cadena;
    cadena.reserve(static_cast<size_t>(referencias));

    if (patron == "localidad") {
        // Modelo de conjunto de trabajo: el programa trabaja un rato sobre un
        // subconjunto pequeno de paginas (la "ventana") y despues se desplaza.
        // Es el comportamiento tipico de un programa real: bucles, funciones
        // que se llaman entre si, estructuras que se recorren varias veces.
        const int ventana = paginas / 5 > 2 ? paginas / 5 : 3;
        std::uniform_int_distribution<int> dentro(0, ventana - 1);
        std::uniform_int_distribution<int> duracion(20, 60);
        std::uniform_int_distribution<int> salto(1, 3);

        int base = 0;
        while (static_cast<int>(cadena.size()) < referencias) {
            const int cuantas = duracion(rng);
            for (int k = 0; k < cuantas && static_cast<int>(cadena.size()) < referencias; k++)
                cadena.push_back((base + dentro(rng)) % paginas);
            base = (base + salto(rng)) % paginas;
        }
    } else if (patron == "secuencial") {
        // Barrido lineal repetido: se recorren todas las paginas en orden y se
        // vuelve a empezar. Es el peor caso para cualquier algoritmo de
        // reemplazo cuando la ventana no cabe: para cuando se vuelve a la
        // pagina 0, ya fue desalojada.
        for (int i = 0; i < referencias; i++) cadena.push_back(i % paginas);
    } else {
        // Aleatoria uniforme: sin localidad de ninguna clase. Es el escenario
        // donde no deberia haber ventaja clara de LRU sobre FIFO.
        std::uniform_int_distribution<int> uniforme(0, paginas - 1);
        for (int i = 0; i < referencias; i++) cadena.push_back(uniforme(rng));
    }

    printf("# Cadena sintetica generada por herramientas/generar\n");
    printf("# patron=%s referencias=%d paginas=%d semilla=%u\n",
           patron.c_str(), referencias, paginas, semilla);
    printf("# Regenerable con: ./generar %s %d %d %u\n",
           patron.c_str(), referencias, paginas, semilla);
    for (size_t i = 0; i < cadena.size(); i++) {
        printf("%d", cadena[i]);
        printf((i + 1) % 25 == 0 ? "\n" : " ");
    }
    printf("\n");
    return 0;
}

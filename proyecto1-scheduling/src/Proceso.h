#ifndef PROCESO_H
#define PROCESO_H

// Representa un proceso de la carga de trabajo sintetica.
// Campos de entrada (id, llegada, rafaga) se leen del archivo y no se modifican.
// Campos de trabajo/salida (restante, finalizacion, tiempos) se calculan durante la simulacion.
struct Proceso {
    int id;
    int llegada;
    int rafaga;

    int restante;
    int finalizacion;
    int tiempoEspera;
    int tiempoRetorno;

    Proceso(int id_, int llegada_, int rafaga_)
        : id(id_), llegada(llegada_), rafaga(rafaga_),
          restante(rafaga_), finalizacion(0), tiempoEspera(0), tiempoRetorno(0) {}
};

#endif

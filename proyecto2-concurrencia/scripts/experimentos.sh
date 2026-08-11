#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Protocolo experimental (seccion 6 del enunciado).
#
# Variables controladas: mismo TOTAL de items en todas las configuraciones,
# mismos binarios, misma maquina. Lo unico que varia entre configuraciones es
# N y la cantidad de hilos.
#
# Configuraciones: N en {5, 50} x (2P,2C) y (4P,4C)  -> cubre RF-1 y RF-2
# Repeticiones: 5 por version y por configuracion    -> cubre el minimo exigido
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/.."

TOTAL=20000
REPS=5
CSV=evidencia/resultados.csv

mkdir -p evidencia
rm -f "$CSV"

for N in 5 50; do
  for P in 2 4; do
    C=$P
    ITEMS=$(( TOTAL / P ))          # el total se mantiene constante
    for VER in a b; do
      for i in $(seq 1 $REPS); do
        ./version_$VER/version_$VER \
            --capacidad "$N" --productores "$P" --consumidores "$C" \
            --items "$ITEMS" --sin-color --csv "$CSV" \
            > "evidencia/corrida_${VER}_N${N}_${P}P${C}C_${i}.txt"
      done
    done
  done
done

echo "Listo. Resultados en $CSV y salidas por corrida en evidencia/"

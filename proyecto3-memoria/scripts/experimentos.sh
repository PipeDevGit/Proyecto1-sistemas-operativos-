#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Protocolo experimental del Proyecto 3.
#
# Variables controladas: mismo binario, misma maquina, mismas cadenas
# versionadas en data/. Lo unico que varia es la cadena, el numero de marcos
# y el algoritmo.
#
# Cubre los resultados obligatorios del enunciado:
#   - tabla de fallos por algoritmo con al menos 3 tamanos de marco
#   - comparacion FIFO vs LRU frente al tamano de marco
#   - metricas de tiempo sobre cadenas largas (1000+ referencias)
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/.."

CSV=evidencia/resultados.csv
mkdir -p evidencia
rm -f "$CSV"

# Cadenas cortas: para la traza y para la anomalia de Belady.
for M in 3 4 5; do
  ./simulador --cadena data/corta.txt --marcos $M --sin-color --csv "$CSV" > /dev/null
done
for M in 3 4 5; do
  ./simulador --cadena data/belady.txt --marcos $M --sin-color --csv "$CSV" > /dev/null
done

# Cadenas largas: 1200 referencias sobre 20 paginas, tres patrones de acceso.
for C in localidad secuencial aleatoria; do
  for M in 3 4 5 8 12 16; do
    ./simulador --cadena "data/larga_$C.txt" --marcos $M --sin-color --csv "$CSV" > /dev/null
  done
done

echo "Listo. Resultados en $CSV"

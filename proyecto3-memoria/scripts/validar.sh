#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# Validacion contra la literatura.
#
# La cadena 7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1 con 3 marcos es el ejemplo
# canonico de Silberschatz, con valores publicados: FIFO 15, LRU 12, OPT 9.
# Que la implementacion los reproduzca es una verificacion EXTERNA: no depende
# de que el autor considere correcto su propio codigo.
# ---------------------------------------------------------------------------
set -u
cd "$(dirname "$0")/.."

esperado_fifo=15
esperado_lru=12
esperado_opt=9

salida=$(./simulador --cadena data/corta.txt --marcos 3 --sin-color)
obtenido_fifo=$(echo "$salida" | awk '/^  FIFO /{print $4}')
obtenido_lru=$(echo  "$salida" | awk '/^  LRU /{print $4}')
obtenido_opt=$(echo  "$salida" | awk '/^  OPTIMO /{print $4}')

fallos=0
verificar() {
  if [ "$2" = "$3" ]; then
    printf "  OK    %-8s esperado %-3s obtenido %s
" "$1" "$2" "$3"
  else
    printf "  FALLA %-8s esperado %-3s obtenido %s
" "$1" "$2" "$3"
    fallos=$((fallos+1))
  fi
}

echo "Validacion contra los valores publicados (cadena canonica, 3 marcos):"
verificar FIFO   "$esperado_fifo" "$obtenido_fifo"
verificar LRU    "$esperado_lru"  "$obtenido_lru"
verificar OPTIMO "$esperado_opt"  "$obtenido_opt"

if [ "$fallos" -eq 0 ]; then
  echo "  Los tres algoritmos reproducen los valores de la literatura."
  exit 0
else
  echo "  $fallos algoritmo(s) NO coinciden con la literatura."
  exit 1
fi

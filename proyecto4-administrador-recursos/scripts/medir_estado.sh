#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# medir_estado.sh - Fotografia del estado del sistema
# Proyecto 4: Administrador Simplificado de Recursos
# TIIT2007 Sistemas Operativos - Universidad Invenio
#
# QUE HACE
#   Imprime memoria, procesos y uso de CPU del momento. Lo llama
#   experimentos.sh antes, durante y despues de la carga.
#
# POR QUE NO SE CONFIA EN LA PRIMERA LINEA DE vmstat
#   La PRIMERA linea que imprime vmstat NO es el instante actual: es el
#   promedio desde que arranco el sistema. En una maquina que lleva una hora
#   encendida y casi siempre ociosa, esa linea dice "99% ocioso" aunque en ese
#   momento haya ocho hilos quemando CPU. Leerla como si fuera el estado actual
#   fue exactamente el error que casi entra al informe: la evidencia decia 99%
#   ocioso mientras 'ps' mostraba los procesos de carga al 197%.
#
#   Por eso el uso de CPU se calcula aca a mano, con dos lecturas de /proc/stat
#   separadas por un intervalo. Es el mismo mecanismo que usan vmstat, top y
#   mpstat por dentro, sin la trampa de la primera linea.
#
# USO
#   ./scripts/medir_estado.sh "ETIQUETA" [segundos_de_muestreo]
# ---------------------------------------------------------------------------
set -u

ETIQUETA="${1:-estado}"
INTERVALO="${2:-2}"

# --- uso de CPU por diferencia de /proc/stat --------------------------------
# La primera linea de /proc/stat trae los tics acumulados de todos los nucleos
# desde el arranque, repartidos por modo: usuario, nice, sistema, ocioso, ...
# El uso real es la fraccion de tics que NO fueron ociosos durante el intervalo.
leer_tics() {
  awk '/^cpu /{
        ocioso = $5 + $6;                 # idle + iowait
        total  = 0; for (i = 2; i <= NF; i++) total += $i;
        print total, ocioso; exit }' /proc/stat
}

read -r total1 ocioso1 <<< "$(leer_tics)"
sleep "$INTERVALO"
read -r total2 ocioso2 <<< "$(leer_tics)"

dt=$(( total2 - total1 ))
di=$(( ocioso2 - ocioso1 ))
if [ "$dt" -gt 0 ]; then
  USO_CPU=$(( (100 * (dt - di)) / dt ))
else
  USO_CPU="?"
fi

echo "--- $ETIQUETA ---------------------------------------------"
echo
echo "Uso de CPU en los ultimos ${INTERVALO}s: ${USO_CPU}%"
echo "  (calculado por diferencia de /proc/stat; ver el comentario del script"
echo "   sobre por que no se usa la primera linea de vmstat)"
echo
echo "Carga media (1, 5 y 15 minutos): $(cut -d' ' -f1-3 /proc/loadavg)"
echo "  (en esta maquina hay $(nproc) nucleos logicos: 6.0 seria saturacion)"
echo
echo "\$ free -h"
free -h
echo
echo "Procesos totales: $(ps -e --no-headers | wc -l)"
echo
echo "Procesos de carga activos: $(pgrep -c -x carga 2>/dev/null || echo 0)"
echo
echo "\$ ps -eo pid,pcpu,pmem,rss,stat,comm --sort=-pcpu | head -8"
ps -eo pid,pcpu,pmem,rss,stat,comm --sort=-pcpu | head -8
echo

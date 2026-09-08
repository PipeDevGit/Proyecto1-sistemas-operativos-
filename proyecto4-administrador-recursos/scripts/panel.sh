#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# panel.sh - Panel comparativo de recursos en Linux (Laboratorio 6)
# Proyecto 4: Administrador Simplificado de Recursos
# TIIT2007 Sistemas Operativos - Universidad Invenio
# Isaac Felipe Morun Moreira
#
# QUE HACE
#   Recoge las mismas metricas que pide el Laboratorio 6 -CPU, memoria, disco y
#   procesos- usando las herramientas del sistema, y al final las mismas
#   metricas segun la herramienta del proyecto. Lado a lado.
#
# PARA QUE SIRVE ESA COMPARACION
#   La pregunta 3 de analisis del Laboratorio 6 es literalmente que insumo
#   aporta el laboratorio al componente de monitoreo del Proyecto 4. La
#   respuesta util no es una lista de comandos: es poder poner los numeros de
#   'free' y 'ps' al lado de los de la herramienta propia y ver si coinciden.
#   Si no coincidieran, la herramienta estaria mal.
#
#   Su equivalente para Windows es scripts/panel.ps1, con los mismos apartados
#   en el mismo orden para que las dos salidas se puedan comparar directamente.
#
# USO
#   ./scripts/panel.sh > results/panel_linux.txt
# ---------------------------------------------------------------------------
set -u
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$RAIZ"

titulo() { echo; echo "=== $* ==================================================="; }

echo "==========================================================="
echo " PANEL DE RECURSOS - Linux            $(date '+%Y-%m-%d %H:%M:%S')"
echo "==========================================================="
uname -a
echo "Nucleos logicos: $(nproc 2>/dev/null || echo '?')"

titulo "1. CPU"
echo "\$ uptime   (carga media de 1, 5 y 15 minutos)"
uptime
echo
echo "Uso de CPU calculado desde /proc/stat:"
bash "$RAIZ/scripts/medir_estado.sh" "instantanea" 2 2>/dev/null | grep -E "Uso de CPU|Carga media"
echo
echo "\$ mpstat 2 2"
mpstat 2 2 2>/dev/null | tail -4 || echo "  (mpstat no instalado: sudo apt install sysstat)"

titulo "2. MEMORIA"
echo "\$ free -h"
free -h
echo
echo "\$ grep -E '^(MemTotal|MemAvailable|SwapTotal):' /proc/meminfo"
grep -E "^(MemTotal|MemAvailable|SwapTotal):" /proc/meminfo

titulo "3. DISCO"
echo "\$ df -h /"
df -h / 2>/dev/null
echo
echo "\$ head -3 /proc/diskstats"
head -3 /proc/diskstats 2>/dev/null || echo "  (sin /proc/diskstats)"

titulo "4. PROCESOS"
echo "\$ ps -eo pid,pcpu,pmem,rss,stat,comm --sort=-rss | head -6"
ps -eo pid,pcpu,pmem,rss,stat,comm --sort=-rss | head -6
echo
echo "Procesos totales: $(ps -e --no-headers | wc -l)"

titulo "5. INTERRUPCIONES  (Unidad VI)"
echo "\$ head -5 /proc/interrupts"
head -5 /proc/interrupts 2>/dev/null || echo "  (sin /proc/interrupts)"
echo
echo "Cada fila es una linea de peticion de interrupcion (IRQ) y cuantas veces"
echo "la atendio cada nucleo. Es la evidencia mas directa de que la E/S del"
echo "sistema funciona por interrupciones y no por consulta en bucle."

titulo "6. LO MISMO, SEGUN LA HERRAMIENTA DEL PROYECTO"
if [ -x ./administrador ]; then
  printf "3\n0\n" | ./administrador --sin-color --ascii 2>/dev/null \
    | sed -n "/MEMORIA DEL SISTEMA/,/Fuente:/p"
  printf "2\n0\n0\n" | ./administrador --sin-color --ascii --procesos 5 2>/dev/null \
    | sed -n "/PID *Nombre/,/Fuente:/p"
  printf "6\n0\n" | ./administrador --sin-color --ascii 2>/dev/null \
    | sed -n "/Memoria residente/,/Fuente:/p"
else
  echo "  (falta ./administrador: corre 'make' primero)"
fi

echo
echo "==========================================================="
echo " Los numeros de los apartados 2 y 4 deben coincidir con los"
echo " del apartado 6. Si no coincidieran, la herramienta estaria mal."
echo "==========================================================="

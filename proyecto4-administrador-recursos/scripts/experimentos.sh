#!/usr/bin/env bash
# ---------------------------------------------------------------------------
# experimentos.sh - Protocolo experimental completo
# Proyecto 4: Administrador Simplificado de Recursos
# TIIT2007 Sistemas Operativos - Universidad Invenio
# Isaac Felipe Morun Moreira
#
# QUE HACE
#   Corre las dos condiciones que pide el enunciado -sistema en reposo y
#   sistema bajo carga- y deja toda la evidencia en results/.
#
# POR QUE UN SCRIPT Y NO COMANDOS SUELTOS
#   El enunciado dice que los resultados deben ser reproducibles y que el
#   docente puede pedir volver a correrlos en vivo. Un script versionado es la
#   unica forma de garantizar que la proxima corrida haga exactamente lo mismo
#   que la que produjo los numeros del documento.
#
# TRES COSAS QUE ESTE SCRIPT COMPRUEBA ANTES DE DAR UN NUMERO POR BUENO
#   Las tres salieron de errores reales cometidos midiendo, no de precaucion
#   teorica:
#
#   1. QUE EL SISTEMA ESTE DE VERDAD EN REPOSO ANTES DE EMPEZAR.
#      Una corrida anterior tomo el baseline con carga media 11.3, porque la
#      maquina todavia se estaba recuperando de la corrida previa. El resultado
#      fue absurdo: el escenario "en reposo" salio MAS LENTO que el escenario
#      "bajo carga". Ahora se espera a que la carga media baje del umbral y, si
#      no baja, el script se niega a medir.
#
#   2. QUE LA CARGA ESTE PRESENTE MIENTRAS SE MIDE, NO ANTES NI DESPUES.
#      La carga interna vive dentro del proceso del benchmark, asi que medir el
#      estado del sistema antes y despues de ejecutarlo no la ve. Ahora un
#      muestreador corre EN PARALELO al benchmark y registra el estado real
#      durante la medicion.
#
#   3. QUE LA CARGA NO SEA TANTA QUE MIDA OTRA COSA.
#      Con 2 GB y 10 hilos, una llamada a 'ps' que en reposo tarda 18 ms tardo
#      226 SEGUNDOS. Eso no mide la herramienta, mide la inanicion. Los
#      parametros de abajo dejan la mitad de la CPU libre a proposito.
#
# COMO SE GENERA LA CARGA
#   Dos partes. La INTERNA (--carga-mb y --carga-hilos del benchmark) la genera
#   el propio proceso que mide, asi que esta presente en todas sus mediciones
#   por construccion. Las instancias EXTERNAS suben el numero de procesos del
#   sistema, que es la otra mitad de lo que pide el enunciado.
#
#   La primera version dejaba todo en manos de las externas, y morian a los
#   pocos segundos: la conexion SSH se cae bajo la carga y arrastra la sesion
#   entera. Ni nohup ni setsid lo evitan. Por eso este script debe lanzarse
#   DESACOPLADO de la sesion:
#
#     setsid nohup ./scripts/experimentos.sh 5 > results/corrida.log 2>&1 </dev/null &
#
# USO
#   ./scripts/experimentos.sh [repeticiones]
# ---------------------------------------------------------------------------
set -u

REPETICIONES="${1:-5}"
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
cd "$RAIZ"

BENCH="./herramientas/benchmark"
CARGA="./herramientas/carga"
MEDIR="./scripts/medir_estado.sh"
CSV="results/benchmark.csv"
COMPARATIVA="results/comparativa_carga.txt"
TESTIGO="results/.muestreando"

# --- Parametros de la carga ------------------------------------------------
# Interna: la que sostiene el experimento. 3 hilos sobre 6 nucleos deja la
# mitad de la CPU para el resto del sistema (ver punto 3 de la cabecera).
CARGA_MB=1000
CARGA_HILOS=3

# Externas: aportan procesos. Si mueren, el escenario sigue siendo valido.
INSTANCIAS=2
MB_POR_INSTANCIA=200
HILOS_POR_INSTANCIA=1
SEGUNDOS_CARGA=900

# --- Umbral de reposo -------------------------------------------------------
# Carga media de 1 minuto por debajo de la cual se considera reposo. Con 6
# nucleos, 1.5 es holgado: menos de un cuarto de la capacidad ocupada.
UMBRAL_REPOSO=1.5
ESPERA_MAXIMA=300

for prog in "$BENCH" "$CARGA"; do
  if [ ! -x "$prog" ]; then
    echo "Error: falta $prog. Corre 'make' primero." >&2
    exit 1
  fi
done
chmod +x "$MEDIR" 2>/dev/null

mkdir -p results
rm -f "$CSV" "$TESTIGO"

carga_media() { cut -d' ' -f1 /proc/loadavg; }

contar_cargas() {
  local n
  n=$(pgrep -c -x carga 2>/dev/null)
  [ -z "$n" ] && n=0
  echo "$n"
}

# Muestrea el estado del sistema cada 5 segundos hasta que desaparece el
# archivo testigo. Se lanza EN PARALELO al benchmark para capturar la carga
# mientras esta presente, que es el punto 2 de la cabecera.
muestrear() {
  local salida="$1" etiqueta="$2"
  {
    echo "--- Muestreo DURANTE el benchmark ($etiqueta) ----------------"
    echo "  Cada linea es una foto tomada mientras la medicion estaba corriendo."
    printf "  %-9s %8s %12s %10s %8s\n" "instante" "carga1m" "mem_usada" "procesos" "cargas"
  } >> "$salida"
  local t=0
  while [ -f "$TESTIGO" ]; do
    printf "  %+8ss %8s %9s MB %10s %8s\n" \
      "$t" "$(carga_media)" \
      "$(free -m | awk '/^Mem/{print $3}')" \
      "$(ps -e --no-headers | wc -l)" \
      "$(contar_cargas)" >> "$salida"
    sleep 5
    t=$((t + 5))
  done
  echo >> "$salida"
}

echo "==========================================================="
echo " PROTOCOLO EXPERIMENTAL - Proyecto 4"
echo " Repeticiones por tamano: $REPETICIONES (mas una de cache fria)"
echo "==========================================================="

# ---------------------------------------------------------------------------
# Precondicion: el sistema tiene que estar en reposo de verdad
# ---------------------------------------------------------------------------
echo
echo ">>> Esperando reposo (carga media de 1 minuto < $UMBRAL_REPOSO)"
esperado=0
while [ "$esperado" -lt "$ESPERA_MAXIMA" ]; do
  actual=$(carga_media)
  if awk -v a="$actual" -v u="$UMBRAL_REPOSO" 'BEGIN{exit !(a < u)}'; then
    echo "    carga media $actual: listo tras ${esperado}s"
    break
  fi
  echo "    carga media $actual, esperando... (${esperado}s)"
  sleep 15
  esperado=$((esperado + 15))
done

actual=$(carga_media)
if ! awk -v a="$actual" -v u="$UMBRAL_REPOSO" 'BEGIN{exit !(a < u)}'; then
  echo "Error: el sistema no llego al reposo (carga media $actual) tras ${ESPERA_MAXIMA}s." >&2
  echo "Medir ahora daria un baseline contaminado, que es exactamente el error" >&2
  echo "que hizo que una corrida anterior diera el reposo mas lento que la carga." >&2
  exit 1
fi

# ---------------------------------------------------------------------------
# Cabecera de la evidencia
# ---------------------------------------------------------------------------
{
  echo "==========================================================="
  echo " COMPARATIVA DE CARGA - Proyecto 4"
  echo " Generado por scripts/experimentos.sh el $(date '+%Y-%m-%d %H:%M:%S')"
  echo "==========================================================="
  echo
  echo "--- Entorno ---------------------------------------------"
  uname -a
  echo "Compilador: $(g++ --version | head -1)"
  echo "Nucleos logicos: $(nproc 2>/dev/null || echo '?')"
  echo
  echo "--- Composicion de la carga ------------------------------"
  echo "Interna (dentro del proceso que mide): ${CARGA_MB} MB tocados, ${CARGA_HILOS} hilos"
  echo "Externas (aportan procesos):           ${INSTANCIAS} x ${MB_POR_INSTANCIA} MB, ${HILOS_POR_INSTANCIA} hilo c/u"
  echo "Total: $((CARGA_MB + INSTANCIAS * MB_POR_INSTANCIA)) MB y $((CARGA_HILOS + INSTANCIAS * HILOS_POR_INSTANCIA)) hilos sobre $(nproc) nucleos"
  echo
  echo "NOTA SOBRE LA MEDICION DE CPU"
  echo "  Se calcula por diferencia de /proc/stat y no leyendo la primera linea"
  echo "  de vmstat: esa primera linea es el promedio DESDE EL ARRANQUE del"
  echo "  sistema, no el instante actual."
  echo
  echo "PRECONDICION VERIFICADA"
  echo "  Se espero a que la carga media de 1 minuto bajara de $UMBRAL_REPOSO antes de"
  echo "  medir el baseline. Valor al empezar: $actual"
  echo
} > "$COMPARATIVA"

bash "$MEDIR" "Estado del sistema EN REPOSO" 3 >> "$COMPARATIVA"

# ---------------------------------------------------------------------------
# ESCENARIO 1: sistema en reposo
# ---------------------------------------------------------------------------
echo
echo ">>> Escenario 1 de 2: sistema en reposo"
touch "$TESTIGO"
muestrear "$COMPARATIVA" "sin_carga" &
MUESTREADOR=$!

"$BENCH" --repeticiones "$REPETICIONES" --escenario sin_carga --csv "$CSV"

rm -f "$TESTIGO"
wait "$MUESTREADOR" 2>/dev/null

# ---------------------------------------------------------------------------
# ESCENARIO 2: sistema bajo carga
# ---------------------------------------------------------------------------
echo
echo ">>> Escenario 2 de 2: sistema bajo carga"
echo "    lanzando $INSTANCIAS instancias externas"

for i in $(seq 1 "$INSTANCIAS"); do
  setsid "$CARGA" --mb "$MB_POR_INSTANCIA" --hilos "$HILOS_POR_INSTANCIA" \
                  --segundos "$SEGUNDOS_CARGA" --silencioso >/dev/null 2>&1 &
done
sleep 8

VIVAS_ANTES=$(contar_cargas)
echo "    instancias externas activas: $VIVAS_ANTES de $INSTANCIAS"
echo "    carga interna del benchmark: ${CARGA_MB} MB y ${CARGA_HILOS} hilos"

touch "$TESTIGO"
muestrear "$COMPARATIVA" "con_carga" &
MUESTREADOR=$!

"$BENCH" --repeticiones "$REPETICIONES" --escenario con_carga --csv "$CSV" \
         --carga-mb "$CARGA_MB" --carga-hilos "$CARGA_HILOS"

rm -f "$TESTIGO"
wait "$MUESTREADOR" 2>/dev/null

VIVAS_DESPUES=$(contar_cargas)
echo "    instancias externas al terminar: $VIVAS_DESPUES de $INSTANCIAS"

{
  echo "--- Validez de la corrida --------------------------------"
  echo "Carga interna: presente en TODAS las mediciones de con_carga por"
  echo "  construccion, porque vive dentro del proceso que mide. Los dos"
  echo "  muestreos de mas arriba permiten comprobarlo: la memoria usada y la"
  echo "  carga media son visiblemente mayores durante con_carga que durante"
  echo "  sin_carga, y vuelven a bajar cuando el benchmark termina."
  echo
  echo "Instancias externas: $VIVAS_ANTES vivas al empezar, $VIVAS_DESPUES al terminar,"
  echo "  de $INSTANCIAS lanzadas. Aportan procesos; el experimento no depende de ellas."
  echo
} >> "$COMPARATIVA"

echo
echo "    apagando la carga..."
pkill -x carga 2>/dev/null
for _ in $(seq 1 30); do
  [ "$(contar_cargas)" -eq 0 ] && break
  sleep 1
done

sleep 5
bash "$MEDIR" "Estado del sistema DESPUES DE LA CARGA" 3 >> "$COMPARATIVA"

# ---------------------------------------------------------------------------
# Reporte
# ---------------------------------------------------------------------------
echo
echo "==========================================================="
echo " Mediciones en: $CSV"
echo " Evidencia de carga en: $COMPARATIVA"
echo "==========================================================="

if [ -x ./herramientas/reporte ]; then
  ./herramientas/reporte "$CSV" --ascii --sin-color > results/reporte.txt
  echo " Reporte en: results/reporte.txt"
fi

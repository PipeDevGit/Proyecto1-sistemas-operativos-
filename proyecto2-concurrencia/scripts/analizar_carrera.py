#!/usr/bin/env python3
"""
Localiza en el log de eventos de la version A un caso concreto de condicion de
carrera y lo imprime con su contexto, para la evidencia que exige la seccion 7
del enunciado.

Uso: python3 scripts/analizar_carrera.py evidencia/log_version_a.txt
"""
import collections
import sys


def cargar(ruta):
    filas = []
    for linea in open(ruta, encoding="utf-8"):
        if linea.startswith("#") or not linea.strip():
            continue
        # Columnas: ms | hilo | tipo | id | valor | pos | antes->despues | integro
        c = linea.split()
        antes, despues = c[6].split("->")
        filas.append({
            "texto": linea.rstrip(),
            "hilo": c[1],
            "tipo": c[2],
            "id": c[3],
            "valor": int(c[4]),
            "pos": int(c[5]),
            "antes": int(antes),
            "despues": int(despues),
            "integro": c[7],
        })
    return filas


def main():
    ruta = sys.argv[1] if len(sys.argv) > 1 else "evidencia/log_version_a.txt"
    filas = cargar(ruta)

    consumos = collections.defaultdict(list)
    for i, f in enumerate(filas):
        if f["tipo"] == "CONS":
            consumos[f["id"]].append(i)
    duplicados = {k: v for k, v in consumos.items() if len(v) > 1}

    # Operaciones donde la ocupacion no vario: prueba de un incremento o un
    # decremento perdido en el contador compartido.
    perdidas = [i for i, f in enumerate(filas) if f["antes"] == f["despues"]]

    # Posiciones escritas dos veces seguidas sin consumo intermedio.
    print("=" * 72)
    print("EVIDENCIA DE CONDICION DE CARRERA - VERSION A")
    print("=" * 72)
    print()
    print("Eventos registrados                     : %d" % len(filas))
    print("Items consumidos mas de una vez         : %d" % len(duplicados))
    print("Operaciones sin cambio en la ocupacion  : %d" % len(perdidas))
    print("  (un incremento o decremento se perdio: dos hilos leyeron el mismo")
    print("   valor del contador y escribieron el mismo resultado)")
    print()

    if perdidas:
        i = perdidas[0]
        f = filas[i]
        print("-" * 72)
        print("CASO 1 - ACTUALIZACION PERDIDA EN EL CONTADOR")
        print("-" * 72)
        print()
        for k in range(max(0, i - 2), min(len(filas), i + 3)):
            print(("  >>> " if k == i else "      ") + filas[k]["texto"])
        print()
        print("  El hilo %s ejecuto una operacion de %s y la ocupacion quedo en" %
              (f["hilo"], "produccion" if f["tipo"] == "PROD" else "consumo"))
        print("  %d antes y %d despues: no cambio. El contador es una operacion de" %
              (f["antes"], f["despues"]))
        print("  lectura-modificacion-escritura en tres pasos; otro hilo leyo el")
        print("  mismo valor, opero sobre el, y una de las dos escrituras piso a la")
        print("  otra. El efecto de esta operacion se perdio por completo.")
        print()

    if duplicados:
        k = sorted(duplicados, key=lambda x: duplicados[x][0])[0]
        a, b = duplicados[k][0], duplicados[k][1]
        print("-" * 72)
        print("CASO 2 - EL MISMO ITEM CONSUMIDO DOS VECES")
        print("-" * 72)
        print()
        for i in range(max(0, a - 1), min(len(filas), b + 2)):
            print(("  >>> " if i in (a, b) else "      ") + filas[i]["texto"])
        print()
        print("  El item id=%s se entrego a un consumidor dos veces, ambas desde" % k)
        print("  la posicion %d del buffer." % filas[a]["pos"])
        print()
        print("  Cadena causal: los decrementos perdidos del caso anterior dejan el")
        print("  contador de ocupacion mas alto de lo que corresponde. Los")
        print("  consumidores creen entonces que hay datos disponibles cuando el")
        print("  buffer ya se vacio, avanzan el indice de lectura dando la vuelta")
        print("  al anillo, y vuelven a leer una posicion que ningun productor ha")
        print("  sobrescrito todavia. El item viejo se entrega por segunda vez.")
        print()
        print("  Por eso la suma de los valores consumidos supera a la de los")
        print("  producidos: no es que se hayan inventado items, es que algunos se")
        print("  contaron dos veces.")
        print()

    print("-" * 72)
    print("POR QUE ESTO NO PUEDE OCURRIR EN LA VERSION B")
    print("-" * 72)
    print()
    print("  En la version A la comprobacion de que hay espacio (o datos) y la")
    print("  escritura correspondiente son dos operaciones separadas: entre una y")
    print("  otra, otro hilo puede ejecutarse. En la version B ambas ocurren")
    print("  dentro del mismo bloqueo del mutex, de modo que ningun hilo puede")
    print("  observar el estado intermedio ni actuar sobre el.")
    print()


if __name__ == "__main__":
    main()

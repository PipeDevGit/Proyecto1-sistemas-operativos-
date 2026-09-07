#!/usr/bin/env python3
# ---------------------------------------------------------------------------
# ansi_a_html.py - Convierte la salida real de la herramienta a HTML
# Proyecto 4: Administrador Simplificado de Recursos
# TIIT2007 Sistemas Operativos - Universidad Invenio
#
# QUE RESUELVE
#   La bitacora del curso tiene que mostrar como se ve la herramienta de
#   verdad, con sus colores. Una captura PNG pesa cientos de kilobytes, no se
#   puede buscar por texto y se desactualiza en silencio cuando el programa
#   cambia. Este script toma la salida ANSI literal del programa y la vuelve
#   HTML equivalente: pesa una decima parte, es texto seleccionable y buscable,
#   y se regenera con un comando cuando la salida cambia.
#
# USO
#   ./administrador | python3 herramientas/ansi_a_html.py > bloque.html
#
# ALCANCE
#   Solo entiende los codigos SGR que emite este proyecto (Consola.h): 0, 1,
#   31, 32, 33, 36 y 90. No pretende ser un emulador de terminal completo; si
#   se agrega un color nuevo a Consola.h hay que agregarlo tambien aqui.
# ---------------------------------------------------------------------------
import html
import re
import sys

# Paleta pensada para leerse sobre el fondo oscuro que fija el <pre>, de modo
# que el bloque se ve igual en el tema claro y en el oscuro de la bitacora.
COLORES = {
    "31": "#ff6b6b",  # rojo   - errores
    "32": "#6ec46e",  # verde  - operaciones correctas
    "33": "#e8b84b",  # ambar  - avisos
    "36": "#5bc8d4",  # cian   - opciones del menu
    "90": "#7a8290",  # gris   - texto secundario
}

ESCAPE = re.compile(r"\x1b\[([0-9;]*)m")


def convertir(texto):
    salida = []
    abiertos = 0          # cuantos <span> quedan sin cerrar
    pos = 0

    for m in ESCAPE.finditer(texto):
        salida.append(html.escape(texto[pos:m.start()]))
        pos = m.end()

        for codigo in (m.group(1) or "0").split(";"):
            if codigo in ("", "0"):
                # Reset: cierra todos los span abiertos de una vez. Sin este
                # conteo, un reset dejaria etiquetas colgando y el resto de la
                # pagina heredaria el color del ultimo mensaje.
                salida.append("</span>" * abiertos)
                abiertos = 0
            elif codigo == "1":
                salida.append('<span style="font-weight:600">')
                abiertos += 1
            elif codigo in COLORES:
                salida.append('<span style="color:%s">' % COLORES[codigo])
                abiertos += 1

    salida.append(html.escape(texto[pos:]))
    salida.append("</span>" * abiertos)
    return "".join(salida)


def main():
    texto = sys.stdin.read()
    cuerpo = convertir(texto).strip("\n")
    estilo = ("background:#1a1d21;color:#d4d7dd;padding:14px 16px;"
              "border-radius:8px;overflow-x:auto;font-size:12.5px;"
              "line-height:1.5;margin:0")
    sys.stdout.write('<pre style="%s">%s</pre>\n' % (estilo, cuerpo))


if __name__ == "__main__":
    main()

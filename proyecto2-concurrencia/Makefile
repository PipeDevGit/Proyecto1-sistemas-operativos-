# ---------------------------------------------------------------------------
# Makefile raiz - Proyecto 2, Simulador de Concurrencia
# TIIT2007 Sistemas Operativos - Universidad Invenio
#
# RNF-6: el proyecto compila con UN SOLO comando.
#
#   make               compila las dos versiones y las herramientas
#   make experimentos  ejecuta el protocolo completo y muestra el reporte
#   make evidencia     regenera el log de eventos y la carrera explicada
#   make tsan          binarios con ThreadSanitizer y su contraste A vs B
#   make clean
#
# Banderas usadas en todos los binarios: -std=c++17 -Wall -Wextra -pthread -O2
# Cada version tiene ademas su propio Makefile y compila por separado.
# ---------------------------------------------------------------------------

all: versiones herramientas extras

versiones:
	$(MAKE) -C version_a
	$(MAKE) -C version_b

herramientas:
	$(MAKE) -C herramientas

# Los extras NO son entregables exigidos; sostienen el analisis de RNF-5.
extras:
	$(MAKE) -C extras

experimentos: versiones herramientas
	./scripts/experimentos.sh
	./herramientas/reporte evidencia/resultados.csv

evidencia: versiones
	@mkdir -p evidencia
	# El guion inicial es necesario: la version A devuelve 2 cuando viola el
	# invariante, que es justamente lo que se espera de ella aqui.
	-./version_a/version_a --capacidad 5 --productores 2 --consumidores 2 \
	    --items 25 --sin-color --log evidencia/log_version_a.txt \
	    > evidencia/salida_carrera.txt
	python3 scripts/analizar_carrera.py evidencia/log_version_a.txt \
	    > evidencia/carrera_explicada.txt
	@echo "Evidencia regenerada en evidencia/"

tsan:
	$(MAKE) -C version_a tsan
	$(MAKE) -C version_b tsan
	@mkdir -p evidencia
	-./version_a/version_a_tsan --capacidad 10 --productores 2 --consumidores 2 \
	    --items 500 --sin-color > evidencia/tsan_version_a.log 2>&1
	-./version_b/version_b_tsan --capacidad 10 --productores 2 --consumidores 2 \
	    --items 500 --sin-color > evidencia/tsan_version_b.log 2>&1
	@echo "carreras detectadas en la version A: $$(grep -c 'WARNING: ThreadSanitizer' evidencia/tsan_version_a.log)"
	@echo "carreras detectadas en la version B: $$(grep -c 'WARNING: ThreadSanitizer' evidencia/tsan_version_b.log)"

clean:
	$(MAKE) -C version_a clean
	$(MAKE) -C version_b clean
	$(MAKE) -C herramientas clean
	$(MAKE) -C extras clean

.PHONY: all versiones herramientas extras experimentos evidencia tsan clean

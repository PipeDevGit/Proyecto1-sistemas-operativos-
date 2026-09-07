// ---------------------------------------------------------------------------
// SistemaInfo.cpp - EL UNICO ARCHIVO DEL PROYECTO CON #ifdef
// Proyecto 4: Administrador Simplificado de Recursos
// TIIT2007 Sistemas Operativos - Universidad Invenio
// Isaac Felipe Morun Moreira
//
// QUE RESUELVE
//   C++ no tiene API estandar para consultar los procesos ni la memoria del
//   sistema, asi que esa parte no puede escribirse de forma portable. Todo lo
//   que depende del sistema operativo vive aca dentro y se expone al resto del
//   programa a traves de las interfaces de MonitorProcesos.h y
//   MonitorMemoria.h, que no contienen un solo condicional de plataforma.
//
// POR QUE UN SOLO ARCHIVO Y NO #ifdef REPARTIDOS
//   1. Es lo que la Rubrica 1 llama "codigo modular", y es facil de defender:
//      se puede senalar un archivo y decir "la portabilidad esta toda aqui".
//   2. <windows.h> define macros muy invasivas (entre ellas min y max) que
//      rompen codigo de la biblioteca estandar. Manteniendolo en una unica
//      unidad de traduccion, esa contaminacion no sale de este archivo.
//   3. Anadir una tercera plataforma seria tocar solo este archivo.
//
// NOTA SOBRE habilitarAnsi()
//   Encender el modo VT de la consola es un asunto de presentacion, no de
//   "informacion del sistema", asi que conceptualmente pertenece a Consola.h.
//   Vive aqui igual porque es codigo especifico de Windows, y la regla de que
//   ningun otro archivo tenga #ifdef pesa mas que la pureza tematica.
// ---------------------------------------------------------------------------
#include "Consola.h"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #define NOMINMAX
  #include <windows.h>
#endif

namespace consola {

bool habilitarAnsi() {
#ifdef _WIN32
    // La consola de Windows interpreta secuencias ANSI desde Windows 10, pero
    // hay que pedirlo: por compatibilidad con programas antiguos viene apagado.
    // Sin esto, los colores salen como texto crudo del tipo <-[31m.
    HANDLE salida = GetStdHandle(STD_OUTPUT_HANDLE);
    if (salida == INVALID_HANDLE_VALUE) return false;

    DWORD modo = 0;
    if (!GetConsoleMode(salida, &modo)) return false;   // redirigido a archivo o tuberia

    modo |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(salida, modo) != 0;
#else
    // Linux y macOS: cualquier terminal moderna interpreta ANSI sin pedir nada.
    return true;
#endif
}

} // namespace consola

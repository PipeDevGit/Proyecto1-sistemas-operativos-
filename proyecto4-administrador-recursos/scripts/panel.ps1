# ---------------------------------------------------------------------------
# panel.ps1 - Panel comparativo de recursos en Windows (Laboratorio 6)
# Proyecto 4: Administrador Simplificado de Recursos
# TIIT2007 Sistemas Operativos - Universidad Invenio
# Isaac Felipe Morun Moreira
#
# QUE HACE
#   El equivalente de scripts/panel.sh para Windows, con los mismos apartados
#   en el mismo orden para que las dos salidas se puedan comparar linea por
#   linea. Es lo que necesita la tabla comparativa del Laboratorio 6.
#
# POR QUE LOS COMANDOS SON TAN DISTINTOS DE LOS DE LINUX
#   Porque los dos sistemas exponen la informacion de formas incompatibles.
#   Linux la publica como archivos de texto en /proc, que se leen con cualquier
#   herramienta; Windows la publica a traves de una infraestructura de
#   contadores de rendimiento a la que se llega con Get-Counter, o por llamadas
#   a la API. No hay /proc/meminfo que leer.
#
#   Esa diferencia es justamente la respuesta a la pregunta 2 de analisis del
#   Laboratorio 6, y es la razon de que SistemaInfo.cpp tenga dos ramas.
#
# USO
#   powershell -ExecutionPolicy Bypass -File scripts\panel.ps1 > results\panel_windows.txt
# ---------------------------------------------------------------------------

function Titulo($t) {
    Write-Output ""
    Write-Output "=== $t ==================================================="
}

Write-Output "==========================================================="
Write-Output " PANEL DE RECURSOS - Windows          $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
Write-Output "==========================================================="
$info = Get-ComputerInfo | Select-Object OsName, OsVersion, CsProcessors, CsTotalPhysicalMemory
$info | Format-List
Write-Output "Nucleos logicos: $env:NUMBER_OF_PROCESSORS"

Titulo "1. CPU"
Write-Output "Get-Counter '\Processor(_Total)\% Processor Time' -MaxSamples 2"
Write-Output "  OJO: la primera muestra suele venir en cero, por eso se piden dos."
try {
    (Get-Counter '\Processor(_Total)\% Processor Time' -MaxSamples 2 -SampleInterval 1 -ErrorAction Stop).CounterSamples |
        ForEach-Object { "  uso de CPU: {0:N1} %" -f $_.CookedValue }
} catch {
    Write-Output "  (no se pudieron leer los contadores: $($_.Exception.Message))"
}

Titulo "2. MEMORIA"
$os = Get-CimInstance Win32_OperatingSystem
$totalMb = [math]::Round($os.TotalVisibleMemorySize / 1KB, 0)
$libreMb = [math]::Round($os.FreePhysicalMemory / 1KB, 0)
Write-Output ("  Total:      {0,8} MB" -f $totalMb)
Write-Output ("  Disponible: {0,8} MB" -f $libreMb)
Write-Output ("  Usada:      {0,8} MB" -f ($totalMb - $libreMb))
Write-Output ""
Write-Output "  Equivale a MemTotal y MemAvailable de /proc/meminfo en Linux."

Titulo "3. DISCO"
Get-PSDrive -PSProvider FileSystem |
    Where-Object { $null -ne $_.Used } |
    Select-Object Name,
        @{n = 'UsadoGB'; e = { [math]::Round($_.Used / 1GB, 1) } },
        @{n = 'LibreGB'; e = { [math]::Round($_.Free / 1GB, 1) } } |
    Format-Table -AutoSize

Titulo "4. PROCESOS"
Write-Output "Get-Process | Sort-Object WS -Descending | Select-Object -First 5"
Get-Process |
    Sort-Object WS -Descending |
    Select-Object -First 5 Id, ProcessName,
        @{n = 'MemoriaMB'; e = { [math]::Round($_.WS / 1MB, 1) } },
        @{n = 'CPU_s'; e = { if ($null -eq $_.CPU) { 'n/d' } else { [math]::Round($_.CPU, 2) } } } |
    Format-Table -AutoSize
Write-Output "Procesos totales: $((Get-Process).Count)"

Titulo "5. E/S Y CONTROLADORES  (Unidad VI)"
Write-Output "Windows no expone un equivalente de /proc/interrupts al usuario."
Write-Output "Lo mas cercano son los contadores de disco y la lista de drivers:"
try {
    (Get-Counter '\PhysicalDisk(_Total)\Disk Transfers/sec' -MaxSamples 2 -SampleInterval 1 -ErrorAction Stop).CounterSamples |
        ForEach-Object { "  transferencias de disco por segundo: {0:N1}" -f $_.CookedValue }
} catch {
    Write-Output "  (contador de disco no disponible)"
}
Write-Output ""
Write-Output "Primeros 5 controladores cargados:"
try { driverquery | Select-Object -First 6 } catch { Write-Output "  (driverquery no disponible)" }

Titulo "6. LO MISMO, SEGUN LA HERRAMIENTA DEL PROYECTO"
if (Test-Path .\administrador.exe) {
    "3`n0`n"     | .\administrador.exe --sin-color --ascii 2>$null |
        Select-String -Pattern 'Total:|Usada:|Disponible:|Fuente:'
    "2`n0`n0`n"  | .\administrador.exe --sin-color --ascii --procesos 5 2>$null |
        Select-String -Pattern '^\s+\d+\s|Fuente:'
    "6`n0`n"     | .\administrador.exe --sin-color --ascii 2>$null |
        Select-String -Pattern 'Memoria residente|Memoria virtual|Tiempo de|Fuente:'
} else {
    Write-Output "  (falta administrador.exe: compilar primero con make)"
}

Write-Output ""
Write-Output "==========================================================="
Write-Output " Los numeros de los apartados 2 y 4 deben coincidir con los"
Write-Output " del apartado 6. Si no coincidieran, la herramienta estaria mal."
Write-Output "==========================================================="

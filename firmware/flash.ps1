# Flasht die neue KaRadio32-Firmware (mit Sendertasten + Poti) auf den WROVER.
# Aufruf im Ordner mit den .bin-Dateien (GitHub-Artefakt "karadio32-firmware"):
#   powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COM4
# Sender, WLAN-Daten und Einstellungen bleiben erhalten.
param(
    [Parameter(Mandatory = $true)][string]$Port,
    [switch]$OhneKonfig   # nur die Firmware, Hardware-Konfiguration nicht neu schreiben
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$esptool = Get-ChildItem "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py" -Recurse -Filter esptool.exe |
    Sort-Object FullName -Descending | Select-Object -First 1
if (-not $esptool) { throw "esptool.exe nicht gefunden (kommt mit dem ESP32-Core der Arduino IDE)." }

# Sicherheitsabfrage: ist am Port wirklich der WROVER (CH343, Seriennummer 5C92044868)?
$dev = Get-PnpDevice -PresentOnly -Class Ports | Where-Object { $_.FriendlyName -match "\($Port\)" }
if (-not $dev) { throw "$Port nicht gefunden." }
Write-Host "Geraet an ${Port}: $($dev.FriendlyName)  [$($dev.InstanceId)]"
if ($dev.InstanceId -match '5CE5194172') { throw "$Port ist der ESP32-S3, nicht der WROVER. Abbruch." }
if ($dev.InstanceId -notmatch '5C92044868') {
    $a = Read-Host "Seriennummer des WROVER (5C92044868) nicht erkannt. Trotzdem flashen? (j/n)"
    if ($a -ne 'j') { exit 1 }
}

$flashArgs = @('--chip', 'esp32', '--port', $Port, '--baud', '460800', 'write-flash',
    '0x1000', 'bootloader.bin',
    '0x8000', 'partitions.bin',
    '0x10000', 'KaRadio32.bin')
if (-not $OhneKonfig) { $flashArgs += @('0x3a2000', 'standard_psram_dan_keys.bin') }

# otadata loeschen, damit der WROVER sicher die neue Firmware (ota_0) startet
& $esptool.FullName --chip esp32 --port $Port --baud 460800 erase-region 0xd000 0x2000
if ($LASTEXITCODE -ne 0) { throw "erase-region fehlgeschlagen" }
& $esptool.FullName @flashArgs
if ($LASTEXITCODE -ne 0) { throw "write-flash fehlgeschlagen" }
Write-Host "Fertig. Der WROVER startet neu."

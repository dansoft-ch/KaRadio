# Archiv – frühere Lösungen

Diese Varianten werden nicht mehr weiterentwickelt. Aktuell ist die **Firmware NG mit Bluetooth** im Hauptordner (`firmware/`, siehe [README](../README.md)): Der ESP32-WROVER liest Tasten und Potis selbst, es braucht kein zweites Board.

Jeder Stand ist zusätzlich als Tag gesichert. So holt man ihn vollständig zurück (Dateien an ihrem damaligen Ort, damalige `CLAUDE.md`):
```
git switch --detach archiv/s3-usbhost
git switch main            # zurück
```

| Ordner / Datei | Was | Stand | Tag |
|---|---|---|---|
| `TastaturCode/` | Pro Mini: Tasten als NEC-IR-Signal an `P_IR_SIGNAL` (erste Variante) | 24.10.2026 | `archiv/promini-ir-uart` |
| `TastaturLautstaerkeIR/` | Pro Mini: Tasten + Poti als IR-Signal, Lautstärke nur per lauter/leiser (`K_UP` = 0xFF0040, `K_DOWN` = 0xFF0044). Lief nur auf einem von drei Boards zuverlässig. | 30.10.2026 | `archiv/promini-ir-uart` |
| `TastaturLautstaerkeUART/` | Pro Mini: Taste n → `cli.play("n")`, Poti → `cli.vol("0..254")` als Textzeile über UART (115200 Baud) an RX0/GPIO 3 des WROVER | 25.10.2026 | `archiv/promini-ir-uart` |
| `TestUART/` | einfacher UART-Test (9600 Baud, gibt „test n“ aus) | 24.10.2026 | `archiv/promini-ir-uart` |
| `TastaturLautstaerkeUART-S3/` | ESP32-S3 als USB-Host: schickt die Befehle übers USB-Kabel an den WROVER, startet ihn per RTS neu, RGB-LED zeigt Sender und Lautstärke | 05.11.2026 | `archiv/s3-usbhost` |
| `standard_psram_dan.csv` / `.bin` | KaRadio32-Hardware-Konfiguration für die Varianten oben (VS1053 per SPI, IR an GPIO 21, I2S 25/26/22, IR-Codes `K_0`–`K_5`). Gehört an `0x3a2000`. | 24.10.2026 | beide |
| `KaRadio-Factsheet-S3.docx` | Factsheet der S3-Lösung | 05.11.2026 | `archiv/s3-usbhost` |
| `KaRadio-Factsheet-FirmwareNG.docx` | Factsheet Firmware NG v1 (ohne Bluetooth) | 06.11.2026 | `firmware-ng-v1` |

Die Sketche liegen jeweils in einem gleichnamigen Ordner, damit arduino-cli sie direkt bauen kann.

## Technische Notizen zur S3-Lösung

### Ablauf
- Der S3 arbeitet am Port „USB“ als **USB-Host** und schickt die Befehle per USB-Kabel an den USB-Seriell-Chip des WROVER-Boards (wie vorher der serielle Monitor am PC). Treiber im Sketch für CP210x, CH340/CH341 und CDC-ACM (CH9102/CH343; der WROVER hat einen CH343).
- Tasten GPIO 4–9, Poti GPIO 1. Zusätzlich gehen die Befehle über UART0 (`Serial0`, Port „COM“ zum Mitlesen, TX0 = GPIO 43, dort auch `[USB] …`-Statusmeldungen).
- Die eingebaute RGB-LED zeigt Farbe = gewählter Sender (rot, grün, blau, gelb, cyan, magenta; weiß vor der ersten Taste), Helligkeit = Lautstärke.
- **Start-Ablauf** (nur CDC, getestet 05.11.2026): Der WROVER startet beim gemeinsamen Einschalten nicht von selbst. Der S3 löst nach dem Anmelden einen Reset per RTS aus (GPIO 0 bleibt HIGH). Die Ausgabe des WROVER wird mitgelesen und als `[WROVER] …` auf dem COM-Port ausgegeben. Befehle gehen erst nach `READY.` raus. Beim Kaltstart scheitert der erste WLAN-Versuch meist und KaRadio stürzt bei „Try next AP“ ab; der S3 erkennt das (bzw. 30 s ohne `READY`) und startet den WROVER erneut per RTS (max. 5×). Der zweite Versuch klappt, `READY` nach ca. 16 s.

### Bauen und flashen (arduino-cli der Arduino IDE)
```
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 --upload -p COM9 archiv/TastaturLautstaerkeUART-S3
```
- FQBN `esp32:esp32:esp32s3`, „USB CDC On Boot“ aus → `Serial0` = UART0/COM-Port.
- Der Core (esp32s3-libs 3.3.x, IDF 5.5) enthält nur `usb/usb_host.h`, keine CDC/VCP-Host-Treiber → eigener Mini-Treiber im Sketch.
- **USB-Host braucht `enum_filter_cb`** (`CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK=y`): ohne Callback bleibt die Anmeldung hängen. Im Sketch: `usbEnumFilter`, gibt `true` zurück.
- S3 und WROVER haben beide einen **CH343** (VID 1A86 / PID 55D3). Seriennummern: S3 = 5CE5194172 (COM9), altes WROVER-Board = 5C92044868. Vor dem Flashen mit `Get-PnpDevice -PresentOnly -Class Ports` prüfen.
- Chip: ESP32-S3 (QFN56) rev v0.2, 8 MB Octal-PSRAM → GPIO 33–37 nicht verwenden.

### Hardware des S3-Boards (2 USB-Ports)
- „USB“ = native USB (GPIO 19/20), „COM“ = USB-UART-Brücke an UART0 (TX 43, RX 44).
- **Lötbrücke „USB-OTG“ geschlossen**: Port „USB“ liefert 5 V und versorgt den WROVER. Daher nie beide Ports gleichzeitig an PC/Netzteil, WROVER nicht zusätzlich versorgen, Netzteil am COM-Port ≥ 1 A (besser 2 A).
- `analogRead` liefert 0–4095; an den Enden kleine Totzone. Kein Spannungsteiler nötig (3,3 V auf beiden Seiten).

# KaRadio – Projektnotizen

## Aufbau (2 Boards arbeiten zusammen)

| Board | Aufgabe | Software |
|---|---|---|
| **ESP32 WROVER** | Das eigentliche Radio | KaRadio32 (geflasht), Konfiguration `standard_psram_dan.csv` / `.bin` |
| **ESP32-S3** (2 USB-Ports) | Liest Tastatur (6 Taster) und Lautstärke-Poti, schickt Befehle per UART an den WROVER | `TastaturLautstaerkeUART-S3/TastaturLautstaerkeUART-S3.ino` |

Ablauf: Taste n → `cli.play("n")`, Poti → `cli.vol("0..254")`, jeweils als Textzeile (115200 Baud). Der S3 arbeitet am Port „USB“ als **USB-Host** und schickt die Befehle per USB-Kabel an den USB-Seriell-Chip des WROVER-Boards (wie vorher der serielle Monitor am PC). Treiber im Sketch für CP210x, CH340/CH341 und CDC-ACM (CH9102/CH343; der WROVER hat einen CH343).

Start-Ablauf (nur CDC, getestet 05.11.2026): Der WROVER startet beim gemeinsamen Einschalten nicht von selbst. Der S3 löst deshalb nach dem Anmelden einen Reset per RTS aus (wie die Arduino-IDE, GPIO 0 bleibt HIGH). Die Ausgabe des WROVER wird über USB mitgelesen und als `[WROVER] …` auf COM9 ausgegeben. Befehle gehen erst nach `READY.` von KaRadio raus; danach wird die Poti-Lautstärke gesendet. Beim Kaltstart scheitert der erste WLAN-Versuch meist (`auth -> init (200)`), und KaRadio stürzt danach bei „Try next AP“ ab (Guru Meditation, gdb stub). Der S3 erkennt `Wifi Disconnected`/`Try next AP` vor `READY`, Absturz oder 30 s ohne `READY` und startet den WROVER erneut per RTS (max. 5×). Der zweite Versuch klappt; `READY` nach ca. 16 s. Stromversorgung: Netzteil/PC nur am Port „COM“ des S3; der S3 versorgt den WROVER über das USB-Kabel am Port „USB“ (dafür Lötbrücke „USB-OTG“ geschlossen, siehe Hardware). Zusätzlich gehen die Befehle wie bisher über UART0 (`Serial0`, Port „COM“ zum Mitlesen, Pin TX0 = GPIO 43 → alternativ per Draht an RX0/GPIO 3 des WROVER, dann GND verbinden). Der Rückkanal (WROVER → S3) läuft nur über USB und dient der Start-Überwachung und zum Mitlesen.

## Firmware Next Generation (Branch `firmware-ng`, ab 06.11.2026)

Alternative ohne S3: KaRadio32 selbst liest Tasten und Poti (`firmware/`, Anleitung in `firmware/README.md`). Neue NVS-Einträge `P_KEY0..5` (Tasten gegen GND, GPIO 13, 14, 27, 15, 5, 2) und `P_VOL_POT` (GPIO 39, nur ADC1). Gebaut wird per GitHub Actions (`.github/workflows/firmware.yml`, Ubuntu-20.04-Container, ESP-IDF 3.3.6, Ka-Radio32 auf festem Commit + `firmware/karadio.patch`), weil auf dem PC kein ESP-IDF/Python installiert ist. Flashen mit `firmware/flash.ps1 -Port COMx` (esptool aus Arduino15, prüft die Seriennummer des WROVER).

**Neues WROVER-Board für Firmware NG** (06.11.2026): CH343-Seriennummer **5C92044814**, COM10, ESP32-D0WD-V3, 16 MB Flash. Ab Werk lag ein Testsketch „ESP32-WROOM-32E“ drauf. Komplett gelöscht und mit Firmware NG + `standard_psram_dan_keys.bin` geflasht (flash.ps1 kennt beide WROVER-Seriennummern). Getestet: alle 6 Tasten und Poti (GPIO 39) werden erkannt, die Lautstärke folgt dem Poti, kein VS1053 erkannt (Ausgabe über I2S). WLAN und Senderliste sind noch leer: AP „WifiKaRadio“, http://192.168.4.1.

**Stand v2 `firmware-ng-bt`** (07.11.2026 abends, geflasht auf neues Board an COM3, vom Nutzer getestet: „klappt super“): Bluetooth immer an, automatische Umschaltung (Handy spielt → Radio stoppt → `init_i2s()`; 15 s Pause/Trennen → I2S abbauen → `playStation("255")`; Sendertaste → `esp_a2d_sink_disconnect` + Radio). Taste 6 = Sender 6 (`P_KEY5`=2, kein `P_KEY_BT`). Zusätzlich gespart: WLAN-Puffer RX 10/TX 6, AAC-Decoder `buf_create` statt `buf_create_dma` (20 KB). Intern frei: Radio+BT ~27 KB, BT-Ton ~32 KB, Radio mit verbundenem Handy ~18 KB. v1 (Taste 6 = BT, Neustart) = Tag `firmware-ng-bt-v1`.

**Versionen** (07.11.2026): Stabiler Stand von `firmware-ng` gesichert als Tag/Release `firmware-ng-v1` (Release wird von GitHub Actions bei Tags `firmware-ng-v*` angelegt). Bluetooth-Lautsprecher (A2DP-Sink, Taste 6 = BT) wird im Branch `firmware-ng-bt` entwickelt; `firmware-ng` bleibt solange unverändert. Übersicht für Menschen: `VERSIONEN.md`.

**Stand `firmware-ng-bt`** (07.11.2026, auf das neue WROVER-Board geflasht, damals an **COM3**; Start ok: alle Tasten/Potis erkannt, AP-Modus, `READY` nach ca. 17 s, interner RAM beim Start 166 KB; Bluetooth/Mute am Gerät noch nicht getestet). Test 07.11.: Radio-Stream brach ab, Webinterface tot, intern nur 0,8–37 KB frei (BT reserviert fest 0xdb5c). Lösung: Beim Start ohne eingerastete BT-Taste `esp_bt_controller_mem_release(ESP_BT_MODE_BTDM)`, Wechsel Radio↔BT per Neustart. Ausserdem ging KaRadio nach einem WLAN-Fehlversuch in den AP-Modus (früher stürzte es ab und der Neustart rettete es) → Patch: 3 Versuche, dann 1 Neustart (RTC_NOINIT-Zähler), dann AP. Zusätzlich `CONFIG_WIFI_LWIP_ALLOCATION_FROM_SPIRAM_FIRST=y`, `SPIRAM_MALLOC_ALWAYSINTERNAL=4096`, `SPIRAM_MALLOC_RESERVE_INTERNAL=32768`, `ESP32_WIFI_DYNAMIC_RX_BUFFER_NUM=32`, `ESP32_WIFI_RX_BA_WIN=16` (mit PSRAM-Option Pflicht ≥ 16, sonst `#error` in wifi_init.c). Ergebnis 07.11. abends: intern beim Abspielen stabil ~66 KB (vorher 25–30 KB, Einbrüche bis 0,8 KB); Kaltstart: 2. WLAN-Versuch klappt, READY nach ~22 s. Build-Fehler stehen als Annotation öffentlich in der Check-Run-API (Logs brauchen Login). Bluetooth getestet 07.11. abends: Ton kommt. Vorher Absturz (LoadProhibited), weil KaRadio den I2S-Treiber nur mit laufendem Radio-Decoder installiert → `btSpeakerStart` ruft `init_i2s()` selbst auf. Intern frei im BT-Betrieb stabil ~20 KB (WLAN an). Mute-Poti: GPIO 36 schwebte ohne Poti und löste „Mute an“ aus; seit Poti angeschlossen `P_MUTE_POT=36`, `MUTE_STEP` 410 (~10 %, 5 % war zu empfindlich), getestet. GitHub Actions legt nach jedem Branch-Build die `.bin` in `firmware/bin/` (Bot-Commit) → `git pull`, dann `firmware/flash.ps1 -Port COMx`. `firmware/btspeaker.c` (A2DP-Sink „Biennophone“, Ton über `render_samples`), Rasttaste `P_KEY_BT` = GPIO 2 (statt `P_KEY5`), Mute-Poti `P_MUTE_POT` = GPIO 36 nach Drehrichtung (±5 %, Mute in `audio_renderer.c` per `renderer_set_mute`). Patch sperrt `clientConnect*` im BT-Betrieb, schaltet BT in `sdkconfig` ein (IDF-3.3-Namen: `CONFIG_CLASSIC_BT_ENABLED`, `CONFIG_A2DP_ENABLE`, `CONFIG_BTDM_CONTROLLER_MODE_BR_EDR_ONLY`) und vergrössert `ota_0` auf 0x380000 (ota_1 entfernt, übrige Offsets gleich). Patch neu erzeugen: Ka-Radio32 mit `core.eol=lf` auschecken (das Repo hat sonst CRLF im Arbeitsverzeichnis), Änderungen machen, `git diff > firmware/karadio.patch`.

## Dateien

- `TastaturLautstaerkeUART-S3/TastaturLautstaerkeUART-S3.ino` – **aktuelle Lösung für den S3**: Tasten GPIO 4–9, Poti GPIO 1, Befehle per USB-Host (Port „USB“) an den WROVER und zum Mitlesen über `Serial0` (TX GPIO 43 / COM-Port, dort auch `[USB] …`-Statusmeldungen). Die eingebaute RGB-LED (einzige steuerbare LED des Boards) zeigt beides: Farbe = gewählter Sender (rot, grün, blau, gelb, cyan, magenta; weiß vor der ersten Taste), Helligkeit = Lautstärke.
- `TastaturLautstaerkeUART.ino` – gleiche Funktion für den Pro Mini (Vorlage für die S3-Version).
- `TastaturLautstaerkeIR.ino` – ältere Variante (Pro Mini): Tasten + Poti als NEC-IR-Signal an `P_IR_SIGNAL`, Lautstärke nur per lauter/leiser (`K_UP` = 0xFF0040, `K_DOWN` = 0xFF0044). Lief laut Kommentar nur auf einem von drei Boards zuverlässig.
- `TastaturCode.ino` – erste Variante (Pro Mini): nur Tasten als NEC-IR-Signal.
- `TestUART.ino` – einfacher UART-Test (9600 Baud, gibt „test n“ aus).
- `standard_psram_dan.csv` / `.bin` – KaRadio32-Hardware-Konfiguration (NVS-Partition) für den WROVER, inkl. IR-Codes `K_0`–`K_5` für die IR-Varianten.

## Bauen und flashen (lokal, arduino-cli der Arduino IDE)
- arduino-cli: `C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe` (Core esp32:esp32 3.3.x, arduino:avr)
- arduino-cli braucht einen Ordner mit gleichem Namen wie die `.ino`. Der S3-Sketch liegt deshalb in `TastaturLautstaerkeUART-S3/` und kann direkt gebaut werden; die übrigen Sketche im Repo-Root müssten dafür in einen gleichnamigen Ordner kopiert werden.
- S3-Sketch: FQBN `esp32:esp32:esp32s3` (Standard: „USB CDC On Boot“ aus → `Serial0` = UART0/COM-Port). Kompiliert fehlerfrei, auch mit USB-Host (Stand 05.11.2026). Der Core enthält nur `usb/usb_host.h`, keine fertigen CDC/VCP-Host-Treiber – daher eigener Mini-Treiber im Sketch.
- **USB-Host braucht `enum_filter_cb`**: Der Core (esp32s3-libs 3.3.x, IDF 5.5) ist mit `CONFIG_USB_HOST_ENABLE_ENUM_FILTER_CALLBACK=y` gebaut. Ohne Callback bleibt die Anmeldung hängen (Port aktiv, aber kein NEW_DEV, keine Fehlermeldung). Im Sketch gesetzt (`usbEnumFilter`, gibt `true` zurück); seitdem meldet COM9 `[USB] Geraet angeschlossen: VID 1A86 PID 55D3` / `Chip: CDC-ACM, Einrichtung ok` (05.11.2026).
- Der WROVER hat ebenfalls einen **CH343** (gleiche VID/PID wie der S3!). Hängt er am PC, erscheint er als eigener COM-Port (zuletzt COM4, Seriennummer 5C92044868; S3 = 5CE5194172). Vor dem Flashen per `Get-PnpDevice -PresentOnly -Class Ports` die Seriennummer prüfen. Blaue LED am WROVER-Board = USB vom Host angemeldet.
- Über PowerShell aufrufen (Pfad mit Leerzeichen):
  ```
  & "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --fqbn esp32:esp32:esp32s3 --upload -p COM9 TastaturLautstaerkeUART-S3
  ```
- Ports: `arduino-cli board list`. **COM9 = COM-Port des S3** (CH343, VID 1A86 / PID 55D3, vom Nutzer bestätigt, Flashen dort funktioniert). Vor dem Flashen prüfen, dass wirklich der S3 an COM9 hängt, damit nicht versehentlich der WROVER (KaRadio) überschrieben wird. Serieller Monitor der IDE muss geschlossen sein.
- Test nach dem Flashen: COM9 mit 115200 Baud mitlesen (PowerShell `System.IO.Ports.SerialPort`). Nach 10 s erscheint `cli.vol("…")`, beim Drehen am Poti bzw. Drücken einer Taste weitere Befehle.
- Chip laut esptool: ESP32-S3 (QFN56) rev v0.2, 8 MB PSRAM eingebaut (Octal → GPIO 33–37 nicht verwenden).

## Hardware

### ESP32 WROVER (KaRadio)
- Pinbelegung siehe `standard_psram_dan.csv`: VS1053 per SPI, IR an GPIO 21, I2S an 25/26/22.
- Verstärker MAX98357A: grün → 25, blau → 26, gelb → 22, rot → 5 V, schwarz → GND.
- UART-Eingang: RX0 = GPIO 3.

### ESP32-S3 (Tastatur/Lautstärke) – Board mit 2 USB-Ports
- Typisch bei solchen Boards (z. B. DevKitC-1):
  - **„USB“** = native USB des S3 (USB-Serial-JTAG, GPIO 19/20)
  - **„COM“/„UART“** = USB-UART-Brücke an UART0 (TX = GPIO 43, RX = GPIO 44)
- **Lötbrücke „USB-OTG“ ist geschlossen** (05.11.2026, funktioniert): Port „USB“ liefert jetzt 5 V und versorgt den WROVER. „IN-OUT“ bleibt offen. Folgen:
  - Nie beide Ports des S3 gleichzeitig an PC/Netzteile hängen (zwei Stromquellen direkt verbunden).
  - WROVER nicht zusätzlich selbst versorgen, solange das USB-Kabel zum S3 steckt.
  - Zum Flashen nur „COM“ an den PC; das Kabel zum WROVER darf stecken bleiben.
  - Die Quelle am COM-Port versorgt beide Boards (WROVER + VS1053 + MAX98357A bis ca. 700 mA Spitze) → Netzteil ≥ 1 A, besser 2 A. Startet der WROVER bei lauter Musik neu: Brownout, stärkeres Netzteil nehmen.
- Wohin `Serial` ausgibt, hängt in der Arduino-IDE von **„USB CDC On Boot“** ab: aktiviert → native USB, deaktiviert → UART0 (GPIO 43). Für die Verbindung zum WROVER muss die Ausgabe auf einem echten TX-Pin landen – sicherer ist `Serial1` mit festen Pins (`Serial1.begin(115200, SERIAL_8N1, RX, TX)`).
- Beide Boards arbeiten mit 3,3 V → kein Spannungsteiler nötig (der Hinweis im Sketch gilt nur für einen 5-V-Pro-Mini).
- Unterschiede zum Pro Mini (in `TastaturLautstaerkeUART-S3.ino` bereits berücksichtigt):
  - `analogRead` liefert 0–4095 (12 Bit) statt 0–1023; an den Enden gibt es eine kleine Totzone, weil der S3-ADC dort ungenau ist.
  - `A0` = GPIO 1; Pin 13 hat keine eingebaute LED → stattdessen RGB-LED (`RGB_BUILTIN`, DevKitC-1: GPIO 48 bzw. 38).
- Die Pinbelegung aus `standard_psram_dan.csv` passt nicht auf den S3 (GPIO 22–25 gibt es dort nicht; 26–32 sind für Flash/PSRAM reserviert).

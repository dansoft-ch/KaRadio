# KaRadio – Projektnotizen

## Aufbau (2 Boards arbeiten zusammen)

| Board | Aufgabe | Software |
|---|---|---|
| **ESP32 WROVER** | Das eigentliche Radio | KaRadio32 (geflasht), Konfiguration `standard_psram_dan.csv` / `.bin` |
| **ESP32-S3** (2 USB-Ports) | Liest Tastatur (6 Taster) und Lautstärke-Poti, schickt Befehle per UART an den WROVER | `TastaturLautstaerkeUART-S3.ino` |

Ablauf: Taste n → `cli.play("n")`, Poti → `cli.vol("0..254")`, jeweils als Textzeile (115200 Baud) von S3 GPIO 17 (`Serial1` TX) an RX0 (GPIO 3) des WROVER. GND beider Boards verbinden. Rückkanal (WROVER TX → S3) wird nicht gebraucht.

## Dateien

- `TastaturLautstaerkeUART-S3.ino` – **aktuelle Lösung für den S3**: Tasten GPIO 4–9, Poti GPIO 1, Befehle über `Serial1` (TX GPIO 17), Debug-Ausgabe über USB (`Serial`), Rückmeldung über die RGB-LED.
- `TastaturLautstaerkeUART.ino` – gleiche Funktion für den Pro Mini (Vorlage für die S3-Version).
- `TastaturLautstaerkeIR.ino` – ältere Variante (Pro Mini): Tasten + Poti als NEC-IR-Signal an `P_IR_SIGNAL`, Lautstärke nur per lauter/leiser (`K_UP` = 0xFF0040, `K_DOWN` = 0xFF0044). Lief laut Kommentar nur auf einem von drei Boards zuverlässig.
- `TastaturCode.ino` – erste Variante (Pro Mini): nur Tasten als NEC-IR-Signal.
- `TestUART.ino` – einfacher UART-Test (9600 Baud, gibt „test n“ aus).
- `standard_psram_dan.csv` / `.bin` – KaRadio32-Hardware-Konfiguration (NVS-Partition) für den WROVER, inkl. IR-Codes `K_0`–`K_5` für die IR-Varianten.

## Hardware

### ESP32 WROVER (KaRadio)
- Pinbelegung siehe `standard_psram_dan.csv`: VS1053 per SPI, IR an GPIO 21, I2S an 25/26/22.
- Verstärker MAX98357A: grün → 25, blau → 26, gelb → 22, rot → 5 V, schwarz → GND.
- UART-Eingang: RX0 = GPIO 3.

### ESP32-S3 (Tastatur/Lautstärke) – Board mit 2 USB-Ports
- Typisch bei solchen Boards (z. B. DevKitC-1):
  - **„USB“** = native USB des S3 (USB-Serial-JTAG, GPIO 19/20)
  - **„COM“/„UART“** = USB-UART-Brücke an UART0 (TX = GPIO 43, RX = GPIO 44)
- Wohin `Serial` ausgibt, hängt in der Arduino-IDE von **„USB CDC On Boot“** ab: aktiviert → native USB, deaktiviert → UART0 (GPIO 43). Für die Verbindung zum WROVER muss die Ausgabe auf einem echten TX-Pin landen – sicherer ist `Serial1` mit festen Pins (`Serial1.begin(115200, SERIAL_8N1, RX, TX)`).
- Beide Boards arbeiten mit 3,3 V → kein Spannungsteiler nötig (der Hinweis im Sketch gilt nur für einen 5-V-Pro-Mini).
- Unterschiede zum Pro Mini (in `TastaturLautstaerkeUART-S3.ino` bereits berücksichtigt):
  - `analogRead` liefert 0–4095 (12 Bit) statt 0–1023; an den Enden gibt es eine kleine Totzone, weil der S3-ADC dort ungenau ist.
  - `A0` = GPIO 1; Pin 13 hat keine eingebaute LED → stattdessen RGB-LED (`RGB_BUILTIN`, DevKitC-1: GPIO 48 bzw. 38).
- Die Pinbelegung aus `standard_psram_dan.csv` passt nicht auf den S3 (GPIO 22–25 gibt es dort nicht; 26–32 sind für Flash/PSRAM reserviert).

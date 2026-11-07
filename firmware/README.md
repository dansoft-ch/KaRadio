# Firmware Next Generation – KaRadio32 mit Sendertasten und Lautstärke-Poti

Der WROVER liest die 6 Sendertasten und das Lautstärke-Poti selbst aus. Der ESP32-S3 wird nicht mehr gebraucht.

## Was geändert ist

- `stationkeys.c` / `.h`: neue Funktion in KaRadio32 (Release 2.4 R1).
  - Taste n → Sender n (wie `cli.play("n")`). Der Sender wird gespeichert und nach dem Einschalten wieder gespielt.
  - Poti → Lautstärke 0..254 (wie `cli.vol("n")`). In den ersten 60 s nach dem Start gewinnt immer das Poti, damit die gespeicherte Lautstärke es nicht überschreibt. Danach ändert sich die Lautstärke nur, wenn jemand am Poti dreht. Die Weboberfläche kann die Lautstärke dann weiterhin ändern.
- `karadio.patch`: startet die neue Funktion und stellt den Absturz-Handler um. Nach einem Absturz (z. B. bei „Try next AP“) startet der WROVER jetzt neu, statt im „gdb stub“ hängen zu bleiben. Das hat bisher der S3 per RTS übernommen.
- `standard_psram_dan_keys.csv`: eure Hardware-Konfiguration mit den neuen Einträgen. Drehgeber und I2C/LCD sind abgeschaltet, weil ihre Pins jetzt für Tasten gebraucht werden.

Die Pins stehen in der Hardware-Partition und lassen sich ohne neue Firmware ändern (`.csv` anpassen, `.bin` neu erzeugen und flashen):

| Eintrag | GPIO | Anschluss |
|---|---|---|
| `P_KEY0` | 13 | Taste 1 (Sender 0) gegen GND |
| `P_KEY1` | 14 | Taste 2 (Sender 1) gegen GND |
| `P_KEY2` | 27 | Taste 3 (Sender 2) gegen GND |
| `P_KEY3` | 15 | Taste 4 (Sender 3) gegen GND |
| `P_KEY4` | 5  | Taste 5 (Sender 4) gegen GND |
| `P_KEY5` | 2 → im Branch `firmware-ng-bt`: 255 | Taste 6 (Sender 5) gegen GND |
| `P_KEY_BT` | 2 (nur `firmware-ng-bt`) | Taste 6 = Bluetooth (Rasttaste) gegen GND |
| `P_VOL_POT` | 39 (VN / SVN) | Poti-Schleifer; Poti-Enden an 3,3 V und GND |
| `P_MUTE_POT` | 36 (VP / SVP, nur `firmware-ng-bt`) | Schleifer des Ein/Aus-Knopf-Potis; Enden an 3,3 V und GND; 100 kΩ nach 3,3 V |

- Die Tasten brauchen keinen Widerstand (interner Pull-up). `255` = Taste nicht benutzt.
- Das Poti nur an einen ADC1-Pin hängen (32–39, frei sind 35, 36 und 39). ADC2-Pins funktionieren bei aktivem WLAN nicht.
- Poti **an 3,3 V, nicht an 5 V**.
- GPIO 2, 5 und 15 sind Boot-Pins. Beim Einschalten keine Taste gedrückt halten.

## Bluetooth-Lautsprecher und Mute (Branch `firmware-ng-bt`)

- `btspeaker.c` / `.h`: Das Radio ist per Bluetooth als **„Biennophone“** sichtbar (A2DP-Sink, Koppeln ohne PIN). Der Ton geht wie beim Radio über I2S an den MAX98357A. Lautstärke-Poti und Mute wirken auch hier. Nur mit I2S-Ausgang, nicht mit VS1053.
- **Bluetooth-Taste** (`P_KEY_BT`, GPIO 2, früher Taste 6 / `P_KEY5`): Rasttaste. Beim Einschalten entscheidet sie die Betriebsart: eingerastet = Bluetooth, frei = Radio. Im Radio-Betrieb gibt die Firmware den fest reservierten Bluetooth-Speicher (ca. 56 KB intern) ans Radio ab; ohne diesen Speicher bricht der Radio-Stream ab und das Webinterface hängt. **Ein Wechsel Radio ↔ Bluetooth startet den WROVER deshalb neu** (ca. 15–20 s bis Ton). Eine Sendertaste im Bluetooth-Betrieb speichert den Sender, schaltet Autostart ein und startet neu ins Radio.
- **Speicher**: WLAN- und Netzwerkpuffer liegen zuerst im PSRAM (4 MB), kleine Speicherblöcke ab 4 KB ebenfalls. Gemessen beim Radio-Streaming: rund 66 KB interner Speicher frei, stabil.
- **WLAN beim Start**: Der erste Versuch scheitert beim Kaltstart oft. Die Firmware versucht dasselbe WLAN bis zu 3-mal, startet dann einmal neu und geht erst danach in den AP-Modus „WifiKaRadio“.
- **Mute-Poti** (`P_MUTE_POT`, GPIO 36): Schleifer des originalen Ein/Aus-Knopfs. Es zählt die Drehrichtung: ca. 5 % aufdrehen → Mute aus, ca. 5 % zudrehen → Mute an. Nach dem Einschalten ist Mute aus.
  - GPIO 36 hat **keinen eingebauten Pull-up** (gilt für GPIO 34–39). Damit ein fehlendes Mute-Poti nichts auslöst, einen **Widerstand 100 kΩ von GPIO 36 nach 3,3 V** fest einlöten, oder `P_MUTE_POT` auf `255` setzen. Bei GPIO 32/33 schaltet die Firmware den internen Pull-up selbst ein.
- **Start-Wächter**: Der Hardware-Watchdog des ESP32 (RTC-Watchdog) läuft ab dem Bootloader mit. Erreicht KaRadio nicht innerhalb von **90 s** `READY`, startet der Chip komplett neu. Nach `READY` wird er abgeschaltet (Meldung `Start-Waechter aus (Neustart-Grund beim Start: n)`; n = 1 normales Einschalten, 3 Neustart per Software, 4 Absturz, 7 Start-Wächter/Watchdog, 9 Unterspannung/Brownout). Abstürze (Guru Meditation) führen wie bisher zum Neustart. Nicht abgedeckt: Der Chip startet gar nicht (kein Text im seriellen Monitor). Das ist ein Hardware-Thema, meist zu kurze Reset-Verzögerung am EN-Pin beim Einschalten; Abhilfe ist ein grösserer Kondensator (1–10 µF) von EN nach GND.
- Läuft kein WLAN-Router, wartet KaRadio beim Start auf das WLAN (bzw. geht in den AP-Modus). Erst danach sind Tasten, Poti und Bluetooth aktiv.
- Die App-Partition ist auf 3,5 MB vergrössert (Bluetooth braucht Platz); dafür gibt es kein Web-Update (OTA) mehr. Sender, WLAN-Daten und Hardware-Konfiguration liegen an derselben Adresse wie bisher und bleiben erhalten. `flash.ps1` schreibt die neue Partitionstabelle mit.

## Bauen

GitHub Actions baut bei jedem Push in `firmware/` automatisch (Workflow „Firmware“). Ergebnis ist das Artefakt **karadio32-firmware**:
`bootloader.bin`, `partitions.bin`, `KaRadio32.bin`, `standard_psram_dan_keys.bin`, `flash.ps1`.

Herunterladen:
```
gh run download --name karadio32-firmware --dir firmware\out
```

## Flashen

1. WROVER direkt per USB an den PC hängen (S3-Kabel abziehen, siehe Stromversorgung in `CLAUDE.md`).
2. COM-Port des WROVER suchen (Seriennummer 5C92044868, zuletzt COM4):
   `Get-PnpDevice -PresentOnly -Class Ports`
3. Im Ordner mit den Dateien:
   ```
   powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COM4
   ```
   Das Skript prüft die Seriennummer und bricht ab, wenn der S3 dranhängt. Es nutzt `esptool.exe` aus der Arduino IDE.

Sender, WLAN-Daten und Einstellungen bleiben erhalten. Nur die Firmware und die Hardware-Konfiguration werden neu geschrieben.

**Zurück zur alten Firmware:** `KaRadio32.bin` von karawin flashen (0x10000) und die alte `standard_psram_dan.bin` an 0x3a2000.

## Testen

Seriellen Monitor (115200 Baud) am WROVER öffnen. Beim Start erscheinen:
```
stationkeys: Taste 0 an GPIO 13
…
stationkeys: Poti an GPIO 39 (ADC1 Kanal 3)
```
Ein Tastendruck zeigt `stationkeys: Taste n -> Sender n`. Beim Drehen am Poti erscheint `##CLI.VOL#: …`.

Bluetooth und Mute (`firmware-ng-bt`):
```
stationkeys: Bluetooth-Taste an GPIO 2
stationkeys: Mute-Poti an GPIO 36 (ADC1 Kanal 0)
btspeaker: Bluetooth-Betrieb, freier Speicher …      (Taste 6 eingerastet)
btspeaker: sichtbar als "Biennophone"
btspeaker: Handy verbunden / Wiedergabe startet (44100 Hz)
stationkeys: Mute an / Mute aus                       (Ein/Aus-Knopf gedreht)
btspeaker: Bluetooth aus, zurueck zum Radio           (Sendertaste)
```

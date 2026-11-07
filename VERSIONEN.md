# Versionen und Branches – Übersicht

Diese Datei erklärt, welche Version des Radios wo liegt und wie man zu einer funktionierenden Version zurückkommt. Stand: 07.11.2026.

## Kurzfassung

| Name | Art | Inhalt | Zustand |
|---|---|---|---|
| `main` | Branch | Lösung mit **zwei Boards**: ESP32-S3 liest Tasten + Poti und schickt Befehle an den WROVER (`TastaturLautstaerkeUART-S3/`) | funktioniert |
| `firmware-ng` | Branch | **Firmware NG**: der WROVER liest 6 Sendertasten und das Poti selbst, kein S3 nötig (`firmware/`) | funktioniert, getestet 06.11.2026 |
| `firmware-ng-v1` | Tag + Release | fester Stand von `firmware-ng` mit fertigen `.bin`-Dateien zum Flashen | **gesicherte Version, nicht verändern** |
| `firmware-ng-bt` | Branch | Weiterentwicklung: Firmware NG **plus Bluetooth-Lautsprecher** | in Arbeit |

Regel: In `firmware-ng` wird nichts mehr geändert, solange `firmware-ng-bt` in Arbeit ist. Neues kommt nur in `firmware-ng-bt`.

## Die gesicherte Version `firmware-ng-v1` wieder flashen

Ohne etwas zu bauen:

1. Auf GitHub das Release öffnen: https://github.com/dansoft-ch/KaRadio/releases/tag/firmware-ng-v1
2. Alle Dateien herunterladen (`bootloader.bin`, `partitions.bin`, `KaRadio32.bin`, `standard_psram_dan_keys.bin`, `flash.ps1`) und in einen leeren Ordner legen.
3. WROVER direkt per USB an den PC, COM-Port suchen (`Get-PnpDevice -PresentOnly -Class Ports`, neues Board: Seriennummer 5C92044814).
4. Im Ordner: `powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COMx`

Sender, WLAN-Daten und Einstellungen bleiben erhalten. Mehr Details: `firmware/README.md`.

## Mit den Branches arbeiten (git)

Welcher Branch ist gerade aktiv?
```
git branch --show-current
```

Wechseln (vorher alles committen):
```
git switch firmware-ng-bt     # Bluetooth-Entwicklung
git switch firmware-ng        # stabile Firmware NG
git switch main               # S3-Lösung
```

Den Quellcode der gesicherten Version ansehen, ohne einen Branch zu verändern:
```
git switch --detach firmware-ng-v1
```
(zurück mit `git switch firmware-ng-bt`)

Bluetooth verwerfen und neu anfangen: Branch `firmware-ng-bt` löschen und neu ab `firmware-ng` anlegen. `firmware-ng` und der Tag sind davon nicht betroffen.

Bluetooth ist fertig und gut: `firmware-ng-bt` in `firmware-ng` übernehmen (`git switch firmware-ng`, dann `git merge firmware-ng-bt`), neuen Tag `firmware-ng-v2` setzen und hochladen (`git tag firmware-ng-v2`, `git push origin firmware-ng-v2`). GitHub legt das Release dann automatisch an.

## Wie die Firmware gebaut wird

Auf dem PC ist kein ESP-IDF installiert. GitHub Actions baut (Workflow `.github/workflows/firmware.yml`):

- bei jedem Push mit Änderungen in `firmware/` → Artefakt „karadio32-firmware“ (wird nach 90 Tagen gelöscht),
- bei jedem Tag `firmware-ng-v…` → zusätzlich ein **Release** mit denselben Dateien (bleibt dauerhaft).

Gebaut wird immer derselbe Ka-Radio32-Stand (Release 2.4 R1, fester Commit) plus unsere Änderungen aus `firmware/`.

## Plan für `firmware-ng-bt` (Bluetooth-Lautsprecher)

Ziel: Das Handy verbindet sich per Bluetooth mit dem Radio und spielt Musik über den Lautsprecher (A2DP-Sink). Nur der WROVER kann das (klassisches Bluetooth); der ESP32-S3 hat nur Bluetooth Low Energy.

Die Tasten des Radios sind **Rasttasten** (eine bleibt eingerastet, ein Druck auf eine andere löst sie). Die eingerastete Taste bestimmt also immer die Betriebsart:

| Zustand | Ergebnis |
|---|---|
| Taste 6 (GPIO 2) rastet ein | Radio stoppt, Bluetooth geht an, Handy findet das Radio als **„Biennophone“** |
| Taste 1–5 rastet ein (Taste 6 springt heraus) | Bluetooth aus, Radio spielt den gewählten Sender |
| Einschalten mit eingerasteter Taste 6 | startet direkt als Bluetooth-Lautsprecher (kein Radio-Stream) |
| Einschalten mit Taste 1–5 | Radio wie bisher |
| Poti | Lautstärke in beiden Betriebsarten |

- Es gibt nur noch 5 Sendertasten.
- WLAN bleibt im Bluetooth-Betrieb vorerst an (Weboberfläche erreichbar). Gibt es Aussetzer im Bluetooth-Ton, wird es dort abgeschaltet.
- Bluetooth läuft nur, solange Taste 6 eingerastet ist, damit es nicht gleichzeitig mit dem Radio-Streaming um Speicher und Antenne konkurriert.
### Mute über den Ein/Aus-Knopf

Der originale Ein/Aus-Knopf des Biennophone hat Schalter + Poti. Der **Schalter** steuert ein Reed-Relais (hartes Ein/Aus, keine Firmware). Der **Schleifer des Potis** geht an **GPIO 36** (Eintrag `P_MUTE_POT`, Poti-Enden an 3,3 V und GND) und steuert **Mute**:

- Knopf um ca. 5 % **aufdrehen** (Spannung steigt) → Mute aus.
- Knopf um ca. 5 % **zudrehen** (Spannung sinkt) → Mute an.
- Es zählt die **Drehrichtung**, nicht die Stellung. Nach dem Einschalten ist Mute aus.
- Mute wirkt beim Radio und bei Bluetooth. Das Lautstärke-Poti (GPIO 39) bleibt davon getrennt.
- Das Mute-Poti muss nicht angeschlossen sein. GPIO 36 hat aber keinen eingebauten Pull-up: dafür **100 kΩ von GPIO 36 nach 3,3 V** fest einlöten (oder `P_MUTE_POT` = 255).

### Wichtig beim Wechsel zwischen v1 und Bluetooth-Version

Die Bluetooth-Version braucht eine grössere App-Partition (Partitionstabelle geändert, kein Web-Update/OTA mehr). Sender, WLAN und Hardware-Konfiguration bleiben an derselben Stelle. Beim Zurückwechseln auf `firmware-ng-v1` immer **alle** Dateien aus dem Release flashen (`flash.ps1` macht das), damit auch die alte Partitionstabelle zurückkommt.

- Eingerastete Tasten beim Einschalten sind unkritisch: GPIO 2, 5 und 15 stören nur beim Flashen bzw. unterdrücken die Startmeldungen. Zum Flashen sicherheitshalber Taste 1 (GPIO 13) einrasten.

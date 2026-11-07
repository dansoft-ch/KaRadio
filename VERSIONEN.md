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

| Aktion | Ergebnis |
|---|---|
| Taste 6 (GPIO 2) im Radio-Betrieb | Radio stoppt, Bluetooth geht an, Handy kann sich verbinden |
| Taste 1–5 im Bluetooth-Betrieb | Bluetooth aus, Radio spielt den gewählten Sender |
| Poti | Lautstärke in beiden Betriebsarten |
| Einschalten | startet immer als Radio |

Folge: Es gibt nur noch 5 Sendertasten. Bluetooth läuft nur nach Taste 6, damit es nicht gleichzeitig mit dem Radio-Streaming um Speicher und Antenne konkurriert.

Noch offen: Verhalten bei erneutem Druck auf Taste 6, WLAN im Bluetooth-Betrieb an/aus, Bluetooth-Name.

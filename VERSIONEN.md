# Versionen und Branches – Übersicht

Diese Datei erklärt, welche Version des Radios wo liegt und wie man zu einer funktionierenden Version zurückkommt. Stand: 07.11.2026.

## Kurzfassung

| Name | Art | Inhalt | Zustand |
|---|---|---|---|
| `main` | Branch | Lösung mit **zwei Boards**: ESP32-S3 liest Tasten + Poti und schickt Befehle an den WROVER (`TastaturLautstaerkeUART-S3/`) | funktioniert |
| `firmware-ng` | Branch | **Firmware NG**: der WROVER liest 6 Sendertasten und das Poti selbst, kein S3 nötig (`firmware/`) | funktioniert, getestet 06.11.2026 |
| `firmware-ng-v1` | Tag + Release | fester Stand von `firmware-ng` mit fertigen `.bin`-Dateien zum Flashen | **gesicherte Version, nicht verändern** |
| `firmware-ng-bt-v1` | Tag | Firmware NG **plus Bluetooth über Taste 6** (Wechsel per Neustart), `.bin` liegen im Tag unter `firmware/bin/` | **gesicherte Version**, getestet 07.11.2026 |
| `firmware-ng-bt` | Branch | **v2**: Bluetooth **immer an**, automatische Umschaltung Radio ↔ Handy, 6 Sendertasten | funktioniert, getestet 07.11.2026 |

Regel: In `firmware-ng` wird nichts mehr geändert, solange `firmware-ng-bt` in Arbeit ist. Neues kommt nur in `firmware-ng-bt`.

## Eine gesicherte Version wieder flashen

### `firmware-ng-v1` (ohne Bluetooth)

1. Auf GitHub das Release öffnen: https://github.com/dansoft-ch/KaRadio/releases/tag/firmware-ng-v1
2. Alle Dateien herunterladen (`bootloader.bin`, `partitions.bin`, `KaRadio32.bin`, `standard_psram_dan_keys.bin`, `flash.ps1`) und in einen leeren Ordner legen.
3. WROVER direkt per USB an den PC, COM-Port suchen (`Get-PnpDevice -PresentOnly -Class Ports`, neues Board: Seriennummer 5C92044814).
4. Im Ordner: `powershell -ExecutionPolicy Bypass -File .\flash.ps1 -Port COMx`

### `firmware-ng-bt-v1` (Bluetooth über Taste 6)

Die fertigen Dateien liegen im Tag selbst:
```
git switch --detach firmware-ng-bt-v1
powershell -ExecutionPolicy Bypass -File .\firmware\flash.ps1 -Port COMx
git switch firmware-ng-bt
```
Achtung: Die Konfiguration im Tag hat Taste 6 = Bluetooth und das Mute-Poti aus.

Sender, WLAN-Daten und Einstellungen bleiben bei beiden erhalten. Mehr Details: `firmware/README.md`.

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

Den Quellcode einer gesicherten Version ansehen, ohne einen Branch zu verändern:
```
git switch --detach firmware-ng-v1        # oder firmware-ng-bt-v1
```
(zurück mit `git switch firmware-ng-bt`)

Bluetooth ist fertig und gut: `firmware-ng-bt` in `firmware-ng` übernehmen (`git switch firmware-ng`, dann `git merge firmware-ng-bt`), neuen Tag `firmware-ng-v2` setzen und hochladen (`git tag firmware-ng-v2`, `git push origin firmware-ng-v2`). GitHub legt das Release dann automatisch an.

## Wie die Firmware gebaut wird

Auf dem PC ist kein ESP-IDF installiert. GitHub Actions baut (Workflow `.github/workflows/firmware.yml`):

- bei jedem Push mit Änderungen in `firmware/` → Artefakt „karadio32-firmware“ und ein automatischer Commit mit den `.bin`-Dateien in `firmware/bin/` (holen mit `git pull`),
- bei jedem Tag `firmware-ng-v…` → zusätzlich ein **Release** mit denselben Dateien.
- Schlägt der Build fehl, stehen die Fehlerzeilen als Hinweise (Annotations) beim Build, ohne Anmeldung lesbar.

Gebaut wird immer derselbe Ka-Radio32-Stand (Release 2.4 R1, fester Commit) plus unsere Änderungen aus `firmware/`.

## Was `firmware-ng-bt-v1` kann (Bluetooth über Taste 6)

Das Handy verbindet sich per Bluetooth mit dem Radio und spielt Musik über den Lautsprecher (A2DP-Sink, Name **„Biennophone“**, Koppeln ohne PIN). Nur der WROVER kann das (klassisches Bluetooth); der ESP32-S3 hat nur Bluetooth Low Energy.

Die Tasten des Radios sind **Rasttasten**. Die eingerastete Taste bestimmt die Betriebsart:

| Zustand | Ergebnis |
|---|---|
| Einschalten mit eingerasteter Taste 6 (GPIO 2) | Bluetooth-Betrieb: kein Radio-Stream, Handy kann sich verbinden |
| Einschalten mit Taste 1–5 | Radio wie bisher, Bluetooth aus |
| Wechsel Radio ↔ Bluetooth im Betrieb | automatischer Neustart (ca. 20 s bis Ton) |
| Lautstärke-Poti (GPIO 39) | Lautstärke in beiden Betriebsarten |

Warum der Neustart: Bluetooth reserviert fest ca. 56 KB internen Speicher. Ohne diesen Speicher brach der Radio-Stream ab und das Webinterface hing. ESP-IDF 3.3 kann ihn nur einmal und endgültig freigeben; deshalb entscheidet die Taste beim Start.

Weitere Eigenschaften von bt-v1:
- Nur 5 Sendertasten (Taste 6 = Bluetooth).
- WLAN bleibt im Bluetooth-Betrieb an (Weboberfläche erreichbar).
- Mute über den Ein/Aus-Knopf (siehe unten) ist eingebaut, im Tag bt-v1 aber abgeschaltet (`P_MUTE_POT` = 255).
- Start-Wächter: Neustart, wenn KaRadio nicht innerhalb von 90 s „READY“ meldet.
- WLAN: dasselbe Netz bis zu 3-mal versuchen, dann ein Neustart, erst danach AP-Modus „WifiKaRadio“.
- WLAN- und Netzwerkpuffer liegen im PSRAM.

Gemessen am 07.11.2026 (freier interner Speicher):

| Betrieb | frei |
|---|---|
| Radio-Streaming (SRF 1, 128 kbit/s), 5 Minuten | stabil ca. 66 KB |
| Bluetooth-Wiedergabe, 5 Minuten | stabil ca. 20 KB |

Bekannte Grenzen: Wechsel nur per Neustart; Bluetooth-Betrieb hat wenig Speicherreserve.

## Was v2 kann (Branch `firmware-ng-bt`): Bluetooth immer an

- Bluetooth ist immer sichtbar als „Biennophone“, auch während das Radio spielt. Kein Neustart, keine Bluetooth-Taste.
- Handy verbindet sich und spielt → Radio stoppt, nach ca. 1 s kommt der Ton vom Handy.
- Pause am Handy länger als 15 s oder Handy getrennt → der letzte Radiosender spielt wieder.
- Play am Handy → sofort wieder Ton vom Handy.
- Sendertaste während Bluetooth → Handy wird getrennt, der gewählte Sender spielt.
- Taste 6 ist wieder Sender 6.
- Lautstärke-Poti wirkt bei Radio und Bluetooth.

Damit Radio und Bluetooth zusammen in den Speicher passen:
- WLAN- und Netzwerkpuffer im PSRAM, feste WLAN-Puffer reduziert (Empfang 10, Senden 6).
- Die zwei Puffer des AAC-Decoders (20 KB) liegen im PSRAM statt intern.

Gemessen am 07.11.2026 (freier interner Speicher, Test mit Handy):

| Betrieb | frei |
|---|---|
| Radio, Bluetooth im Hintergrund | stabil ca. 27 KB |
| Bluetooth-Ton vom Handy | stabil ca. 32 KB |
| Radio, Handy noch verbunden (nach Pause) | ca. 18 KB |

## Mute über den Ein/Aus-Knopf

Der originale Ein/Aus-Knopf des Biennophone hat Schalter + Poti. Der **Schalter** steuert ein Reed-Relais (hartes Ein/Aus, keine Firmware). Der **Schleifer des Potis** geht an **GPIO 36** (Eintrag `P_MUTE_POT`, Poti-Enden an 3,3 V und GND) und steuert **Mute**:

- Knopf um ca. 10 % **aufdrehen** (Spannung steigt) → Mute aus.
- Knopf um ca. 10 % **zudrehen** (Spannung sinkt) → Mute an.
- Es zählt die **Drehrichtung**, nicht die Stellung. Nach dem Einschalten ist Mute aus.
- Mute wirkt beim Radio und bei Bluetooth. Das Lautstärke-Poti (GPIO 39) bleibt davon getrennt.
- GPIO 36 hat keinen eingebauten Pull-up: **100 kΩ von GPIO 36 nach 3,3 V** fest einlöten. Seit 07.11.2026 ist ein Poti angeschlossen und `P_MUTE_POT` = 36 (getestet). Mit angeschlossenem Poti braucht es den Widerstand nicht; er verhindert nur, dass ein offener Pin von selbst Mute auslöst.

## Wichtig beim Wechsel zwischen v1 und Bluetooth-Versionen

Die Bluetooth-Versionen brauchen eine grössere App-Partition (Partitionstabelle geändert, kein Web-Update/OTA mehr). Sender, WLAN und Hardware-Konfiguration bleiben an derselben Stelle. Beim Zurückwechseln auf `firmware-ng-v1` immer **alle** Dateien aus dem Release flashen (`flash.ps1` macht das), damit auch die alte Partitionstabelle zurückkommt.

Eingerastete Tasten beim Einschalten sind unkritisch: GPIO 2, 5 und 15 stören nur beim Flashen bzw. unterdrücken die Startmeldungen. Zum Flashen sicherheitshalber Taste 1 (GPIO 13) einrasten.

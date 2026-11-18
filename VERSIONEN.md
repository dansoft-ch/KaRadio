# Versionen und Branches – Übersicht

Diese Datei erklärt, welche Version des Radios wo liegt und wie man zu einer funktionierenden Version zurückkommt. Stand: 18.11.2026.

> **Auf dem Radio läuft: `firmware-ng-bt-v6`** (Code aus Commit `7b0921f`, Build `1527c55`, geflasht 18.11.2026 an COM3 mit Konfiguration, getestet: Kurve und Mindestlautstärke passen). Das ist v5 plus [Kurve und Mindestlautstärke des Lautstärke-Potis](#nach-v5-kurve-und-mindestlautstärke-des-lautstärke-potis).
> Version davor auf dem Radio: Tag `firmware-ng-bt-v4`.

## Kurzfassung

| Name | Art | Inhalt | Zustand |
|---|---|---|---|
| `main` | Branch | **aktueller Stand und Weiterentwicklung** (seit 10.11.2026; enthält alles aus `firmware-ng` und `firmware-ng-bt`) | = `firmware-ng-bt-v6` (18.11.2026) |
| `firmware-ng-v1` | Tag + Release | **Firmware NG**: der WROVER liest 6 Sendertasten und das Poti selbst, kein S3 nötig; fertige `.bin`-Dateien zum Flashen | **gesicherte Version, nicht verändern**, getestet 06.11.2026 |
| `firmware-ng-bt-v1` | Tag | Firmware NG **plus Bluetooth über Taste 6** (Wechsel per Neustart), `.bin` liegen im Tag unter `firmware/bin/` | **gesicherte Version**, getestet 07.11.2026 |
| `firmware-ng-bt-v2` | Tag | **v2**: Bluetooth immer an, automatische Umschaltung, 6 Sendertasten, Lautstärke- und Mute-Poti (10 %); `.bin` im Tag unter `firmware/bin/` | **gesicherte Version**, getestet 07.11.2026; bekannter Fehler: Aussetzer ca. 30 s (gefunden 08.11.2026) |
| `firmware-ng-bt-v3` | Tag | **v3** = v2 + Korrektur „Umschalten nur bei hörbarem Ton“; `.bin` im Tag unter `firmware/bin/` | **gesicherte Version**, getestet 09.11.2026 |
| `firmware-ng-bt-v4` | Tag | **v4** = v3 + Taste 3 an GPIO 32 (GPIO 27 defekt) + geglättetes Lautstärke-Poti; `.bin` im Tag unter `firmware/bin/` | **gesicherte Version**, getestet 15.11.2026 im eingebauten Gerät (lief bis 18.11.2026 auf dem Radio) |
| `firmware-ng-bt-v5` | Tag | **v5** = v4 + [Stream-Stopp nach 1 h Mute](#änderungen-in-v5-stream-stopp-nach-1-h-mute); `.bin` im Tag unter `firmware/bin/` | **gesicherte Version**, getestet 16.11.2026 auf dem Testboard (5C92044868), nie allein auf dem Radio |
| `firmware-ng-bt-v6` | Tag | **v6** = v5 + [Kurve und Mindestlautstärke des Lautstärke-Potis](#nach-v5-kurve-und-mindestlautstärke-des-lautstärke-potis) (`P_VOL_CURVE` 16, `P_VOL_MIN` 30); `.bin` im Tag unter `firmware/bin/` | **gesicherte Version, läuft auf dem Radio**, getestet 18.11.2026 im eingebauten Gerät |
| `archiv/promini-ir-uart` | Tag | frühere Lösung: Arduino Pro Mini sendet IR-Codes bzw. UART-Befehle (Dateien heute in `archiv/`) | nicht mehr in Gebrauch |
| `archiv/s3-usbhost` | Tag | frühere Lösung mit **zwei Boards**: ESP32-S3 liest Tasten + Poti und schickt Befehle per USB an den WROVER (Dateien heute in `archiv/`) | funktionierte, nicht mehr in Gebrauch |

Die Branches `firmware-ng` und `firmware-ng-bt` sind am 10.11.2026 in `main` aufgegangen; ihre Stände bleiben über die Tags erreichbar. Neues kommt in `main`, eine getestete Version bekommt einen neuen Tag (z. B. `firmware-ng-bt-v4`).

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
git switch main
```
Achtung: Die Konfiguration im Tag hat Taste 6 = Bluetooth und das Mute-Poti aus.

### `firmware-ng-bt-v2` (Bluetooth immer an)

Genauso, nur mit `git switch --detach firmware-ng-bt-v2`. Konfiguration: 6 Sendertasten, Mute-Poti an GPIO 36. Achtung: hat den [bekannten Fehler](#bekannter-fehler-in-v2-radio-setzt-immer-wieder-ca-30-s-aus).

### `firmware-ng-bt-v3` (Bluetooth immer an, nur bei hörbarem Ton)

Genauso, mit `git switch --detach firmware-ng-bt-v3`. Konfiguration wie v2. Am 09.11.2026 mit `-OhneKonfig` geflasht (nur Firmware, Konfiguration auf dem Radio unverändert). Am heutigen Board **nur mit `-OhneKonfig`** flashen: Die Konfiguration im Tag hat Taste 3 an GPIO 27 (defekt).

### `firmware-ng-bt-v4` (Taste 3 an GPIO 32)

Genauso, mit `git switch --detach firmware-ng-bt-v4`. Konfiguration: Taste 3 an GPIO 32, VS1053-Pins `P_XCS`/`P_XDCS` = 255. Am 15.11.2026 erst mit Konfiguration, dann die Firmware mit `-OhneKonfig` geflasht.

### `firmware-ng-bt-v5` (Stream-Stopp nach 1 h Mute)

Genauso, mit `git switch --detach firmware-ng-bt-v5`. Konfiguration wie v4. Auf das eingebaute Radio genügt `-OhneKonfig`.

### `firmware-ng-bt-v6` (läuft auf dem Radio)

Genauso, mit `git switch --detach firmware-ng-bt-v6`. Konfiguration wie v4 plus `P_VOL_CURVE` = 16 und `P_VOL_MIN` = 30. Am 18.11.2026 **mit Konfiguration** geflasht. Auf ein Board, das schon v6 hatte, genügt `-OhneKonfig`; von v5 oder älter her braucht es die Konfiguration, sonst fehlen die Poti-Werte.

Sender, WLAN-Daten und Einstellungen bleiben bei allen erhalten. Mehr Details: `firmware/README.md`.

## Mit den Branches arbeiten (git)

Welcher Branch ist gerade aktiv?
```
git branch --show-current
```

Es gibt nur noch einen Branch: `main`.

Den Quellcode einer gesicherten Version ansehen, ohne `main` zu verändern (vorher alles committen):
```
git switch --detach firmware-ng-bt-v3     # oder firmware-ng-v1, archiv/s3-usbhost, ...
```
(zurück mit `git switch main`)

Neue getestete Version sichern: Tag setzen und hochladen, z. B. `git tag firmware-ng-bt-v4`, dann `git push origin firmware-ng-bt-v4`. Ein GitHub-Release mit den `.bin`-Dateien entsteht automatisch nur bei Tags `firmware-ng-v…`; bei `firmware-ng-bt-v…` liegen die Dateien im Tag unter `firmware/bin/`.

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

## Was `firmware-ng-bt-v2` kann: Bluetooth immer an

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

### Bekannter Fehler in v2: Radio setzt immer wieder ca. 30 s aus

**Gefunden:** 08.11.2026, im Alltag.

**Ablauf:** Radio gehört → Musik vom Handy über Bluetooth → am Handy auf Pause → nach 15 s spielt wieder das Radio. Danach verstummt das Radio immer wieder für ca. 30 s und kommt dann von selbst zurück.

**Protokoll vom Radio** (COM3, 115200 Baud, Handy noch verbunden, am Handy lief keine Musik):
```
09:15:09 btspeaker: Handy pausiert
09:15:19 btspeaker: Handy spielt (44100 Hz)      ← Radio verstummt, vom Handy kommt nichts Hörbares
09:15:57 btspeaker: Handy pausiert
09:16:12 btspeaker: zurueck zum Radio (intern frei 46420)   ← 15 s nach „pausiert“
09:16:16 ##CLI.PLAYING#                           ← Radio wieder hörbar
```

**Ursache:**
- Mit Pause zurück zum Radio heisst: Das Handy bleibt verbunden. Nur eine Sendertaste trennt es.
- Ein verbundenes Handy startet die Bluetooth-Wiedergabe auch für Benachrichtigungen, Tastentöne oder Apps, die den Ton kurz belegen. Oft schickt es dabei nur Stille.
- v2 schaltet bei jedem „Handy spielt“ sofort aufs Handy und erst 15 s nach „Handy pausiert“ zurück. Dazu kommen ca. 4 s für den Neuaufbau des Radio-Streams.
- Speichermangel ist es nicht: Beim Zurückschalten waren intern 46 KB frei.

**Umgehen mit v2:** Zurück zum Radio immer mit einer Sendertaste (trennt das Handy), oder am Handy „Biennophone“ trennen bzw. Bluetooth ausschalten.

## Änderung nach v2: Umschalten nur bei hörbarem Ton

**Stand:** Tag `firmware-ng-bt-v3`. Code aus Commit `6bc741d` (08.11.2026), Build `0e9ef99`. **Geflasht und getestet am 09.11.2026, läuft auf dem Radio.**

**Warum:** behebt den [bekannten Fehler in v2](#bekannter-fehler-in-v2-radio-setzt-immer-wieder-ca-30-s-aus).

**Was sich ändert:**

| Situation | v2 (läuft) | neu (Branch) |
|---|---|---|
| Handy meldet „spielt“, aber nur Stille oder kurzer Ton | Radio verstummt sofort | Radio spielt weiter |
| Handy spielt hörbar Musik | nach ca. 1 s Ton vom Handy | nach ca. 1,5 s Ton vom Handy (die ersten 1,5 s fehlen) |
| Pause am Handy | nach 15 s zurück zum Radio | Radio bleibt still, Bluetooth-Betrieb bleibt |
| Play am Handy nach Pause | sofort Ton vom Handy | sofort Ton vom Handy |
| Handy trennt sich | zurück zum Radio | zurück zum Radio (unverändert) |
| Sendertaste | trennt Handy, Sender spielt | trennt Handy, Sender spielt (unverändert) |

Entschieden nach Diskussion am 08.11.2026. Verworfen wurde „verbunden = Handy, getrennt = Radio“: Handys verbinden sich oft von selbst, sobald sie in Reichweite sind, und das Radio wäre dann stumm, ohne dass jemand Musik abspielt.

**Wie es funktioniert** (`firmware/btspeaker.c`):
- Solange das Radio spielt, prüft `checkAudible()` die Samples vom Handy. Hörbar heisst: Spitzenwert über `AUDIBLE_LEVEL` (300 von 32767, ca. −40 dB).
- Nach `AUDIBLE_MS` (1500 ms) hörbarem Ton schaltet `btSpeakerPoll()` um und gibt `btspeaker: hoerbarer Ton vom Handy` aus.
- `SILENCE_MS` (1000 ms) Stille setzt die Zählung zurück, damit mehrere kurze Töne nicht zusammengezählt werden.
- Der 15-s-Rücksprung bei Pause (`BT_PAUSE_MS`) ist entfernt.

**Bekannte Grenze:** Ein langer Ton vom Handy (z. B. ein Klingelton über Bluetooth) schaltet weiterhin um. Zurück dann mit einer Sendertaste.

**Test am 09.11.2026** (am Gerät):

| Schritt | Ergebnis |
|---|---|
| Handy seit längerem verbunden, am Handy nichts abgespielt | Radio läuft ohne Aussetzer ✔ |
| Video am Handy gestartet | Ton kommt über das Radio ✔ |
| Video zu Ende | Radio bleibt still (wie gewollt) ✔ |
| Video erneut gestartet | Ton ok ✔ |
| Taste 2 | Handy getrennt, Radio spielt wieder ✔ |
| Bluetooth am Handy ausschalten | nicht getestet |

**Falls es Probleme gibt:** v2 wieder flashen (siehe [oben](#firmware-ng-bt-v2-bluetooth-immer-an)); dann gilt wieder der bekannte Fehler von v2.

## Änderungen in v4: Taste 3 an GPIO 32, ruhiges Lautstärke-Poti

**Stand:** Tag `firmware-ng-bt-v4`. Code aus Commit `c5bbc33`, Build `6acdb1e`. **Geflasht und getestet am 15.11.2026 im fertig eingebauten Gerät, läuft auf dem Radio.**

**Taste 3 an GPIO 32:** Nach dem Einbau löste Taste 3 nie aus. GPIO 27 liegt auch ohne Draht dauernd bei ca. 1,2 V (statt 3,3 V mit internem Pull-up), vermutlich beim Löten einmal kurzgeschlossen. Taste 3 ist deshalb auf **GPIO 32** umgelötet (interner Pull-up, kein Widerstand nötig). Damit 32 frei ist, stehen `P_XCS` und `P_XDCS` auf 255: Ka-Radio32 (`VS1053_HW_init`) bricht bei `P_XCS` = 255 sofort ab und fasst keinen VS1053-Pin an. Einen VS1053 gibt es nicht.

**Lautstärke-Poti:** Im Gehäuse sprang die Lautstärke ohne Drehen dauernd zwischen 71 und 74 (ca. jede Sekunde eine Meldung). Neu in `firmware/stationkeys.c`: gleitender Mittelwert über die Zeit (jeder neue Wert zählt 1/4, Takt 80 ms) und Mindeständerung `VOL_HYST` 4 statt 2.

**WLAN:** Beim ersten Test fand das Radio kein WLAN, weil als SSID 1 die IP-Adresse eingetragen war. Mit richtigem Namen: Der erste Versuch beim Kaltstart scheitert wie bekannt, der zweite klappt (Signal −63 dBm), `READY` nach ca. 21 s. Die Meldung `invalid static ip` beim Verbinden ist harmlos (feste IP wird erst kurz danach gesetzt).

**Test am 15.11.2026** (am Gerät):

| Schritt | Ergebnis |
|---|---|
| Bluetooth-Ton vom Handy | ✔ |
| WLAN verbinden, Autostart Radio | ✔ |
| Tasten 1–6 (Taste 3 an GPIO 32) | ✔ |
| Lautstärke-Poti 80 s nicht berührt | keine einzige Lautstärke-Meldung ✔ |
| Lautstärke-Poti drehen | Lautstärke folgt ✔ |

## Änderungen in v5: Stream-Stopp nach 1 h Mute

**Stand:** Tag `firmware-ng-bt-v5`. Code aus Commit `2473610`. **Am 16.11.2026 auf dem Testboard getestet** (altes Board 5C92044868 an COM4, Build `f014d90`). Die `.bin` im Tag stammt aus Build `41700c7`: gleicher Quellcode, nur Build-Zeit und Prüfsumme anders. Auf dem Radio läuft noch v4.

**Warum:** Mute setzt nur den Ton auf 0, der Stream läuft weiter (~1,4 GB pro Tag).

**Was es tut:** Nach 1 h Mute stoppt die Firmware den Stream (`MUTE_STOP_MS` in `firmware/stationkeys.c`). Mute aus spielt den Sender wieder. WLAN und Bluetooth bleiben an. Startet jemand das Radio während des Mutes (Sendertaste, Weboberfläche, Handy getrennt), zählt die Stunde neu.

**Entschieden am 16.11.2026:** Das Lautstärke-Poti hebt Mute **nicht** auf. Die Stellung des Ein/Aus-Knopfs soll zeigen, ob das Radio stumm ist; ausserdem liesse sich Mute am Anschlag des Knopfs nicht wieder einschalten. Erst im Alltag beobachten.

**Test am 16.11.2026** (Testboard, neo 1, serielles Protokoll):

| Zeit | Ereignis |
|---|---|
| 09:49:19 | neo 1 spielt |
| 09:49:59 | `stationkeys: Mute an`, Stream läuft weiter |
| 10:49:59 | `stationkeys: 1 h Mute -> Stream gestoppt` ✔ |
| 10:52:47 | `stationkeys: Mute aus`, `Stream wieder an` |
| 10:52:49 | `##CLI.PLAYING#`, Musik wieder da (2 s) ✔ |

## Nach v5: Kurve und Mindestlautstärke des Lautstärke-Potis

**Stand:** Tag `firmware-ng-bt-v6`. Code aus Commit `a491a3c` (17.11.2026), geflasht als Build `1527c55` (Code `7b0921f`, gleiche `.bin` wie im Tag). **Am 18.11.2026 im eingebauten Radio getestet** (Board 5C92044814 an COM3, mit Konfiguration geflasht, damit `P_VOL_CURVE` und `P_VOL_MIN` auf dem Radio stehen). Vorher am selben Tag auf dem Testboard (COM5) geflasht, Start ok.

Am alten 1,4-MΩ-Poti war die Lautstärke schlecht über den Drehweg verteilt. Ein Widerstand 220 kΩ vom Schleifer nach Plus hätte gepasst, dabei sprang der Ton aber oder fiel aus (vermutlich Kontaktaussetzer am alten Schleifer: der Widerstand zieht den Pin dann sofort auf 3,3 V). Die Firmware rechnet den Widerstand jetzt nach (`P_VOL_CURVE` = 16, d. h. 16 % des Poti-Werts) und setzt am unteren Anschlag nicht 0, sondern `P_VOL_MIN` = 30 (leise hörbar; ganz aus = Mute-Poti). Formel und Werte: `firmware/README.md`. Fehlen die Einträge in der Konfiguration, verhält sich das Poti wie in v5.

**Test am 18.11.2026** (am Gerät):

| Schritt | Ergebnis |
|---|---|
| Start: Tasten, Potis, `Poti-Kurve 16 %, Mindestlautstaerke 30` im Protokoll | ✔ |
| WLAN, Autostart Radio (Sender 3) | ✔ |
| Lautstärke über den ganzen Drehweg | gleichmässig verteilt, „Kurve ist super“ ✔ |
| Unterer Anschlag (`P_VOL_MIN` = 30) | leise hörbar, passt perfekt ✔ |

## Senderliste mit kurzen Startzeiten

Seit 16.11.2026 `sender/WebStations_Daniel_schnell.txt` (Laden im Webinterface über **Load**). SRF ohne `.m3u`-Playlist, neo 1 und Rockantenne über HTTP statt HTTPS. Startzeit aller 6 Sender 1,2–2,8 s statt bis 5,7 s. Details und Messwerte: `sender/README.md`.

## Mute über den Ein/Aus-Knopf

Der originale Ein/Aus-Knopf des Biennophone hat Schalter + Poti. Der **Schalter** steuert ein Reed-Relais (hartes Ein/Aus, keine Firmware). Der **Schleifer des Potis** geht an **GPIO 36** (Eintrag `P_MUTE_POT`, Poti-Enden an 3,3 V und GND) und steuert **Mute**:

- Knopf um ca. 10 % **aufdrehen** (Spannung steigt) → Mute aus.
- Knopf um ca. 10 % **zudrehen** (Spannung sinkt) → Mute an.
- Es zählt die **Drehrichtung**, nicht die Stellung. Nach dem Einschalten ist Mute aus.
- Mute wirkt beim Radio und bei Bluetooth. Das Lautstärke-Poti (GPIO 39) bleibt davon getrennt und hebt Mute nicht auf.
- Nach 1 h Mute stoppt der Stream, Mute aus spielt den Sender wieder (ab v5).
- GPIO 36 hat keinen eingebauten Pull-up: **100 kΩ von GPIO 36 nach 3,3 V** fest einlöten. Seit 07.11.2026 ist ein Poti angeschlossen und `P_MUTE_POT` = 36 (getestet). Mit angeschlossenem Poti braucht es den Widerstand nicht; er verhindert nur, dass ein offener Pin von selbst Mute auslöst.

## Wichtig beim Wechsel zwischen v1 und Bluetooth-Versionen

Die Bluetooth-Versionen brauchen eine grössere App-Partition (Partitionstabelle geändert, kein Web-Update/OTA mehr). Sender, WLAN und Hardware-Konfiguration bleiben an derselben Stelle. Beim Zurückwechseln auf `firmware-ng-v1` immer **alle** Dateien aus dem Release flashen (`flash.ps1` macht das), damit auch die alte Partitionstabelle zurückkommt.

Eingerastete Tasten beim Einschalten sind unkritisch: GPIO 2, 5 und 15 stören nur beim Flashen bzw. unterdrücken die Startmeldungen. Zum Flashen sicherheitshalber Taste 1 (GPIO 13) einrasten.

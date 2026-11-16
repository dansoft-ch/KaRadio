# KaRadio-Webinterface: was die App benutzt

KaRadio32 nimmt Befehle als einfache GET-Anfragen an (Quelle: `main/webserver.c` und `main/interface.c` von Ka-Radio32). `N` ist die Sendernummer ab 0, Taste 1 = Sender 0.

| Zweck | Anfrage | Antwort |
|---|---|---|
| Sender spielen | `http://RADIO/?play=N` | `OK`-Antwort ohne Inhalt |
| Lautstärke setzen | `http://RADIO/?volume=0..254` | dito |
| Lautstärke ± 5 | `/?volume+` bzw. `/?volume-` | dito |
| Stoppen / Starten | `/?stop` bzw. `/?start` | dito |
| Zustand lesen | `/?infos` | Text, je Zeile `vol: 120`, `num: 2`, `stn: SRF 3`, `tit: <Titel>`, `sts: 1` (1 = spielt) |
| Sendername lesen | `/?list=N` | Name des Senders N |
| Version | `/?version` | `Release: …, Revision: …` |

Zusätzlich gibt es einen WebSocket (für Live-Meldungen im Original-Webinterface); für die App reicht Abfragen alle 2–3 s mit `?infos`.

## Einschränkung 1: kein Mute-Befehl

Mute ist in unserer Firmware nur am Mute-Poti (`renderer_set_mute`, `stationkeys.c`). Übers Web gibt es keinen Befehl dafür. Zwei Wege:

1. **Ohne Firmware-Änderung (im Prototyp):** Die App merkt sich die Lautstärke, setzt `volume=0` und stellt sie beim Aufheben wieder her. Nachteil: Dreht jemand am Lautstärke-Knopf, ist das „Stumm“ der App weg; das Radio-Mute (Poti) und das App-Mute wissen nichts voneinander.
2. **Mit kleiner Firmware-Ergänzung:** neuer Befehl `?mute=1` / `?mute=0`, der `renderer_set_mute` aufruft, und `mute: 0/1` in der `?infos`-Antwort. Dann zeigen App und Gerät denselben Zustand. Ein paar Zeilen in `karadio.patch`, braucht aber einen neuen Firmware-Build und Flashen.

## Einschränkung 2: CORS (nur im Browser)

KaRadio schickt keine `Access-Control-Allow-Origin`-Kopfzeile. Eine Webseite, die nicht vom Radio selbst kommt, darf deshalb die **Antworten** nicht lesen (Befehle kommen trotzdem an). Folgen:

- Im Prototyp im Browser: Tasten, Lautstärke und Stumm wirken, aber Sendername/Titel/Lautstärke vom Radio werden nicht angezeigt.
- In der fertigen App (Capacitor mit nativem HTTP, siehe `PLANUNG.md`) gilt diese Sperre nicht.
- Alternative: eine Zeile in `karadio.patch` (`Access-Control-Allow-Origin: *` in `strsROK`), dann könnte auch eine reine Webseite alles lesen.

## Einschränkung 3: Bluetooth-Betrieb

Spielt das Handy per Bluetooth, ist der Radio-Stream gestoppt. `?play=N` schaltet zurück aufs Radio wie eine Sendertaste. Ob `?infos` dann „Bluetooth“ erkennen lässt, ist noch offen (sts = 0); eine Anzeige „Bluetooth aktiv“ bräuchte ebenfalls eine kleine Firmware-Ergänzung.

## Adresse des Radios

Standard in der App: **`http://karadio32.local`**. KaRadio32 meldet sich per mDNS unter seinem Hostnamen (Standard `karadio32`). Am Handy noch zu prüfen: Android löst `.local`-Namen erst ab neueren Versionen zuverlässig auf, iPhones können es.

Klappt der Name nicht, die IP-Adresse eintragen. Das Radio holt seine IP per DHCP, deshalb im Router eine **feste IP** für das Radio reservieren.

## Erreichbarkeit (Kontrolllampe)

Die App fragt alle 3 s `?infos` ab (Zeitlimit 3 s). Antwortet das Radio, leuchtet die Lampe. Kommt keine Antwort (Radio aus, falsches WLAN, falsche Adresse), geht sie aus und die Bedienelemente werden gedimmt. Im Browser klappt das trotz CORS: Eine Anfrage im Modus `no-cors` zeigt, ob das Radio antwortet, auch wenn der Inhalt nicht lesbar ist.

# Biennophone-App (Handy-Fernbedienung)

Eigenständiges Teilprojekt: eine Handy-App, die das Biennophone-Radio über das **KaRadio-Webinterface** im WLAN steuert. Sie ist bewusst vom Rest des Projekts (Firmware, Factsheet, `VERSIONEN.md`) getrennt; alles zur App liegt in diesem Ordner.

## Was die App kann (Ziel)

- **6 Sendertasten** in einer Reihe im schwarzen Rahmen wie am Gerät. Die gedrückte Taste bleibt „eingerastet“, die Ziffer darüber wird rostrot hinterlegt. Die Sendernamen stehen klein über den Ziffern.
- **Schild** mit „BIENNOPHONE“, Sendername und aktuellem Titel.
- **Lautstärke** als Chrom-Drehknopf mit rostrotem Bogen (drehen mit dem Finger, 0–100 %, am Radio 0–254).
- **Ein/Aus-Knopf = Stumm**, wie am Gerät, wo das Poti dieses Knopfs stumm schaltet. Antippen schaltet um.
- **Orange Kontrolllampe** auf goldener Unterlagscheibe, oben links im Stoff. Sie leuchtet, wenn das Radio antwortet, und ist aus, wenn es ausgeschaltet oder nicht erreichbar ist. Beim Suchen glimmt sie.
- **Menü** (Chromknopf oben rechts) für die Adresse des Radios, Standard `http://karadio32.local`, mit „Verbindung testen“.

## Design

Vorbild ist das Biennophone **„Crans“**, von oben nach unten wie am Gerät:

- **Holzrahmen** aus geflammtem hellem Ahorn rundherum, unten breiter als Abschluss.
- **Webstoff** grau-weiss mit rostroten Fäden und hellen Zickzack-Fäden, darin oben links die orange Kontrolllampe auf goldener Unterlagscheibe.
- **Messingplatte** (gebürstet) mit rostroter Beschriftung: links „Lautstärke · Puissance“ über dem Lautstärke-Knopf mit rotem Bogen, in der Mitte „— • BIENNOPHONE • —“, rechts „Aus ⌒ Ein / Hors En“ mit Notenschlüssel über dem Ein/Aus-Knopf. Darunter Sendernamen und die Ziffern 1–6.
- **Knöpfe** wie am Original: gerändelter Silberkranz auf schwarzem Sockel, polierter Chromring mit goldenem Rand, goldene gewölbte Mitte mit dunklem Spiegelkeil. Die rote Kerbe am Kranz zeigt die Stellung.
- **Holzpaneel** mit den 6 weissen Tasten im schwarzen Rahmen und dem Schriftzug „Crans“. Alles ist mit CSS/SVG nachgezeichnet, das Foto (© biennophone.ch) ist nicht eingebunden.

Schriften (Google Fonts, frei): **Jost** für das Schild (geometrische Grotesk wie die Futura-artige Schrift am Gerät) und **Damion** für den Schriftzug „Crans“. In der fertigen App werden sie mitgeliefert, damit sie auch ohne Internet stimmen.

## Ordner

| Pfad | Inhalt |
|---|---|
| `README.md` | diese Übersicht |
| `PLANUNG.md` | Technikwahl, Aufwand, Schritte bis zur fertigen App |
| `SCHNITTSTELLE.md` | Befehle des KaRadio-Webinterfaces, Einschränkungen (CORS, Mute) |
| `beispiel/index.html` | klickbarer Prototyp (eine Datei, läuft im Browser) |

## Beispiel ansehen

`beispiel/index.html` im Browser öffnen (am PC per Doppelklick; am Handy z. B. die Datei aufs Handy kopieren). Ohne Adresse läuft es im **Demo-Betrieb** (orange Lampe). Mit eingetragener Adresse schickt es die Befehle wirklich ans Radio. Den Zustand des Radios zurücklesen kann erst die fertige App (Grund: siehe `SCHNITTSTELLE.md`, Abschnitt CORS).

## Stand

16.11.2026: Prototyp und Planung, noch keine installierbare App.

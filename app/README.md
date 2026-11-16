# Biennophone-App (Handy-Fernbedienung)

Eigenständiges Teilprojekt: eine Handy-App, die das Biennophone-Radio über das **KaRadio-Webinterface** im WLAN steuert. Sie ist bewusst vom Rest des Projekts (Firmware, Factsheet, `VERSIONEN.md`) getrennt; alles zur App liegt in diesem Ordner.

## Was die App kann

- **6 Sendertasten** in einer Reihe im schwarzen Rahmen wie am Gerät. Die gedrückte Taste bleibt „eingerastet“, die Ziffer darüber wird rostrot hinterlegt. Die Sendernamen stehen klein über den Ziffern.
- **Schild** mit „BIENNOPHONE“, Sendername und aktuellem Titel.
- **Lautstärke** als Drehknopf (drehen mit dem Finger, 0–100 %, am Radio 0–254). Links davon der rote Keil wie am Gerät, er füllt sich mit der Lautstärke.
- **Ein/Aus-Knopf = Stumm**, wie am Gerät, wo das Poti dieses Knopfs stumm schaltet. Antippen schaltet um.
- **Orange Kontrolllampe** auf goldener Unterlagscheibe, oben links im Stoff. Sie leuchtet, wenn das Radio antwortet, und ist aus, wenn es ausgeschaltet oder nicht erreichbar ist. Beim Suchen glimmt sie.
- **Menü** (flache Goldscheibe oben rechts) für die Adresse des Radios, Standard `http://karadio32.local`, mit „Verbindung testen“.

## Design

Vorbild ist das Biennophone **„Crans“**, von oben nach unten wie am Gerät:

- **Holzrahmen** aus geflammtem hellem Ahorn rundherum, unten breiter als Abschluss.
- **Webstoff** grau-weiss mit rostroten Fäden und hellen Zickzack-Fäden, darin oben links die orange Kontrolllampe: zylindrisches Glas mit flacher Stirn auf flacher goldener Unterlagscheibe.
- **Messingplatte** (gebürstet) mit rostroter Beschriftung: links „Lautstärke · Puissance“ über dem Lautstärke-Knopf mit rotem Keil, in der Mitte „— • BIENNOPHONE • —“, rechts „Aus ⌒ Ein / Hors En“ mit Notenschlüssel über dem Ein/Aus-Knopf. Darunter Sendernamen und die Ziffern 1–6.
- **Knöpfe** wie am Original: gerändelter Silberkranz auf schwarzem Sockel, polierter Chromring mit goldenem Rand, goldene gewölbte Mitte mit dunklem Spiegelkeil. Die rote Kerbe am Kranz zeigt die Stellung.
- **Holzpaneel** mit den 6 weissen Tasten im schwarzen Rahmen und dem Schriftzug „Crans“. Alles ist mit CSS/SVG nachgezeichnet, das Foto (© biennophone.ch) ist nicht eingebunden.

Schriften (Google Fonts, frei): **Jost** für das Schild (geometrische Grotesk wie die Futura-artige Schrift am Gerät) und **Damion** für den Schriftzug „Crans“. In der fertigen App werden sie mitgeliefert, damit sie auch ohne Internet stimmen.

## Ordner

| Pfad | Inhalt |
|---|---|
| `README.md` | diese Übersicht |
| `PLANUNG.md` | Technikwahl, Aufwand, Schritte |
| `SCHNITTSTELLE.md` | Befehle des KaRadio-Webinterfaces, Einschränkungen (CORS, Mute) |
| `www/index.html` | die App selbst (eine Datei, läuft auch im Browser) |
| `icon.svg` | App-Symbol (Holz, Goldscheibe, orange Lampe) |
| `package.json`, `capacitor.config.json` | Capacitor-Projekt (verpackt `www/` als Android-App) |
| `bin/Biennophone.apk` | fertige App, legt GitHub Actions nach jedem Build hinein |
| `signing/` | Signierschlüssel der App, beim ersten Build erzeugt |

Der Build läuft in GitHub Actions (`.github/workflows/app.yml`), auf dem PC muss nichts installiert sein. Er startet bei jeder Änderung in `app/` (ausser `.md`-Dateien) oder von Hand unter *Actions → App → Run workflow*. Der Ordner `android/` wird dabei jedes Mal neu erzeugt und ist nicht im Repo.

## App aufs Handy bringen (Android)

1. Nach dem Build `git pull`, oder direkt am Handy im Browser öffnen:
   `https://github.com/dansoft-ch/KaRadio/raw/main/app/bin/Biennophone.apk`
2. Die Datei öffnen. Android fragt einmal, ob der Browser bzw. Dateimanager Apps installieren darf → erlauben.
3. „Biennophone“ starten. Standardadresse ist `http://karadio32.local`. Bleibt die Lampe aus, im Menü die IP-Adresse des Radios eintragen.

Neue Versionen installieren sich über die alte (gleicher Schlüssel, höhere Versionsnummer = Build-Nummer).

**Zum Schlüssel:** Er liegt im öffentlichen Repo. Wer ihn hat, könnte eine fremde App als „Update“ von Biennophone signieren. Für eine selbst installierte Bastel-App ist das vertretbar; für den Play Store bräuchte es einen geheimen Schlüssel (GitHub-Secret).

## Im Browser ansehen

`www/index.html` am PC per Doppelklick öffnen. Ohne Adresse läuft sie im **Demo-Betrieb**. Mit Adresse schickt sie die Befehle wirklich ans Radio, kann im Browser aber den Zustand nicht zurücklesen (CORS, siehe `SCHNITTSTELLE.md`). In der Android-App gilt diese Sperre nicht (natives HTTP).

## Stand

- 16.11.2026: Prototyp und Planung.
- 16.11.2026: Capacitor-Projekt und Build per GitHub Actions; erste APK, am Handy noch nicht getestet.

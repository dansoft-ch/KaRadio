# Planung und Aufwand

## Technikwahl

| Variante | Wie | Vorteil | Nachteil |
|---|---|---|---|
| **A – Web-App + Capacitor (Empfehlung)** | Oberfläche als HTML/CSS/JS (wie `beispiel/index.html`), mit Capacitor als Android-App verpackt; HTTP über das native Capacitor-HTTP-Plugin | Prototyp wird direkt zur App, kein CORS-Problem, später auch iPhone möglich | Android-Build braucht Android SDK → wie die Firmware per **GitHub Actions** bauen, `.apk` aus dem Repo laden |
| B – Native Android (Kotlin/Compose) | eigene App in Android Studio | „echte“ Android-App, Widgets einfacher | mehr Code, Design muss neu gebaut werden, nur Android |
| C – Webseite vom Radio selbst | `app.html` in die KaRadio-Firmware, am Handy „Zum Startbildschirm“ | kein App-Build, kein CORS | Firmware-Änderung, Platz im Flash, Adresse nicht frei wählbar (die Seite *ist* das Radio) |
| D – Arduino/MIT App Inventor | Baukasten-App | ohne Programmieren änderbar | Design wie das Biennophone kaum machbar |

Empfehlung: **A**, zuerst nur Android (APK direkt installieren, kein Play Store nötig). iPhone geht mit derselben Oberfläche, braucht aber einen Mac/Apple-Entwicklerkonto zum Installieren.

## Aufwand (für mich, Claude)

| Schritt | Inhalt | Aufwand |
|---|---|---|
| 1. Prototyp | Design, Tasten, Lautstärke, Stumm, Menü mit Adresse | **erledigt** (heute `www/`) |
| 2. Design nach dem Biennophone „Crans“ | Holz, Webstoff, Schild-Schrift, Tasten, Chromknöpfe, Kontrolllampe, Standardadresse `karadio32.local` | **erledigt** (16.11.2026) |
| 3. Capacitor-Projekt | `package.json`, Capacitor-Konfiguration, natives HTTP, App-Icon, Name „Biennophone“ | **erledigt** (16.11.2026) |
| 4. Build per GitHub Actions | Workflow `app.yml` baut die APK (Node + Android SDK im Runner), legt sie unter `app/bin/` ab bzw. als Release | **eingerichtet** (16.11.2026), erster Lauf noch offen |
| 5. Test am Handy | APK installieren, mit dem Radio testen, Fehler beheben | 1 Sitzung zusammen mit dir |
| optional | Firmware: `?mute=`, Bluetooth-Anzeige in `?infos`, CORS-Kopfzeile | je 1 kurze Sitzung + Flashen |
| optional | Radio im WLAN suchen statt IP eintippen, Home-Screen-Widget | je 1 Sitzung |

Insgesamt etwa **3–5 Arbeitssitzungen** bis zur installierbaren Android-App. Der grösste Unsicherheitsfaktor ist der Android-Build in GitHub Actions (Erfahrung von der Firmware: ein, zwei Fehlerrunden). Auf dem PC muss nichts installiert werden, nur am Handy „Installation aus unbekannten Quellen“ erlauben.

## Offene Fragen

- Android oder iPhone? → **Android** (Entscheid 16.11.2026)
- Mute: App-seitig (Lautstärke 0) reicht, oder Firmware-Befehl `?mute=` (dann stimmen App und Mute-Poti überein)?
- Funktioniert `karadio32.local` am eigenen Handy?

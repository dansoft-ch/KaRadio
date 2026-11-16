# Senderliste

`WebStations_Daniel_schnell.txt` ist die aktuelle Senderliste (16.11.2026). Laden im Webinterface des Radios über **Load**. Sender 1–6 liegen auf den Tasten 1–6.

| Taste | Sender | URL | Datei | Port |
|---|---|---|---|---|
| 1 | SRF 1 | `stream.srg-ssr.ch` | `/srgssr/srf1/aac/96` | 80 |
| 2 | neo 1 | `neo1.ice.infomaniak.ch` | `/neo1.mp3` | 80 |
| 3 | SRF 3 | `stream.srg-ssr.ch` | `/srgssr/srf3/aac/96` | 80 |
| 4 | GRIFF | `grrif.ice.infomaniak.ch` | `/grrif-64.aac` | 80 |
| 5 | Rockantenne | `stream.rockantenne.de` | `/rockantenne/stream/mp3` | 80 |
| 6 | 1.FM Blues | `strm112.1.fm` | `/blues_mobile_mp3` | 80 |

## Warum diese URLs

Die alte Liste brauchte bei vier Sendern 3–6 s bis zur Musik, GRIFF und 1.FM Blues nur gut 1 s. Gründe:

- **HTTPS** kostet auf dem ESP32 ca. 1,5 s pro Verbindung (TLS-Handshake). neo 1 und Rockantenne laufen deshalb über HTTP, Port 80.
- **Playlist (`.m3u`)**: SRF war als `/drs1/aacp_96.m3u` eingetragen. Das Radio lud erst die Playlist und dann den Stream. Jetzt steht die Stream-Adresse direkt drin.
- **Weiterleitungen**: SRF und Rockantenne leiten noch auf einen ihrer Server weiter (`livestreaming-node-…`, `s2/s5-webradio…`). Diese Server wechseln, deshalb sind sie nicht fest eingetragen. Kostet ca. 1 s.

Gemessen am Testboard (5C92044868, Firmware `2473610`), Zeit von `cli.play` bis `##CLI.PLAYING#`, Mittel aus 3 Läufen:

| Taste | Sender | alte Liste | neue Liste |
|---|---|---|---|
| 1 | SRF 1 | 3,66 s | 2,35 s |
| 2 | neo 1 | 3,10 s | 1,50 s |
| 3 | SRF 3 | 3,65 s | 2,54 s |
| 4 | GRIFF | 1,19 s | 1,21 s |
| 5 | Rockantenne | 5,69 s | 2,76 s |
| 6 | 1.FM Blues | 1,27 s | 1,26 s |

Neue Sender möglichst direkt (keine `.m3u`/`.pls`) und über HTTP eintragen. Prüfen am PC: `curl -I -L <url>` zeigt Weiterleitungen.

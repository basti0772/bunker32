# Bunker 32

Ein eigenständiger Retro-Ego-Shooter für den angeschlossenen ESP32 mit ILI9341 und resistivem Touch. Eigene Pixelgrafik, texturierte Wände, neun Wachen, Munition, Verbände, Karte und ein goldener Schlüssel. Finde den Schlüssel im nordöstlichen Raum und erreiche den grünen Ausgang im Südosten. Das Spiel läuft vollständig auf dem ESP32, auch ohne PC.

<img src="docs/screenshots/gameplay.png" alt="Bunker 32: tatsächlicher Screenshot vom ESP32 mit texturierten Wänden, Wachen, Pistole und Touch-Tasten" width="640">

*Direkt aus dem Bildpuffer des ESP32 aufgenommen. Interne Auflösung: 160 × 120 Pixel; das Display zeigt das Bild mit zweifacher Skalierung.*

Weitere Ansichten: [Startbild](docs/screenshots/title.png) · [Spiel mit Karte](docs/screenshots/map.png).

## Spielen

**Am Gerät:** Startbild antippen. Die fünf Tasten unten sind Zurück, Links drehen, Vorwärts, Rechts drehen und Feuer. Im 3D-Bild kann man oben/unten zum Gehen und links/rechts zum Drehen halten; die diagonalen Bereiche kombinieren beides. Auto-Feuer ist anfangs eingeschaltet: Ziele durch Drehen auf eine Wache. Oben links öffnet sich die Pause, oben rechts die Karte. In der Pause lässt sich links Auto-Feuer umschalten oder rechts Touch kalibrieren. Unten tippen setzt das Spiel fort.

**Am PC:** `Spielen.cmd` doppelklicken. Das Programm verbindet sich mit COM3; ein anderer Port ist im Fenster wählbar. Das Fenster muss für Tastatureingaben den Fokus haben. USB bleibt angeschlossen, eine WLAN-Einrichtung ist nicht nötig.

| Taste | Aktion |
|---|---|
| Enter | Start / weiter / nach Spielende neu |
| W / S oder ↑ / ↓ | Vor / zurück |
| A / D oder ← / → | Links / rechts drehen |
| Q / E | Seitwärts gehen |
| Leertaste | Feuern |
| F | Auto-Feuer umschalten |
| M | Karte |
| P / Esc | Pause / weiter |
| R | Neue Runde |

Die PC-Vorschau zeigt rund zwei Bilder pro Sekunde. Tastatureingaben werden alle 33 ms übertragen; die Anzeige direkt am ESP32 läuft unabhängig davon. Nach Fokusverlust werden Tasten freigegeben. Der ESP32 verwirft PC-Bewegung nach 250 ms ohne Aktualisierung. Nach USB-Trennung bleibt Touch nutzbar; die PC-App versucht die Verbindung erneut.

Eine vorhandene `chess-touch/rot3-v1`-Kalibrierung wird für die aktuelle Displaydrehung übernommen. Eine neue Vierpunkt-Kalibrierung wird separat in `bunker32/rot1-v1` gespeichert. Ohne gespeicherte Kalibrierung startet der erste Touch die Kalibrierung. PC-Steuerung funktioniert auch ohne Touch-Kalibrierung.

## Hardware

Ermittelt: ESP32-D0WDQ6, 240 MHz, 4 MB Flash, CP210x an COM3. ILI9341 im Querformat 320 × 240; intern 160 × 120 Pixel mit zweifacher Skalierung. Kein PSRAM nötig.

| Anschluss | GPIO |
|---|---:|
| SPI MISO / MOSI / SCLK | 19 / 23 / 18 |
| TFT CS / DC / RST | 15 / 2 / 4 |
| Touch CS | 32 |

## Bauen und flashen

Getestete Voraussetzungen: Arduino CLI, Espressif ESP32 Core 3.3.12, TFT_eSPI 2.5.43; für die PC-App Windows PowerShell 5.1 und .NET Framework. Die Build-Skripte sind für Windows ausgelegt. Die PC-EXE wird lokal aus dem mitgelieferten C#-Quellcode erstellt; `Spielen.cmd` erledigt das beim ersten Start automatisch.

```powershell
powershell -ExecutionPolicy Bypass -File .\Build-And-Flash.ps1 -BuildOnly
powershell -ExecutionPolicy Bypass -File .\Build-And-Flash.ps1 -Port COM3
powershell -ExecutionPolicy Bypass -File .\pc\Build-Controller.ps1
powershell -ExecutionPolicy Bypass -File .\tests\Test-Device.ps1
```

Das PC-Steuerfenster vor Flashen oder Gerätetests schließen, damit der COM-Port frei ist. `DisplayConfig.h` wird beim Build auch für TFT_eSPI eingebunden; die globale Bibliothekskonfiguration wird nicht verändert. Flashen ersetzt die bestehende Anwendung. Sicherungen liegen im ignorierten Ordner `backups`, Build-Dateien in `build`, PC-EXE und Testbilder in `artifacts`.

`powershell -ExecutionPolicy Bypass -File .\Restore-Previous.ps1` kann auf dem ursprünglichen Entwicklungs-PC den vorher gesicherten kompletten Flash wiederherstellen. Dafür werden die lokale Datei `backups/before-bunker32-safe.bin` und ihre `.sha256`-Prüfsumme benötigt. Diese gerätespezifische Sicherung ist nicht Bestandteil des Repositorys. Beim Wiederherstellen wird Bunker 32 auf dem Gerät ersetzt; die Projektdateien bleiben erhalten.

## Projektstruktur und geprüfter Stand

| Pfad | Inhalt |
|---|---|
| `firmware/Bunker32/` | ESP32-Sketch, Spiellogik, Renderer und Display-Konfiguration |
| `pc/` | C#-Quellcode und Build-Skript für das Windows-Steuerfenster |
| `Spielen.cmd` | PC-Steuerung bauen und starten |
| `Build-And-Flash.ps1` | Firmware kompilieren und aufspielen |
| `tests/` | Karten-/Grafikprüfung und Integrationstest auf dem ESP32 |
| `docs/screenshots/` | Originale, über USB gelesene Bilder vom ESP32 |

Die Firmware wurde auf den ESP32 geflasht und die geschriebenen Daten per Hash geprüft. Der Gerätetest bestand: Spielmechanik-Selbsttest, Start, Bewegung über USB, Eingabe-Timeout, Kampf, Pause und Bildübertragung. Die vorhandene Touch-Kalibrierung wurde erfolgreich geladen; die physische Touch-Bedienung wurde nicht automatisch getestet. Der Build verwendet 318.656 Byte Programmspeicher und 105.676 Byte globale Variablen. Im Gerätetest wurden bis zu 18 Bilder/s gemeldet.

Die statische Prüfung kann ohne angeschlossenen ESP32 laufen:

```powershell
python .\tests\check_layout.py
```

## USB-Protokoll

921600 Baud, 8N1. ASCII-Zeilen mit LF. `K n` setzt die Eingabebits: vor=1, zurück=2, links=4, rechts=8, seitwärts links=16, seitwärts rechts=32, Feuer=64. Mehrere Bits können kombiniert werden. Mindestens alle 250 ms auffrischen; sonst stoppt die Bewegung.

Weitere Befehle: `START`, `NEW`, `MENU`, `PAUSE`, `AUTO`, `MAP`, `CAL`, `CANCEL`, `STATE`, `SELFTEST`, `SNAP`, `STREAM 1`, `STREAM 0`. Streaming braucht `K` als Heartbeat innerhalb von zwei Sekunden. Ein Bildpaket enthält `B32F`, 32-Bit-Little-Endian-Länge 38400, acht Metadatenbytes (HP, Munition bis 255, besiegte Wachen, Modus, FPS, Schlüssel, Auto-Feuer, Kalibrierung), dann 160 × 120 RGB565-Pixel, Little Endian. Modi: Titel=0, Spiel=1, Pause=2, Sieg=3, Tod=4.

Der Gerätetest prüft mit unabhängigen Spielzuständen unter anderem Karten-Erreichbarkeit, Kollision, Sichtlinien, Raycasting, Kampf und Siegbedingung; anschließend echte serielle Bewegung, Timeout, Kampf, Pause und Bildschirmübertragung. Er setzt die Spielrunde zurück.

Technische Referenzen: [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI), [Espressif Flash-Werkzeuge](https://docs.espressif.com/projects/esptool/en/latest/esp32/esptool/basic-commands.html).

# Zwitscher - Ordner-basierter Zufalls-MP3-Player 🎵
![Bild der Zwitscherbox](https://github.com/menckow/Zwitscher/blob/main/doc/Zwitscher.png)
Ein intelligenter, ordner-basierter MP3-Player basierend auf dem **ESP32-S3 (YB-ESP32-S3-AMP)** Controller.

### ✨ Hauptfunktionen im Überblick
- **Ordner-basierte Wiedergabe** mit einem Intro-Sound für jeden Ordner.
- **PIR-Sensor** startet eine zufällige MP3-Wiedergabe, wenn Bewegung erkannt wird.
- **Hardware-Bedienelemente** für Lautstärke (Potentiometer) und Ordnerauswahl (Taster).
- **Freundschaftslampen-Funktion:** Leuchtet auf, wenn eine *andere* Box Bewegung erkennt oder per Taster sendet.
- **Zwei separate MQTT-Integrationen** für Smart Home Systeme (z.B. Home Assistant) und die Freundschaftslampen-Funktion.
- **Konfigurierbare LED-Effekte** (Helligkeit, Fade-Dauer).
- **NVS-Speicher** für Lautstärke und den letzten Ordner, um den Status nach einem Standby beizubehalten.
- **Integriertes Webportal** zur einfachen Konfiguration bei Verbindungsproblemen (inklusive visueller Farbauswahl für die LED).

## 🌟 Detaillierte Funktionen

- **Ordner-basierte Wiedergabe:** MP3s sind in Ordnern auf der SD-Karte organisiert. Ein Taster ermöglicht das Wechseln zwischen den Ordnern.
- **Intro-Sound:** Beim Ordnerwechsel wird automatisch eine `intro.mp3` Datei aus dem neuen Ordner abgespielt (falls vorhanden).
- **Bewegungsmelder (PIR):** Erkennt Bewegung und spielt dann eine zufällige MP3-Datei aus dem aktuellen Ordner ab. Löst auch das Senden eines MQTT-Signals an die Freundschaftslampe aus.
- **Hardware-Bedienelemente:**
  - **Potentiometer** für eine weiche und stufenlose Lautstärkeregelung.
  - **Taster** mit Entprellung zum Wechseln der Ordner.
- **Standby-Modus & NVS:**
  - Nach 5 Minuten Inaktivität wechselt der ESP32 automatisch in den **Standby-Modus**. Die Audiowiedergabe stoppt und der LED-Ring wird ausgeschaltet, aber WLAN und MQTT bleiben aktiv (wichtig für die Empfangsfunktion der Freundschaftslampe).
  - Wacht intern auf, wenn der PIR-Sensor ausgelöst oder ein Taster gedrückt wird.
  - Der aktuelle Ordner und die Lautstärke werden im **NVS (Non-Volatile Storage)** gespeichert, um Datenintegrität sicherzustellen.
- **Freundschaftslampe (RGB LED Ring):**
  - **FUNKTIONSWEISE:** Wenn eine *andere* Box Bewegung erkennt und ein MQTT-Signal sendet, leuchtet der eigene LED-Ring in der Farbe des Senders. Das Auslösen des *eigenen* PIR-Sensors lässt die eigene Lampe **nicht** aufleuchten, sondern sendet nur ein Signal an die anderen.
  - **Zwei Topics:** Die Box unterscheidet zwischen Nachrichten auf dem `ZWITSCHERBOX_TOPIC` (der Ring leuchtet komplett in der gesendeten Farbe) und dem `FRIENDLAMP_TOPIC` (jede dritte LED des Rings leuchtet in der Komplementärfarbe).
  - Die LEDs blenden mit einem weichen **Fade-Effekt** ein und aus. Die Dauer und Helligkeit können in der `config.txt` angepasst werden.
- **Smart Home / MQTT Integration:**
  - Die MQTT-Funktionalität ist in zwei Teile aufgeteilt:
    - Eine Integration für **Statusmeldungen**, die an einen internen Broker gesendet werden (z.B. Home Assistant).
    - Eine separate Integration für die **Freundschaftslampen-Funktion**, die auch einen öffentlichen Broker nutzen kann.
    - Beide können unabhängig voneinander aktiviert werden. Das Gerät bleibt komplett offline, wenn beide deaktiviert sind.
  - Verbindet sich über WLAN und sendet Echtzeit-Statusaktualisierungen über MQTT (Lautstärke, Wiedergabestatus, Fehler, aktuelle IP).
- **Timeouts:** Die maximale Wiedergabedauer für eine Session ist auf 5 Minuten begrenzt, um Endlosschleifen zu verhindern.
- **Web-Konfigurationsportal:** Wenn keine gültige WLAN-Verbindung hergestellt werden kann, startet das Gerät seinen eigenen passwortgeschützten Access Point (`Zwitscherbox`, **Passwort: `12345678`**). Alle Einstellungen lassen sich bequem über eine Weboberfläche mit Dropdown-Menüs (inkl. WLAN-Scanner) konfigurieren, ohne die SD-Karte manuell bearbeiten zu müssen. Die Farbe der Freundschaftslampe kann über einen visuellen HTML-Farbwähler besonders intuitiv eingestellt werden. Das UI kann optional mit einem permanenten Administrator-Passwort abgesichert werden.
- **SD-Karten Datei-Manager:** Die Weboberfläche beinhaltet auch einen vollwertigen `/files` Dateimanager-Endpunkt! Damit kannst du direkt im Browser durch das Verzeichnis der SD-Karte navigieren, neue Ordner anlegen, Dateien löschen und neue MP3s über WLAN hochladen, ohne die SD-Karte jemals aus dem ESP32 herausnehmen zu müssen. Große MP3-Dateien werden flüssig über asynchrone, speichersichere Datenblöcke hochgeladen – inkl. Live-Fortschrittsbalken.

## 🛠 Hardware-Anforderungen

- **Board:** YB-ESP32-S3-AMP (basiert auf Arduino ESP32 Core >= v3.1.1)
- **SD-Karten-Modul:** Integriert (SPI)
- **PIR-Sensor:** Verbunden mit `Pin 18`
- **Taster:** Verbunden mit `Pin 17` (interner Pull-Up konfiguriert)
- **Potentiometer:** Verbunden mit `Pin 4`
- **RGB LED Ring (NeoPixel):** Datenleitung an `Pin 16`
- **I2S Audio:** Pins zur I2S-Konfiguration (in die Audio-Bibliothek integriert).

## 🗂 Ordnerstruktur auf der SD-Karte
Die SD-Karte sollte folgendermaßen formatiert sein:
```
/
├── config.txt                   # (Optional) WLAN- und MQTT-Konfiguration
├── VogelSounds/                 # Ordner 1
│   ├── intro.mp3                # Intro für Ordner 1 ("Du hörst jetzt Vögel")
│   ├── amsel.mp3
│   └── drossel.mp3
└── Wasserfall/                  # Ordner 2
    ├── intro.mp3
    ├── wasserfall1.mp3
    └── wasserfall2.mp3
```

## ⚙️ Konfiguration (`config.txt`)

Um MQTT und WLAN zu nutzen, erstelle eine `config.txt` Datei im Hauptverzeichnis der SD-Karte. Hier ist ein Beispiel mit allen Optionen:

```ini
# --- 1. MQTT Aktivierung ---
# Setze diesen Wert auf 1, um Statusmeldungen an deinen internen
# MQTT-Broker (z.B. Home Assistant) zu aktivieren.
MQTT_INTEGRATION=1

# Setze diesen Wert auf 1, um die Freundschaftslampen-Funktion über MQTT
# zu aktivieren. Dies kann der interne oder ein öffentlicher Broker sein.
FRIENDLAMP_MQTT_INTEGRATION=1

# Hinweis: Wenn beide Werte auf 0 stehen, bleibt das Gerät offline (kein WLAN).

# --- 2. WLAN Einstellungen ---
# Erforderlich, falls eine der MQTT-Integrationen aktiviert ist.
WIFI_SSID=DeinWlanName
WIFI_PASS=DeinWlanPasswort

# --- 3. Interner MQTT Broker (für Home Assistant) ---
MQTT_SERVER=192.168.1.100
MQTT_PORT=1883
MQTT_USER=mqtt_user
MQTT_PASS=mqtt_password
MQTT_CLIENT_ID=ESP32_AudioPlayer
MQTT_BASE_TOPIC=audioplayer

# --- 4. Freundschaftslampe (RGB LED Ring) ---
# Schaltet die Hardware des LED Rings ein (1) oder aus (0).
FRIENDLAMP_ENABLE=1
# Feste Sende-Farbe für DIESE Box im Hexadezimalformat (visuell wählbar im Webportal)
FRIENDLAMP_COLOR=0000FF
# Topic für Freundschaftslampen-Signale
FRIENDLAMP_TOPIC=freundschaft/farbe
# Topic für Zwitscherbox-Signale
ZWITSCHERBOX_TOPIC=zwitscherbox/farbe

# Steuerung für LED Effekte
LED_FADE_EFFECT=1
LED_FADE_DURATION=1000
LED_BRIGHTNESS=100

# --- Externer Broker für die Freundschaftslampe (optional) ---
# Bleiben diese Felder hier leer, wird automatisch der interne Broker genutzt.
FRIENDLAMP_MQTT_SERVER=broker.hivemq.com
FRIENDLAMP_MQTT_PORT=1883
FRIENDLAMP_MQTT_USER=
FRIENDLAMP_MQTT_PASS=

# TLS Verschlüsselung (nur relevant für den externen Friendlamp Broker)
# Setze 1, um die Verbindung zum Friendlamp Broker zu verschlüsseln (meist Port 8883).
# Der interne Home Assistant Broker bleibt unberührt und unverschlüsselt.
FRIENDLAMP_MQTT_TLS_ENABLED=0

# Falls TLS aktiviert ist, MUSS das Root Zertifikat (BEGIN_CERT...END_CERT) in die config.txt kopiert werden.
```
Wird die Datei weggelassen oder beide `_INTEGRATION` Flags auf `0` gesetzt, läuft der Player komplett offline.

## 🚀 Installation & Kompilierung

Dieses Projekt verwendet **PlatformIO**. 
1. Klone oder lade das Repository herunter.
2. Öffne den Ordner in VSCode / PlatformIO.
3. Die Abhängigkeiten (wie `ESP32-audioI2S` und `PubSubClient`) werden beim ersten Build automatisch über die `platformio.ini` heruntergeladen.
4. Verbinde das Board über USB und klicke auf **Upload and Monitor**.

## 📡 MQTT Topics

Wenn die MQTT-Verbindung aktiv ist, veröffentlicht das Setup an die folgenden Topics (basierend auf `MQTT_BASE_TOPIC`, z.B. `audioplayer`):

* `audioplayer/status`: Statusnachrichten ("Online", "Playing Intro", "Entering Standby", "Woke up from Standby" usw.)
* `audioplayer/volume`: Aktuelle Lautstärke.
* `audioplayer/directory`: Pfad zum aktuell gewählten Ordner.
* `audioplayer/playing`: Der Pfad der aktuell abgespielten MP3-Datei oder "STOPPED".
* `audioplayer/ip_address`: Die aktuelle IP-Adresse im WLAN-Netzwerk.
* `audioplayer/friendlamp`: Wird für den Austausch von Farben der Freundschaftslampe zwischen den Geräten genutzt.
* `audioplayer/error`: Fehlermeldungen (z.B. Dateisystem-Fehler, fehlende MP3s).

### 📝 MQTT JSON Payload (Freundschaftslampe & Zwitscherbox)

Die Hardware übermittelt die Sensordaten untereinander über ein strukturiertes JSON-Format zur Farbübermittlung. Aus Gründen der Abwärtskompatibilität werden auch ältere String-Nachrichten im Legacy Format (`SenderID:HexFarbe`) fehlerfrei decodiert (Fallback). 

Ein Standard JSON-Paket (von der Freundschaftslampe oder dem Smart Home System) sieht folgendermaßen aus:

```json
{
  "client_id": "Mein_PIR_Sensor_ESP",
  "color": "FF0000",
  "effect": "fade",
  "duration": 30000
}
```

- **`client_id` (string):** Der eindeutige Name des abspielenden Geräts (wird genutzt, um Endlosschleifen zu vermeiden und sicherzustellen, dass die Box nicht auf ihre eigenen Nachrichten reagiert).

- **`color` (string):** Der HEX Farbcode (z.B. `00FF00` oder `#00FF00` für grün). Wird als Farbe das Wort `RAINBOW` übergeben, startet der versteckte, kreisende Lichteffekt.
- **`effect` (string):** Steuert das Animationsverhalten der NeoPixel LEDs. Unterstützt werden `fade` (weiches Einblenden), `blink` (1x pro Sekunde im Takt) und `rainbow`.
- **`duration` (Integer):** Zeit in Millisekunden für die Dauer des Aufleuchtens, bevor sich die LEDs wieder abschalten (Standard: `30000` = 30 Sekunden). Dies ermöglicht es Home Assistant, die Dauer der Signale perfekt dem Anwendungsszenario anzugleichen (kurzer Alarm vs. langes Hintergrundlicht).

### 🔄 OTA Update via MQTT

Die Firmware kann Over-The-Air (OTA) über MQTT aktualisiert werden. Sende dazu ein JSON-Paket an das statische Topic `zwitscherbox/update/trigger` (dies ist unabhängig von deinen Topic-Einstellungen in der config.txt):

```json
{
  "url": "http://dein-server.de/firmware.bin",
  "version": "7.0.1"
}
```

Wenn die ankommende `version` von der aktuellen `FW_VERSION` im Quellcode abweicht, stoppt das Gerät die Audiowiedergabe, lässt den LED-Ring konstant blau leuchten und beginnt mit dem Firmware-Download. Der Fortschritt und der aktuelle Status werden auf dem festen Topic `zwitscherbox/update/status` veröffentlicht. Die Payload enthält dabei neben der Meldung auch immer die Version und die Client-ID im Format `V7.0.0:ESP32_AudioPlayer - Meldung`. Nach erfolgreicher Installation startet das Gerät automatisch neu. Bei einem Fehler blinken die LEDs kurz rot auf und eine entsprechende Fehlermeldung wird per MQTT abgesetzt.

## 📄 Lizenz

Dieses Projekt wurde ausschließlich als privat/open-source entwickelt. 

## 🐞 Fehlerbehebung (Troubleshooting)

### Build Fehler: 'not able to power down in light sleep'

Während der Kompilierung kann folgende Fehlermeldung auftauchen, in der Regel bei Initialisierung des NeoPixel Rings:

```
E (3637) rmt: rmt_new_tx_channel(269): unable to power down into light sleep
[  3664][E][esp32-hal-rmt.c:548] rmtInit(): GPIO 16 - RMT TX initialisation error.
```

- **Ursache:** Dieser Fehler wird durch einen Bug in älteren Versionen des ESP32 Frameworks (Arduino Core) für PlatformIO verursacht. Der per NeoPixel-Bibliothek verwendete RMT-Treiber hat ein Kompatibilitätsproblem mit dem Energiesparmodus der MCU.

- **Lösung:** Um diesen Fehler zu beheben, wurde die `platformio.ini` geändert. Der Plattformlink wurde auf ein Github Repository gesetzt, um eine verlässliche und gepatchte Version des Frameworks zu nutzen, welche zudem auch die Board-Definitionen für das YB-ESP32-S3-AMP mitbringt.

Die entscheidende Zeile in der `platformio.ini` lautet:
```ini
[env]
platform = https://github.com/pioarduino/platform-espressif32.git
```

Durch diese Änderung ist sichergestellt, dass das Projekt auf einem kompatiblen Core fehlerfrei kompiliert wird.

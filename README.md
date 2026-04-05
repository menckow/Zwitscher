# Zwitscher - Directory-Based Random MP3 Player 🎵
![Bild der Zwitscherbox](https://github.com/menckow/Zwitscher/blob/main/doc/Zwitscher.png)
Ein intelligenter, verzeichnisbasierter MP3-Player auf Basis des **ESP32-S3 (YB-ESP32-S3-AMP)** Controllers.

### ✨ Kernfunktionen im Überblick
- **Verzeichnisbasierte Wiedergabe** mit Intro-Sound pro Ordner.
- **PIR-Sensor** startet zufällige MP3-Wiedergabe bei Bewegung.
- **Hardware-Steuerung** für Lautstärke (Poti) und Ordnerwechsel (Taster).
- **Freundschaftslampen-Funktion:** Leuchtet auf, wenn eine *andere* Box eine Bewegung erkennt.
- **Zwei getrennte MQTT-Integrationen** für Smart Home (z.B. Home Assistant) und die Freundschaftslampen-Funktion.
- **Konfigurierbare LED-Effekte** (Helligkeit, Fade-Dauer).
- **NVS-Speicher** für Lautstärke und letzten Ordner, um den Zustand nach Standby beizubehalten.
- **Integriertes Web-Portal** zur einfachen Konfiguration bei Verbindungsproblemen (inkl. visueller Farbauswahl für die LED).

## 🌟 Detaillierte Features

- **Verzeichnisbasierte Wiedergabe:** MP3s werden in Ordnern auf der SD-Karte organisiert. Über einen Taster lässt sich zwischen den Verzeichnissen wechseln.
- **Intro-Sound:** Beim Ordnerwechsel wird automatisch eine `intro.mp3` aus dem neuen Verzeichnis abgespielt (falls vorhanden).
- **Bewegungsmelder (PIR):** Erkennt Bewegung und spielt anschließend zufällig eine MP3-Datei aus dem aktuellen Verzeichnis ab. Löst außerdem das Senden eines MQTT-Signals für die Freundschaftslampe aus.
- **Hardwaresteuerung:**
  - **Potentiometer** zur flüssigen und geglätteten Lautstärkeregelung.
  - **Taster** inklusive Entprellung (Debounce) zum Wechseln der Verzeichnisse.
- **Standby-Modus & NVS:**
  - Bei 5 Minuten Inaktivität geht der ESP32 automatisch in einen **Standby-Modus**. Die Audiowiedergabe stoppt und der LED-Ring wird dunkel geschaltet, aber WLAN und MQTT bleiben aktiv (wichtig für die passive Empfängerfunktion der Freundschaftslampe).
  - Wacht beim Auslösen des PIR-Sensors oder per Tasterdruck intern auf.
  - Das aktuelle Verzeichnis und die Lautstärke werden ausfallsicher im **NVS (Non-Volatile Storage)** gespeichert.
- **Freundschaftslampe (RGB LED Ring):**
  - **KORREKTUR:** Wenn eine *andere* Box eine Bewegung erkennt und ein MQTT-Signal sendet, leuchtet der eigene LED-Ring in der Farbe des Senders auf. Das Auslösen des *eigenen* PIR-Sensors führt **nicht** zum Leuchten der eigenen Lampe, sondern sendet nur ein Signal an die anderen.
  - **Zwei Topics:** Die Box unterscheidet zwischen Nachrichten auf dem `ZWITSCHERBOX_TOPIC` (Ring leuchtet komplett in der gesendeten Farbe) und dem `FRIENDLAMP_TOPIC` (Jede dritte LED des Rings leuchtet in der Komplementärfarbe auf).
  - Die LEDs blenden mit einem sanften **Fade-Effekt** ein und aus. Dauer und Helligkeit sind in der `config.txt` einstellbar.
- **Smart-Home / MQTT Integration:**
  - Die MQTT-Funktionalität ist aufgeteilt:
    - Eine Integration für **Statusmeldungen** an einen internen Broker (z.B. Home Assistant).
    - Eine separate Integration für die **Freundschaftslampen-Funktion**, die auch einen öffentlichen Broker nutzen kann.
    - Beide können unabhängig voneinander aktiviert werden. Das Gerät bleibt komplett offline, wenn beide deaktiviert sind.
  - Verbindet sich über WiFi und schickt Echtzeit-Statusupdates über MQTT (Lautstärke, Wiedergabestatus, Fehler, aktuelle IP).
- **Timeouts:** Maximale Wiedergabedauer für eine Random-Session ist auf 5 Minuten limitiert um Endlos-Wiedergabe zu vermeiden.
- **Web-Konfigurationsportal:** Wenn keine gültige WLAN-Verbindung hergestellt werden kann, startet das Gerät einen eigenen Access Point (`Zwitscherbox`). Über eine Weboberfläche können alle Einstellungen bequem konfiguriert werden, ohne die SD-Karte manuell bearbeiten zu müssen. Die Farbe der Freundschaftslampe lässt sich hierbei besonders intuitiv über einen visuellen HTML-Farbwähler einstellen.

## 🛠 Hardware-Anforderungen

- **Board:** YB-ESP32-S3-AMP (basierend auf Arduino ESP32 core >= v3.1.1)
- **SD-Kartenmodul:** Integriert (SPI)
- **PIR Sensor:** Angeschlossen an `Pin 18`
- **Taster:** Angeschlossen an `Pin 17` (Pull-up intern konfiguriert)
- **Potentiometer:** Angeschlossen an `Pin 4`
- **RGB LED Ring (NeoPixel):** Datenleitung angeschlossen an `Pin 16`
- **I2S Audio:** Auspins für I2S Konfiguration (in Audiobibliothek integriert).

## 🗂 Ordnerstruktur auf der SD-Karte

Die SD-Karte sollte wie folgt strukturiert sein:

```
/
├── config.txt                   # (Optional) WLAN- und MQTT-Konfiguration
├── Vogelstimmen/                # Verzeichnis 1
│   ├── intro.mp3                # Intro für Verzeichnis 1 ("Sie hören nun Vögel")
│   ├── amsel.mp3
│   └── drossel.mp3
└── Wasserfall/                  # Verzeichnis 2
    ├── intro.mp3
    ├── waterfall1.mp3
    └── waterfall2.mp3
```

## ⚙️ Konfiguration (`config.txt`)

Um MQTT und WLAN zu nutzen, erstelle eine `config.txt` im Hauptverzeichnis (Root) der SD-Karte. Hier ein Beispiel mit allen neuen Optionen:

```ini
# --- 1. MQTT-Aktivierung ---
# Setze diesen Wert auf 1, um die Status-Übermittlung an deinen internen
# MQTT Broker (z.B. Home Assistant) zu aktivieren.
MQTT_INTEGRATION=1

# Setze diesen Wert auf 1, um die Freundschaftslampen-Funktion über MQTT
# zu aktivieren. Dies kann der interne oder ein öffentlicher Broker sein.
FRIENDLAMP_MQTT_INTEGRATION=1

# Hinweis: Wenn beide obigen Werte auf 0 stehen, bleibt das Gerät offline (kein WLAN).

# --- 2. WLAN (WiFi) Einstellungen ---
# Erforderlich, wenn eine der MQTT-Integrationen aktiviert ist.
WIFI_SSID=DeinWLANName
WIFI_PASS=DeinWLANPasswort

# --- 3. Interner MQTT Broker (für Home Assistant) ---
MQTT_SERVER=192.168.1.100
MQTT_PORT=1883
MQTT_USER=mqtt_benutzer
MQTT_PASS=mqtt_passwort
MQTT_CLIENT_ID=ESP32_AudioPlayer
MQTT_BASE_TOPIC=audioplayer

# --- 5. Freundschaftslampe (RGB LED Ring) ---
# Schaltet die LED-Ring-Hardware an (1) oder aus (0).
FRIENDLAMP_ENABLE=1
# Feste Sende-Farbe für DIESE Box im Hex-Format (im Web-Portal visuell auswählbar)
FRIENDLAMP_COLOR=0000FF
# Topic für Freundschafts-Farbsignale
FRIENDLAMP_TOPIC=freundschaft/farbe
# Topic für Zwitscherbox-Farbsignale
ZWITSCHERBOX_TOPIC=zwitscherbox/farbe

# Steuerung für LED-Effekte
LED_FADE_EFFECT=1
LED_FADE_DURATION=1000
LED_BRIGHTNESS=100

# --- Externer Broker für die Freundschaftslampe (Optional) ---
# Wenn diese Felder leer bleiben, wird automatisch der interne Broker verwendet.
FRIENDLAMP_MQTT_SERVER=broker.hivemq.com
FRIENDLAMP_MQTT_PORT=1883
FRIENDLAMP_MQTT_USER=
FRIENDLAMP_MQTT_PASS=

# TLS-Verschlüsselung (nur für den externen Friendlamp Broker relevant)
# Setze auf 1, um die Verbindung zum Friendlamp-Broker zu verschlüsseln (Port meist 8883).
# Der interne Home Assistant Broker bleibt davon unberührt und unverschlüsselt.
FRIENDLAMP_MQTT_TLS_ENABLED=0

# Wenn TLS aktiviert wird, muss in der config.txt das Root-Zertifikat (BEGIN_CERT...END_CERT) eingefügt werden.
```
Wird die Datei weggelassen oder sind beide `_INTEGRATION` Flags auf `0` gesetzt, läuft der Player komplett offline.

## 🚀 Installation & Kompilierung

Dieses Projekt verwendet **PlatformIO**. 

1. Klone oder lade das Repository herunter.
2. Öffne den Ordner in VSCode / PlatformIO.
3. Die Abhängigkeiten (wie `ESP32-audioI2S` und `PubSubClient`) werden beim ersten Build automatisch über die `platformio.ini` heruntergeladen.
4. Schließe das Board per USB an und klicke auf **Upload und Monitor**.

## 📡 MQTT Topics

Bei aktiver MQTT-Verbindung veröffentlicht der Aufbau auf folgenden Topics (basierend auf `MQTT_BASE_TOPIC`, z.B. `audioplayer`):

* `audioplayer/status`: Statusnachrichten ("Online", "Playing Intro", "Entering Standby", "Woke up from Standby", etc.)
* `audioplayer/volume`: Aktuelle Lautstärke.
* `audioplayer/directory`: Der Pfad zum frisch ausgewählten Verzeichnis auf der SD-Karte.
* `audioplayer/playing`: Der Pfad der aktuell abgespielten MP3-Datei oder "STOPPED".
* `audioplayer/ip_address`: Die aktuelle IP-Adresse im WLAN.
* `audioplayer/friendlamp`: Wird zum Austausch der Farben für die Freundschaftslampe zwischen den Geräten genutzt.
* `audioplayer/error`: Fehler-Meldungen (z.B. Dateisystem-Fehler, fehlende MP3s).

### 📝 MQTT JSON-Payload (Freundschaftslampe & Zwitscherbox)

Die Hardware kommuniziert zur Farbübergabe und Effektsteuerung untereinander über ein strukturiertes JSON-Format. Aus Gründen der Abwärtskompatibilität werden auch noch ältere String-Nachrichten im Legacy-Format (`SenderID:HexFarbe`) fehlerfrei decodiert (Fallback-Mechanismus). 

Ein reguläres JSON-Paket vom Sender (oder deinem Smart Home System) sieht wie folgt aus:

```json
{
  "client_id": "Mein_PIR_Sensor_ESP",
  "color": "FF0000",
  "effect": "fade",
  "duration": 30000
}
```

- **`client_id` (String):** Der eindeutige Name des abspielenden Gerätes (wird genutzt, damit die Box nicht auf ihre eigenen Nachrichten reagiert und Endlosschleifen verhindert werden).
- **`color` (String):** Der HEX-Farbcode (z. B. `00FF00` für Grün). Wird als Farbe das Wort `RAINBOW` übergeben, startet der versteckte, kreisende Licht-Effekt.
- **`effect` (String):** Steuert das Animationsverhalten der NeoPixel-LEDs. Unterstützte Werte sind `"fade"` (weiches Einblenden), `"blink"` (sekündliches Aufblinken im Takt) und `"rainbow"`.
- **`duration` (Integer):** Zeit in Millisekunden, wie lange die LEDs leuchten sollen, bevor sie sich wieder automatisch abschalten (Standard: `30000` = 30 Sekunden). Hiermit kann ein Home Assistant die Dauer der optischen Signale je nach Anwendungsszenario (z.B. kurzer Alarm vs. langes Hintergrundleuchten) perfekt steuern.

## 📄 Lizenz

Dieses Projekt wurde exklusiv als Private/Open-Source-Lösung entwickelt. 

## 🐞 Fehlerbehebung (Troubleshooting)

### Build-Fehler: "not able to power down in light sleep"

Beim Kompilieren des Projekts kann es zu folgender Fehlermeldung kommen, die typischerweise bei der Initialisierung des NeoPixel-LED-Rings auftritt:

```
E (3637) rmt: rmt_new_tx_channel(269): not able to power down in light sleep
[  3664][E][esp32-hal-rmt.c:548] rmtInit(): GPIO 16 - RMT TX Initialization error.
```

- **Ursache:** Dieser Fehler wird durch einen Bug in älteren Versionen des ESP32-Frameworks (Arduino Core) für PlatformIO verursacht. Der RMT-Peripherie-Treiber, der von der NeoPixel-Bibliothek verwendet wird, hat ein Kompatibilitätsproblem mit den Energiesparmodi des ESP32.

- **Lösung:** Um dies zu beheben, wurde die `platformio.ini`-Datei angepasst. Die `platform`-Einstellung wurde auf ein spezifisches Git-Repository gesetzt, das eine neuere Version des Frameworks enthält, die diesen Fehler behebt und gleichzeitig die notwendigen Board-Definitionen für das YB-ESP32-S3-AMP-Board bereitstellt.

Die relevante Zeile in `platformio.ini` lautet:
```ini
[env]
platform = https://github.com/pioarduino/platform-espressif32.git
```

Diese Änderung stellt sicher, dass das Projekt mit einer kompatiblen und fehlerbereinigten Version des ESP32-Frameworks erstellt wird.

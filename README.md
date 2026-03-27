# Zwitscher - Directory-Based Random MP3 Player 🎵

Ein intelligenter, verzeichnisbasierter MP3-Player auf Basis des **ESP32-S3 (YB-ESP32-S3-AMP)** Controllers. Das Projekt nutzt einen SD-Kartenleser, einen PIR-Bewegungssensor, einen Hardware-Taster zur Verzeichnisauswahl, ein Potentiometer zur Lautstärkeregelung sowie optionale **MQTT- und WiFi-Integration** (z. B. zur Anbindung an Smart-Home-Systeme).

## 🌟 Features

- **Verzeichnisbasierte Wiedergabe:** MP3s werden in Ordnern auf der SD-Karte organisiert. Über einen Taster lässt sich zwischen den Verzeichnissen wechseln.
- **Intro-Sound:** Beim Ordnerwechsel wird automatisch eine `intro.mp3` aus dem neuen Verzeichnis abgespielt (falls vorhanden).
- **Bewegungsmelder (PIR):** Erkennt Bewegung und spielt anschließend zufällig eine MP3-Datei aus dem aktuellen Verzeichnis ab.
- **Hardwaresteuerung:**
  - **Potentiometer** zur flüssigen und geglätteten Lautstärkeregelung.
  - **Taster** inklusive Entprellung (Debounce) zum Wechseln der Verzeichnisse.
- **Standby-Modus & NVS:**
  - Bei 5 Minuten Inaktivität geht der ESP32 automatisch in einen **Standby-Modus**. Die Audiowiedergabe stoppt und der LED-Ring wird dunkel geschaltet, aber WLAN und MQTT bleiben aktiv (wichtig für die passive Empfängerfunktion der Freundschaftslampe).
  - Wacht beim Auslösen des PIR-Sensors oder per Tasterdruck intern auf.
  - Das aktuelle Verzeichnis und die Lautstärke werden ausfallsicher im **NVS (Non-Volatile Storage)** gespeichert.
- **Freundschaftslampe (RGB LED Ring):**
  - Ein an Pin 12 angeschlossener LED-Ring leuchtet in deiner eigenen (in `config.txt` festgelegten) Farbe auf, wenn der PIR-Sensor auslöst.
  - Optional wird dieses Farbsignal über MQTT an eine zweite Box gesendet, die dann in deiner Farbe leuchtet. (Hierfür kann auf Wunsch ein unabhängiger öffentlicher Broker, getrennt vom Smart-Home-Broker, konfiguriert werden).
- **Smart-Home / MQTT Integration:**
  - Optional konfigurierbar über eine `config.txt` auf der SD-Karte.
  - Verbindet sich über WiFi und schickt Echtzeit-Statusupdates über MQTT (Lautstärke, Wiedergabestatus, Fehler, aktuelle IP).
- **Timeouts:** Maximale Wiedergabedauer für eine Random-Session ist auf 5 Minuten limitiert um Endlos-Wiedergabe zu vermeiden.

## 🛠 Hardware-Anforderungen

- **Board:** YB-ESP32-S3-AMP (basierend auf Arduino ESP32 core >= v3.1.1)
- **SD-Kartenmodul:** Integriert (SPI)
- **PIR Sensor:** Angeschlossen an `Pin 18`
- **Taster:** Angeschlossen an `Pin 17` (Pull-up intern konfiguriert)
- **Potentiometer:** Angeschlossen an `Pin 4`
- **RGB LED Ring (NeoPixel):** Datenleitung angeschlossen an `Pin 12`
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

Um MQTT und WLAN zu nutzen, erstelle eine `config.txt` im Hauptverzeichnis (Root) der SD-Karte mit folgendem Inhalt (Beispiel):

```ini
MQTT_INTEGRATION=1
WIFI_SSID=DeinWLANName
WIFI_PASS=DeinWLANPasswort
MQTT_SERVER=192.168.1.100
MQTT_PORT=1883
MQTT_USER=mqtt_benutzer
MQTT_PASS=mqtt_passwort
MQTT_CLIENT_ID=ESP32_AudioPlayer
MQTT_BASE_TOPIC=audioplayer

FRIENDLAMP_ENABLE=1
FRIENDLAMP_COLOR=0000FF
FRIENDLAMP_TOPIC=audioplayer/friendlamp

# --- Öffentlicher Broker für die Freundschaftslampe ---
# Wenn diese Felder leer bleiben, wird automatisch der interne Broker verwendet.
FRIENDLAMP_MQTT_SERVER=broker.hivemq.com
FRIENDLAMP_MQTT_PORT=1883
FRIENDLAMP_MQTT_USER=
FRIENDLAMP_MQTT_PASS=
```
Wird die Datei weggelassen oder `MQTT_INTEGRATION=0` gesetzt, läuft der Player komplett offline.

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

## 📄 Lizenz

Dieses Projekt wurde exklusiv als Private/Open-Source-Lösung entwickelt. 
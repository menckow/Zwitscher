# 🐦 Zwitscherbox & Freundschaftslampe

Ein smarter, bewegungsgesteuerter MP3-Player mit integriertem Leuchtring, WLAN-Konfigurationsportal und Smart-Home-Anbindung (MQTT).

## 🌟 Überblick

Dieses Gerät kombiniert einen interaktiven MP3-Player mit einer „Freundschaftslampe“. Sobald der Bewegungsmelder (PIR) eine Person erfasst, spielt das Gerät eine zufällige Audiodatei von der Speicherkarte ab. 
Gleichzeitig kann es übers Internet (MQTT) ein Signal an eine zweite Box senden, damit dort der integrierte LED-Ring aufleuchtet – so wisst ihr sofort, wenn der andere gerade an euch denkt!

## ✨ Hauptfunktionen

- **Bewegungserkennung:** Startet automatisch die zufällige Audiowiedergabe bei Bewegung.
- **Ordner-System:** Mehrere Ordner mit verschiedenen Sounds werden unterstützt. Ein Knopfdruck wechselt den Ordner (bestätigt durch einen kurzen Erkennungssound `intro.mp3`).
- **Freundschaftslampe (LED-Ring):** 
  - **Standard-Signal:** Der komplette Ring leuchtet auf (falls aktiviert auch mit einem sanften Einblenden) und dimmt sich nach 30 Sekunden wieder ab.
  - **Freundschafts-Signal:** Der Ring leuchtet auf, wobei zur optischen Unterscheidung jede 3. LED in der Komplementärfarbe (Kontrastfarbe) erscheint.
- **Einfache Einrichtung:** Keine Programmierung nötig – WLAN, Lichtfarbe und Smart-Home-Server lassen sich über ein bequemes Web-Interface am Handy einrichten.
- **Smart-Home fähig:** Kann über MQTT voll in ein Smart Home (z. B. Home Assistant) integriert werden (Status, Lautstärke, aktuell gespielter Titel, IP-Adresse).
- **Automatischer Standby/Speicher:** Das Gerät merkt sich den aktuellen Ordner und die Lautstärke automatisch. Nach 5 Minuten Inaktivität geht das Gerät selbstständig in den Stromsparmodus.

---

## 🛠️ 1. Die Erste Einrichtung (WLAN-Portal)

Wenn die Box zum ersten Mal gestartet wird (oder sie ihr eingespeichertes WLAN nicht finden kann), öffnet sie automatisch ihr eigenes Setup-Netzwerk.

1. Suche mit dem Smartphone oder Laptop nach dem WLAN-Netzwerk **"Zwitscherbox"** und verbinde dich.
2. Es sollte sich automatisch ein Anmeldefenster (Captive Portal) öffnen. Alternativ kannst du den Browser öffnen und eine beliebige Adresse eingeben.
3. Auf der Konfigurationsseite kannst du alle Einstellungen vornehmen:
   - **Netzwerk Name & Passwort:** Trage hier dein normales WLAN zu Hause ein.
   - **Farbe wählen:** Bestimme, in welcher Farbe dein LED-Ring aufleuchten soll, wenn die Box ein Signal empfängt.
   - **MQTT / Smart Home:** (Optional) Aktiviere die Verbindung zu deinem eigenen oder einem externen Smart-Home Server, um die Box mit deinen Freunden zu verbinden.
4. Klicke auf **"Konfiguration Speichern"**. Die Box startet nun neu und verbindet sich mit dem normalen heimischen WLAN.

---

## 🎛️ 2. Bedienung am Gerät

* **Lautstärke-Regler (Drehregler):** Drehe an dem Regler, um die Lautstärke stufenlos anzupassen.
* **Taster drücken:** Wechselt zum nächsten Audio-Ordner auf der SD-Karte. Beim Wechsel wird kurz eine Erkennungsmelodie gespielt, danach ist das System sofort wieder bereit, auf Bewegung zu reagieren.
* **Bewegungssensor:** Wird eine Bewegung erkannt, wählt das Gerät zufällig eine MP3-Datei aus dem aktuellen Ordner und spielt sie ab. 

---

## 📁 3. Vorbereitung der SD-Karte

Damit die Box Musik abspielen kann, muss die beigelegte SD-Karte wie folgt vorbereitet werden:

1. Erstelle einen oder mehrere Unterordner auf der SD-Karte (z.B. `Vogelstimmen`, `Waldgeräusche`, `Musik`).
2. Kopiere in jeden Ordner deine gewünschten Sound-Dateien (sie müssen exakt auf `.mp3` enden).
3. **WICHTIG:** Ergänze in jedem Haupt-Ordner eine weitere MP3-Datei und nenne sie exakt `intro.mp3`. Diese wird immer kurz angespielt, wenn jemand über den Knopf auf den Ordner wechselt. So weiß man sofort, welcher Ordner gerade aktiviert wurde.

---

## 💡 4. Wie funktioniert das Leuchten?

Sobald deine Box Bewegung erkennt, spielt sie nicht nur ein Audiofile ab, sondern funkt über das Internet (MQTT) im Hintergrund "verdeckt" an einen Kanal weiter, in welcher Farbe du leuchten möchtest. 

* **Deine Box empfängt ein allgemeines "Zwitscherbox-Signal":** Der LED-Ring füllt sich komplett mit der Empfänger-Farbe und wechselt sanft in einen Fade-Out (Dimmen) nach etwa 30 Sekunden. (Ideal für das Hauptsystem).
* **Deine Box empfängt ein Signal auf dem "Freundschafts-Kanal":** Optisch wird ein spezielles Muster gezeigt: Der Ring leuchtet auf, doch jede dritte LED leuchtet als Highlight in der *Komplementärfarbe*. So erkennst du auf den ersten Blick, auf welchem der beiden Kanäle die Box gerade angesprochen wurde!

---

## 🔧 Technische Spezifikationen (Für Interessierte)
- **Komponenten:** ESP32 (Modell: YB-ESP32-S3-AMP), PIR-Bewegungssensor (Pin 18), Taster (Pin 17), analoges Potentiometer (Pin 4), NeoPixel LED-Ring (16 LEDs an Pin 16).
- **Speicher:** NVS (Non-Volatile Storage) speichert Lautstärke und Ordnerauswahl ausfallsicher über Neustarts hinweg.
- **Wiedergabe-Sicherheit:** Einzelne Audio-Tracks haben ein eingebautes Zeitlimit von 5 Minuten. Bleibt der Raum ungenutzt, sinkt das System leise in den Standby-Modus ab.

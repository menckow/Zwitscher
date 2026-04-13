# 🐦 Zwitscher - Anwenderdokumentation

Willkommen zur Anleitung für deine neue **Zwitscherbox**! 
Die Zwitscherbox ist ein intelligenter, verzeichnisbasierter MP3-Player. Sie spielt auf Knopfdruck oder durch Bewegungserkennung entspannende Sounds ab und kann gleichzeitig über das Internet mit anderen Boxen oder Freundschaftslampen kommunizieren.

---

## 🛠️ 1. Die erste Einrichtung (Setup)

Wenn du die Box zum ersten Mal einschaltest, sucht sie nach einem bekannten WLAN. Findet sie keins, erstellt sie ein eigenes Einrichtungs-WLAN.

1. Öffne die WLAN-Einstellungen deines Smartphones oder Computers.
2. Suche nach dem Netzwerk mit dem Namen **`Zwitscherbox`** und verbinde dich damit.
3. Gib das Standard-Passwort ein: **`12345678`**
4. Normalerweise öffnet sich nun automatisch ein Konfigurationsfenster (Captive Portal). Falls nicht, öffne deinen Webbrowser und rufe die Adresse **`http://192.168.4.1`** auf.
5. In der Weboberfläche kannst du nun alle Einstellungen bequem über Menüs vornehmen:
   * Wähle dein **Heim-WLAN** aus der Liste aus und gib dein Passwort ein.
   * Du kannst die **Farbe deiner Zwitscherbox** über einen visuellen Farbwähler (Color Picker) ganz einfach festlegen.
   * Bei Bedarf kannst du auch die MQTT-Server-Einstellungen (für Smart Home) hinterlegen.
6. Klicke auf Speichern. Die Box startet neu und verbindet sich mit deinem Heimnetzwerk.

*Tipp:* Du musst die SD-Karte für diese Einstellungen nicht entnehmen, das Webinterface übernimmt das Anpassen der `config.txt` für dich!

---

## 💡 2. Bedienung im Alltag

### 🎛️ Lautstärke regeln
Drehe an dem **Drehregler (Potentiometer)**, um die Lautstärke der Sounds stufenlos anzupassen. Die Box merkt sich diese Lautstärke für das nächste Mal.

### 🎵 Ordner (Sounds) wechseln
Drücke den **Taster** an der Box. Die Zwitscherbox wechselt dann in das nächste Verzeichnis auf der SD-Karte (z.B. von "Vögel" zu "Wasserfall").
Gibt es in dem neuen Ordner eine Datei namens `intro.mp3`, wird diese zur Bestätigung kurz angespielt (z.B. eine Stimme, die "Wasserfall" sagt).

### 🚶 Bewegungsmelder (PIR)
Die Box ist mit einem Infrarot-Bewegungssensor ausgestattet:
* Sobald sie Bewegung erkennt, wählt sie **zufällig** eine MP3-Datei aus dem aktuell gewählten Ordner aus und spielt sie ab.
* Gleichzeitig sendet sie (wenn WLAN eingerichtet ist) ein unsichtbares Signal an andere verbundene Freundschaftslampen oder Zwitscherboxen in deinem Netzwerk.

### 💤 Standby-Modus
Wenn 5 Minuten lang keine Bewegung erkannt wurde und kein Sound spielt, geht die Box automatisch in einen stromsparenden **Standby-Modus**. Die LEDs gehen aus, aber sie bleibt im Hintergrund mit dem WLAN verbunden. Sobald du den Knopf drückst oder Bewegung erkannt wird, wacht sie sofort wieder auf.

---

## 🎨 3. Die Freundschaftslampen-Funktion

Deine Zwitscherbox hat einen integrierten, leuchtenden LED-Ring. Dieser leuchtet auf, wenn **andere** Zwitscherboxen oder Freundschaftslampen aus deinem Bekanntenkreis aktiviert werden!

* **Wie es funktioniert:** Wenn bei einem Freund der Bewegungsmelder auslöst, leuchtet der LED-Ring an *deiner* Box in seiner vorher festgelegten Farbe auf.
* **Gut zu wissen:** Wenn *dein* eigener Bewegungsmelder auslöst, leuchtet dein eigener LED-Ring nicht auf. Er schickt das Signal nur an die anderen weiter.
* Der Ring nutzt weiche **Fade-Effekte** (sanftes Ein- und Ausblenden). Helligkeit und Dauer können in den Einstellungen konfiguriert werden.

---

## 📁 4. MP3s hinzufügen (Dateimanager)

Du musst die Box nicht aufschrauben oder die SD-Karte herausnehmen, um neue Sounds aufzuspielen!

1. Gehe sicher, dass du im selben WLAN wie die Zwitscherbox bist.
2. Öffne den Dateimanager im Browser über die IP-Adresse deiner Zwitscherbox, indem du ein `/files` anhängst (z.B. `http://192.168.1.50/files`).
3. In diesem Dateimanager kannst du direkt auf die SD-Karte zugreifen:
   * **Neue Ordner** (Kategorien) anlegen.
   * **Alte MP3s** löschen.
   * **Neue Sounds** kabellos per WLAN hochladen. Ein Fortschrittsbalken zeigt dir dabei an, wie lange der Upload noch dauert.

*Tipp für die Struktur:* Lege für jede Sound-Kategorie einen eigenen Ordner an. Optional kannst du in jedem Ordner eine Datei namens `intro.mp3` ablegen, die abgespielt wird, wenn du mit dem Taster in diesen Ordner wechselst.

---

## 📡 5. Smart Home Integration (Für Profis)

Die Zwitscherbox kann optional in Smart-Home-Systeme (wie z.B. Home Assistant) eingebunden werden:
* Sie besitzt **zwei getrennte MQTT-Schnittstellen**: Eine für Status-Nachrichten an dein internes Smart Home, und eine separate für die globale Freundschaftslampen-Verbindung.
* Sie übermittelt Echtzeit-Werte wie Lautstärke, Wiedergabe-Status, die aktuelle IP-Adresse und eventuelle Fehler.
* Sie ist vollständig kompatibel mit dem zentralen **Lamp Manager Dashboard** zur Echtzeit-Überwachung deiner Geräteflotte.
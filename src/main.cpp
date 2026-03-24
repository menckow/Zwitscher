/*
  Directory-Based Random MP3 Player with Intro, PIR, Volume, Timeout, Button
  Version E: Added NVS save/load for directory index and volume across Deep Sleep.
  based on a YB-ESP32-S3-AMP https://github.com/yellobyte/ESP32-DevBoards-Getting-Started/tree/main/boards/YB-ESP32-S3-AMP
  Letzte Änderung: NVS Implementierung
*/

#include "Audio.h"
#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <vector>
#include "esp_sleep.h"
#include <Preferences.h> 
#include <PubSubClient.h>
#include <WiFi.h>
// ------------------------------------

#define MAX_PATH_DEPTH 1

// --- Pin-Definitionen ---
const int POT_PIN = 4;
const int PIR_PIN = 18;
const int SD_CS_PIN = SS;
const int BUTTON_PIN = 17; //38

// --- Timeout-Konstanten ---
const unsigned long maxPlaybackDuration = 5 * 60 * 1000UL;
const unsigned long deepSleepInactivityTimeout = 5 * 60 * 1000UL;

// --- Globale Objekte und Variablen ---
SPIClass *spi_onboardSD = new SPIClass(FSPI);
Audio audio;
Preferences preferences; // NEU: Objekt für NVS

// --- Variablen für Konfiguration und MQTT ---
String wifi_ssid = "";
String wifi_pass = "";
String mqtt_server = "";
int    mqtt_port = 1883; // Standard-Port
String mqtt_user = "";
String mqtt_pass = "";
String mqtt_client_id = "ESP32_AudioPlayer"; // Default, falls Laden fehlschlägt
String mqtt_base_topic = "audioplayer";      // Default, falls Laden fehlschlägt
bool   mqtt_integration_enabled = false;     // Standardmäßig DEAKTIVIERT

// Dynamisch erstellte MQTT Topics
String mqtt_topic_status;
String mqtt_topic_error;
String mqtt_topic_debug;    // Optional für detailliertere Infos
String mqtt_topic_volume;
String mqtt_topic_directory;
String mqtt_topic_playing;
String mqtt_topic_ip;

// MQTT Client Objekte
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;
const long mqttReconnectInterval = 5000; // Versuche alle 5 Sekunden erneut zu verbinden

// --- Globale Variablen ---
std::vector<String> directoryList;
std::vector<String> currentMp3Files;
int currentDirectoryIndex = -1; // Wird jetzt aus NVS geladen
String introFileName = "intro.mp3";

bool playing = false;
bool inPlaybackSession = false;
bool playingIntro = false;
unsigned long playbackStartTime = 0;

unsigned long lastPotReadTime = 0;
const unsigned long potReadInterval = 100;
int lastVolume = -1; // Wird jetzt aus NVS geladen

int lastButtonState = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;

unsigned long lastPirActivityTime = 0;
// Variablen für Potentiometer-Glättung
float smoothedPotValue = -1.0; // Initialwert, der eine sofortige Berechnung beim ersten Mal erzwingt
const float potSmoothingFactor = 0.2; // Glättungsfaktor (0.0 bis 1.0). Kleinere Werte = mehr Glättung, langsamere Reaktion. 0.1 ist ein guter Start.

// --- Funktion zum Laden der Konfiguration von SD ---
void loadConfig() {
    File configFile = SD.open("/config.txt");
    if (!configFile) {
        Serial.println("ERROR: config.txt not found on SD card!");
        mqtt_integration_enabled = false; // Deaktivieren, wenn Datei fehlt
        return;
    }

    Serial.println("Reading config.txt...");
    // Setze Standardwerte für den Fall, dass Keys fehlen
    mqtt_integration_enabled = false; // Erstmal deaktiviert, muss explizit aktiviert werden
    mqtt_client_id = "ESP32_AudioPlayer";
    mqtt_base_topic = "audioplayer";
    mqtt_port = 1883;


    while (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        line.trim(); // Whitespace entfernen

        if (line.length() == 0 || line.startsWith("#")) {
            continue; // Leere Zeilen oder Kommentare ignorieren
        }

        int separatorPos = line.indexOf('=');
        if (separatorPos == -1) {
            Serial.println("Skipping invalid line in config: " + line);
            continue; // Ungültige Zeile ohne '='
        }

        String key = line.substring(0, separatorPos);
        String value = line.substring(separatorPos + 1);
        key.trim();
        value.trim();
        key.toUpperCase(); // Case-insensitive Key-Vergleich

        // Serial.println("Key: [" + key + "], Value: [" + value + "]"); // Debugging

        if (key == "WIFI_SSID") {
            wifi_ssid = value;
        } else if (key == "WIFI_PASS") {
            wifi_pass = value;
        } else if (key == "MQTT_SERVER") {
            mqtt_server = value;
        } else if (key == "MQTT_PORT") {
            mqtt_port = value.toInt();
            if (mqtt_port == 0) mqtt_port = 1883; // Fallback bei ungültiger Zahl
        } else if (key == "MQTT_USER") {
            mqtt_user = value;
        } else if (key == "MQTT_PASS") {
            mqtt_pass = value;
        } else if (key == "MQTT_CLIENT_ID") {
            mqtt_client_id = value;
        } else if (key == "MQTT_BASE_TOPIC") {
            mqtt_base_topic = value;
        } else if (key == "MQTT_INTEGRATION") {
            if (value == "1") {
                mqtt_integration_enabled = true;
            } else {
                mqtt_integration_enabled = false;
            }
        }
    }
    configFile.close();

    if (mqtt_integration_enabled) {
        Serial.println("MQTT Integration: ENABLED");
        Serial.println("  SSID: " + wifi_ssid);
        // Serial.println("  Pass: [HIDDEN]"); // Passwort nicht ausgeben
        Serial.println("  MQTT Server: " + mqtt_server + ":" + String(mqtt_port));
        Serial.println("  MQTT User: " + mqtt_user);
        // Serial.println("  MQTT Pass: [HIDDEN]");
        Serial.println("  MQTT Client ID: " + mqtt_client_id);
        Serial.println("  MQTT Base Topic: " + mqtt_base_topic);

        // Dynamische Topics erstellen
        mqtt_topic_status = mqtt_base_topic + "/status";
        mqtt_topic_error = mqtt_base_topic + "/error";
        mqtt_topic_debug = mqtt_base_topic + "/debug";
        mqtt_topic_volume = mqtt_base_topic + "/volume";
        mqtt_topic_directory = mqtt_base_topic + "/directory";
        mqtt_topic_playing = mqtt_base_topic + "/playing";
        mqtt_topic_ip = mqtt_base_topic + "/ip_address";
    } else {
         Serial.println("MQTT Integration: DISABLED (config flag not '1' or missing)");
    }
}

// --- Funktion zum Aufbau der WLAN-Verbindung ---
void setup_wifi() {
    if (!mqtt_integration_enabled) {
        return; // Integration nicht aktiviert
    }

    if (wifi_ssid == "") {
        Serial.println("ERROR: WiFi SSID not configured in config.txt!");
        mqtt_integration_enabled = false; // Deaktivieren, wenn SSID fehlt
        return;
    }

    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(wifi_ssid);

    WiFi.mode(WIFI_STA); // Station Mode
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) { // 15 Sek Timeout
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nERROR: WiFi connection failed!");
        // Optional: Hier weitere Versuche oder Fehlermeldung über MQTT (falls Broker erreichbar ist)
        // Fürs Erste deaktivieren wir MQTT, wenn WLAN fehlschlägt
        // mqtt_integration_enabled = false; // Oder man lässt es und versucht später erneut zu verbinden
    } else {
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
    }
}

// --- MQTT Hilfsfunktion zum Publizieren ---
void publishMqtt(const String& topic, const String& payload, bool retain = false) {
    if (!mqtt_integration_enabled || !mqttClient.connected()) {
        // Nicht senden, wenn deaktiviert oder nicht verbunden
        return;
    }
    // Serial.printf("MQTT Publish: [%s] %s\n", topic.c_str(), payload.c_str()); // Debug
    mqttClient.publish(topic.c_str(), payload.c_str(), retain);
}
// -----------------------------------------------

// --- MQTT Wiederverbindungslogik ---
void mqtt_reconnect() {
    // Nur erneut versuchen, wenn Intervall abgelaufen ist
    if (millis() - lastMqttReconnectAttempt < mqttReconnectInterval) {
         return;
    }
    lastMqttReconnectAttempt = millis();

    if (!mqttClient.connected()) {
        Serial.print("Attempting MQTT connection...");
        // Versuche zu verbinden
        bool connected = false;
        if (mqtt_user.length() > 0) {
             connected = mqttClient.connect(mqtt_client_id.c_str(), mqtt_user.c_str(), mqtt_pass.c_str());
        } else {
             connected = mqttClient.connect(mqtt_client_id.c_str());
        }

        if (connected) {
            Serial.println("connected");
            // Sende IP-Adresse (retained) nach erfolgreicher Verbindung
            publishMqtt(mqtt_topic_ip, WiFi.localIP().toString(), true);
            publishMqtt(mqtt_topic_status, "Online", true); // Status setzen
             // Hier könnten ggf. Subscriptions erfolgen, falls benötigt
             // mqttClient.subscribe("some/topic");
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again in 5 seconds");
            // Wartezeit wird durch die Prüfung am Anfang von reconnect sichergestellt
        }
    }
}

// --- Funktion zum Finden von Verzeichnissen mit MP3s --- 
void findMp3Directories(File dir) {
    // ... (Code unverändert) ...
        while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (entry.isDirectory()) {
            File subdir = SD.open(entry.path());
            bool hasMp3 = false;
            if(subdir) {
                while(true) {
                    File subEntry = subdir.openNextFile();
                    if(!subEntry) break;
                    String subName = String(subEntry.name());
                    subName.toLowerCase();
                    if (!subEntry.isDirectory() && subName.endsWith(".mp3")) {
                        hasMp3 = true;
                        subEntry.close();
                        break;
                    }
                    subEntry.close();
                }
                subdir.close();
            }
            if (hasMp3) {
                directoryList.push_back(String(entry.path()));
                Serial.println("Found MP3 directory: " + String(entry.path()));
            }
        }
        entry.close();
    }
}

// --- Funktion zum Laden der MP3-Dateien aus dem aktuellen Verzeichnis --- (
void loadFilesFromCurrentDirectory() {
    // ... (Code unverändert, nutzt jetzt den potenziell geladenen currentDirectoryIndex) ...
    currentMp3Files.clear();
    // Zusätzliche Sicherheitsprüfung hier, falls Index trotz Validierung in Setup ungültig wäre
    if (currentDirectoryIndex < 0 || currentDirectoryIndex >= directoryList.size()) {
         Serial.printf("Warning: loadFiles called with invalid index %d. List size %d.\n", currentDirectoryIndex, directoryList.size());
         // Optional: Auf Index 0 zurücksetzen, falls Liste nicht leer ist
         if (!directoryList.empty()) currentDirectoryIndex = 0;
         else return; // Nichts zu laden
    }

    String currentDirPath = directoryList[currentDirectoryIndex];
    Serial.println("Loading files from: " + currentDirPath);
    File dir = SD.open(currentDirPath);
    if (!dir || !dir.isDirectory()) {
        Serial.println("Error opening directory: " + currentDirPath);
        if(dir) dir.close();
        return;
    }
    while (true) {
        File entry = dir.openNextFile();
        if (!entry) break;
        if (!entry.isDirectory()) {
            String fileName = String(entry.name());
            String lowerCaseFileName = fileName;
            lowerCaseFileName.toLowerCase();
            if (lowerCaseFileName.endsWith(".mp3") && !lowerCaseFileName.endsWith(introFileName)) {
                 currentMp3Files.push_back(fileName);
            }
        }
        entry.close();
    }
    dir.close();
    Serial.printf("  Found %d playable MP3 files (excluding intro).\n", currentMp3Files.size());
}

// --- Audio Callback bei Dateiende --- 
void audio_eof_mp3(const char *info) {
    Serial.println("\n>>> audio_eof_mp3 called <<<");
    Serial.print("eof_mp3 info: "); Serial.println(info);
    playing = false;
    publishMqtt(mqtt_topic_playing, "STOPPED (EOF)"); // Status senden

    if (playingIntro) {
        playingIntro = false;
        Serial.println(">>> Intro playback finished. Ready for PIR. <<<");
        publishMqtt(mqtt_topic_status, "Intro Finished"); // Status senden
    } else if (inPlaybackSession) {
        inPlaybackSession = false;
        playbackStartTime = 0;
        Serial.println(">>> Random file finished. Ready for PIR. <<<");
        publishMqtt(mqtt_topic_status, "Random File Finished"); // Status senden
    } else {
         Serial.println(">>> EOF occurred unexpectedly. <<<");
         publishMqtt(mqtt_topic_debug, "Unexpected EOF"); // Debug senden
    }
     Serial.printf(">>> audio_eof_mp3 end state: playing=%d, playingIntro=%d, inPlaybackSession=%d <<<\n\n",
                  playing, playingIntro, inPlaybackSession);
}

// --- Lautstärke prüfen --- 
void checkVolumePot() {
    unsigned long currentTime = millis();
    if (currentTime - lastPotReadTime >= potReadInterval) {
        lastPotReadTime = currentTime;
        int rawPotValue = analogRead(POT_PIN);
        if (smoothedPotValue < 0.0) { smoothedPotValue = (float)rawPotValue; }
        else { smoothedPotValue = potSmoothingFactor * (float)rawPotValue + (1.0 - potSmoothingFactor) * smoothedPotValue; }
        int currentVolume = map((int)smoothedPotValue, 0, 4095, 0, 21);

        if (currentVolume != lastVolume) {
            audio.setVolume(currentVolume);
            int oldVolume = lastVolume; // Merken für die Ausgabe
            lastVolume = currentVolume;
            Serial.print("Volume set to: "); Serial.print(currentVolume);
            Serial.print(" (Raw: "); Serial.print(rawPotValue); Serial.print(", Smoothed: "); Serial.print(smoothedPotValue, 1); Serial.println(")");
            publishMqtt(mqtt_topic_volume, String(currentVolume)); // Lautstärke senden
        }
    }
}

// --- Taster prüfen --- 
void checkButton() {
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) { lastDebounceTime = millis(); }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;
            if (buttonState == LOW) {
                lastPirActivityTime = millis();
                Serial.println("\n--- Button pressed! Changing directory ---");
                publishMqtt(mqtt_topic_status, "Button Pressed"); // Status senden
                if (playing || playingIntro || inPlaybackSession) {
                    audio.stopSong(); playing = false; playingIntro = false; inPlaybackSession = false;
                    Serial.println("Stopped current playback.");
                    publishMqtt(mqtt_topic_playing, "STOPPED (Button)"); // Status senden
                }
                if (!directoryList.empty()) {
                    currentDirectoryIndex = (currentDirectoryIndex + 1) % directoryList.size();
                    String newDirPath = directoryList[currentDirectoryIndex];
                    Serial.println("Selected directory: " + newDirPath);
                    publishMqtt(mqtt_topic_directory, newDirPath, true); // Verzeichnis senden (retained)
                    loadFilesFromCurrentDirectory();
                    String introPath = newDirPath + "/" + introFileName;
                    if (audio.connecttoFS(SD, introPath.c_str())) {
                        playing = true; playingIntro = true;
                        Serial.println("Playing intro: " + introPath);
                        publishMqtt(mqtt_topic_playing, introPath); // Intro senden
                        publishMqtt(mqtt_topic_status, "Playing Intro"); // Status senden
                    } else {
                        Serial.println("intro.mp3 not found/error in " + newDirPath);
                         publishMqtt(mqtt_topic_error, "intro.mp3 not found/error"); // Fehler senden
                        playingIntro = false;
                    }
                    preferences.begin("appState", false);
                    preferences.putInt("dirIndex", currentDirectoryIndex);
                    preferences.end();
                } else {
                    Serial.println("No directories found.");
                    publishMqtt(mqtt_topic_error, "Button pressed, but no directories"); // Fehler senden
                 }
                Serial.println("--- Button action complete ---");
            }
        }
    }
    lastButtonState = reading;
}
// --- Setup --- 
void setup() {
    pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);
    pinMode(POT_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200); Serial.println("\nStarting: Directory MP3 Player V_F (MQTT)...");

    lastPirActivityTime = millis();
    randomSeed(analogRead(0));
    spi_onboardSD->begin();

    Serial.println("Init SD...");
    if (!SD.begin(SD_CS_PIN, *spi_onboardSD)) { Serial.println("SD FAIL!"); while (true); }
    Serial.println("SD OK."); digitalWrite(LED_BUILTIN, HIGH);

    // --- Konfiguration laden ---
    loadConfig();

    // --- WLAN verbinden (wenn aktiviert) ---
    setup_wifi();
 
    // --- MQTT Setup (wenn aktiviert) ---
    if (mqtt_integration_enabled) {
        if (mqtt_server != "") {
            mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
            // Optional: Callback für eingehende Nachrichten setzen
            // mqttClient.setCallback(mqttCallback);
            Serial.println("MQTT Server configured.");
        } else {
             Serial.println("WARNING: MQTT Server not configured in config.txt, disabling MQTT.");
             mqtt_integration_enabled = false; // Deaktivieren wenn Server fehlt
        }
    }

    Serial.println("Scanning directories...");
    File root = SD.open("/"); if (!root) { Serial.println("ROOT FAIL!"); while (true); }
    directoryList.clear(); findMp3Directories(root); root.close();

    if (directoryList.empty()) {
        Serial.println("NO MP3 DIRS FOUND!");
        publishMqtt(mqtt_topic_error, "No MP3 directories found on SD", true); // Fehler auch per MQTT
        currentDirectoryIndex = -1;
    } else {
        Serial.printf("Found %d directories.\n", directoryList.size());
        preferences.begin("appState", true);
        currentDirectoryIndex = preferences.getInt("dirIndex", 0);
        lastVolume = preferences.getInt("volume", 15);
        preferences.end();
        Serial.printf("NVS Loaded: DirIndex=%d, LastVolume=%d\n", currentDirectoryIndex, lastVolume);
        if (currentDirectoryIndex < 0 || currentDirectoryIndex >= directoryList.size()) {
             Serial.printf("Warning: Loaded directory index %d out of bounds. Resetting to 0.\n", currentDirectoryIndex);
             publishMqtt(mqtt_topic_error, "Loaded directory index out of bounds, reset to 0");
             currentDirectoryIndex = 0;
        }
        loadFilesFromCurrentDirectory();
        // Verzeichnis auch per MQTT publizieren (retained)
        if (currentDirectoryIndex >= 0) {
            publishMqtt(mqtt_topic_directory, directoryList[currentDirectoryIndex], true);
        }
    }

    audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
    lastVolume = constrain(lastVolume, 0, 21);
    audio.setVolume(lastVolume);
    publishMqtt(mqtt_topic_volume, String(lastVolume), true); // Volume auch per MQTT (retained)
    Serial.printf("Initial Volume set to %d\n", lastVolume);

    lastButtonState = digitalRead(BUTTON_PIN); buttonState = lastButtonState;

    Serial.println("Setup complete.");
    publishMqtt(mqtt_topic_status, "Initialized", true); // Bereit-Status
    Serial.println("Waiting for PIR or button...");
}

// --- Loop --- (Speichert Zustand vor Deep Sleep)
void loop() {
    // --- MQTT Verbindungs-Handling ---
    if (mqtt_integration_enabled && WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected()) {
            mqtt_reconnect(); // Versuche zu verbinden/wiederzuverbinden
        }
        mqttClient.loop(); // MQTT Client am Leben halten
    } else if (mqtt_integration_enabled && WiFi.status() != WL_CONNECTED) {
        // Optional: WLAN Reconnect versuchen, wenn Verbindung verloren geht
        // setup_wifi(); // Vorsicht: Kann blockieren
        // Besser: Nicht-blockierender Reconnect oder einfach warten
        static unsigned long lastWifiCheck = 0;
        if (millis() - lastWifiCheck > 30000) { // Prüfe alle 30 Sek
            Serial.println("WiFi disconnected. Attempting reconnect...");
            WiFi.disconnect();
            WiFi.reconnect();
            lastWifiCheck = millis();
        }
    }

    checkVolumePot(); // Liest Poti, aktualisiert ggf. lastVolume, sendet MQTT
    checkButton();    // Wechselt Verzeichnis, spielt Intro, sendet MQTT

    bool pirStateHigh = (digitalRead(PIR_PIN) == HIGH);

    // --- Inaktivitäts-Timer aktualisieren ---
    if (pirStateHigh || playing) {
        lastPirActivityTime = millis();
    }

    // --- PIR Trigger Logik --- 
    if (!inPlaybackSession && !playingIntro) {
         if (pirStateHigh) {
            lastPirActivityTime = millis();
            Serial.println("\n+++ PIR TRIGGER +++");
            publishMqtt(mqtt_topic_status, "PIR Triggered"); // Status senden
             if (currentDirectoryIndex >= 0 && !currentMp3Files.empty()) {
                 inPlaybackSession = true;
                 playbackStartTime = millis();
                 int randomIndex = random(currentMp3Files.size());
                 String randomFileName = currentMp3Files[randomIndex];
                 String currentDirPath = directoryList[currentDirectoryIndex];
                 String fileToPlay = currentDirPath + "/" + randomFileName;
                 Serial.print("Playing random file: "); Serial.println(fileToPlay);
                 publishMqtt(mqtt_topic_playing, fileToPlay); // Datei senden
                 if (audio.connecttoFS(SD, fileToPlay.c_str())) {
                      playing = true;
                      publishMqtt(mqtt_topic_status, "Playing Random"); // Status senden
                 } else {
                      Serial.println("ERROR playing file!");
                      publishMqtt(mqtt_topic_error, "ERROR playing file: " + fileToPlay); // Fehler senden
                      publishMqtt(mqtt_topic_playing, "STOPPED"); // Status korrigieren
                      playing = false; inPlaybackSession = false;
                 }
                 Serial.println("+++ PIR action complete +++\n");
             } else {
                  Serial.println("PIR detected, but no random files/directory selected.");
                  publishMqtt(mqtt_topic_debug, "PIR detected, no files/dir"); // Debug senden
             }
         }
    }

    // --- Timeout Logik für lange Dateien --- 
    if (inPlaybackSession && !playingIntro) {
        unsigned long elapsedTime = millis() - playbackStartTime;
        if (elapsedTime >= maxPlaybackDuration) {
            Serial.println("\n!!! PIR playback session timeout (5 min limit) !!!");
            publishMqtt(mqtt_topic_status, "Timeout Reached"); // Status senden
            if (playing) {
                 audio.stopSong(); playing = false;
                 Serial.println("Audio stopped due to timeout.");
                 publishMqtt(mqtt_topic_playing, "STOPPED (Timeout)"); // Status senden
            }
            inPlaybackSession = false;
            playbackStartTime = 0;
            Serial.println("Timeout ended. Ready for new motion detection.\n");
        }
    }


    // --- Deep Sleep Check --- 
    if (!playing && !playingIntro && !inPlaybackSession) {
        unsigned long inactivityDuration = millis() - lastPirActivityTime;
        if (inactivityDuration >= deepSleepInactivityTimeout) {
            preferences.begin("appState", false);
           // preferences.putInt("dirIndex", currentDirectoryIndex); //wird bereits beim Verzeichniswechsel gemacht
            preferences.putInt("volume", lastVolume);
            preferences.end();
            Serial.printf("\n--- Saved state to NVS: DirIndex=%d, Volume=%d ---\n", currentDirectoryIndex, lastVolume);

            Serial.println("--- inactivity detected. Entering deep sleep. ---");
            publishMqtt(mqtt_topic_status, "Entering Deep Sleep", true); // Letzten Status senden (retained)
            Serial.println("Will wake up when PIR pin goes HIGH.");
            Serial.flush();
             // Kurze Wartezeit, damit MQTT-Nachricht Zeit hat, gesendet zu werden
             unsigned long mqttSentTime = millis();
             while(mqttClient.loop() && (millis() - mqttSentTime < 100)) { delay(10); } // Max 100ms warten

            audio.stopSong();

            esp_sleep_enable_ext0_wakeup((gpio_num_t)PIR_PIN, 1);
            esp_deep_sleep_start();
        }
    }

    audio.loop();
    vTaskDelay(1);
}
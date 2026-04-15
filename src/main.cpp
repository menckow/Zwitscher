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
#include <WiFiClientSecure.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

const char* FW_VERSION = "7.0.0";
#include <ESPAsyncWebServer.h> // For Webserver
#include <AsyncTCP.h>          // For Webserver
#include <DNSServer.h>         // For Captive Portal
#include "freertos/semphr.h"
#include "Config.h"
#include "MqttHandler.h"
#include "LedController.h"
#include "WebManager.h"
#include "GlobalConfig.h"
#include "AudioHardware.h"

SemaphoreHandle_t neoPixelMutex;

// --- Webserver Objects ---
AsyncWebServer server(80);
DNSServer dns;
bool apMode = false; // Flag to indicate if we are in AP mode
bool pendingRestart = false;
unsigned long restartTime = 0;


// ------------------------------------

#define MAX_PATH_DEPTH 1

// --- Pin-Definitionen ---
const int POT_PIN = 4;
const int PIR_PIN = 18;
const int SD_CS_PIN = SS;
const int BUTTON_PIN = 17; //38
const int LED_PIN = 16;
const int DEFAULT_LED_COUNT = 16;
int led_count = DEFAULT_LED_COUNT;

Adafruit_NeoPixel strip(DEFAULT_LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

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
String admin_pass = ""; // Webinterface Password
String mqtt_server = "";
int    mqtt_port = 1883; // Standard-Port
String mqtt_user = "";
String mqtt_pass = "";
String mqtt_client_id = "ESP32_AudioPlayer"; // Default, falls Laden fehlschlägt
String mqtt_base_topic = "audioplayer";      // Default, falls Laden fehlschlägt
bool   homeassistant_mqtt_enabled = false; // Für die Statusmeldung an Home Assistant
bool   friendlamp_mqtt_enabled = false;    // Für die Freundschaftslampen-Funktionalität

// NEU: TLS Variablen
bool   friendlamp_mqtt_tls_enabled = false;
String mqtt_root_ca_content = "";

// Freundschaftslampe MQTT Variablen (Optionaler 2. Broker)
String friendlamp_mqtt_server = "";
int    friendlamp_mqtt_port = 1883;
String friendlamp_mqtt_user = "";
String friendlamp_mqtt_pass = "";

// Freundschaftslampe Variablen
bool   friendlamp_enabled = false;
int    fadeDuration = 1000;
bool   led_fade_effect = false;
int    led_brightness = 100; // Helligkeit (0-255), Standardwert
String friendlamp_color = "0000FF";
String friendlamp_topic = "freundschaft/farbe";
String zwitscherbox_topic = "zwitscherbox/farbe";
unsigned long ledTimeout = 0;
uint32_t currentLedColor = 0;
bool ledActive = false;

// Dynamisch erstellte MQTT Topics
String mqtt_topic_status;
String mqtt_topic_error;
String mqtt_topic_debug;    // Optional für detailliertere Infos
String mqtt_topic_volume;
String mqtt_topic_directory;
String mqtt_topic_playing;
String mqtt_topic_ip;

// MQTT Client Objekte (Interner Broker)

// MQTT Client Objekte (Freundschaftslampe Broker)


// --- Globale Variablen ---
std::vector<String> directoryList;
std::vector<String> currentMp3Files;
int currentDirectoryIndex = -1; // Wird jetzt aus NVS geladen
String introFileName = "intro.mp3";

bool playing = false;
bool inPlaybackSession = false;
bool playingIntro = false;
unsigned long playbackStartTime = 0;
bool isStandby = false; // Verhindert mehrfaches Ausführen der Standby-Aktion

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

/// --- LED Fade-Effekte ---
// --- Funktion zum Laden der Konfiguration von SD ---

// --- Webserver & Config Portal ---


// --- MQTT Hilfsfunktion zum Publizieren ---


// --- Funktion zum Aufbau der WLAN-Verbindung ---


// --- MQTT Callbacks ---


// -----------------------------------------------

// --- MQTT Wiederverbindungslogik ---

// --- Funktion zum Finden von Verzeichnissen mit MP3s ---

// --- Funktion zum Laden der MP3-Dateien aus dem aktuellen Verzeichnis ---

// --- Audio Callback bei Dateiende --- 

// --- Lautstärke prüfen --- 

// --- Taster prüfen --- 
// --- Setup --- 
void setup() {
    neoPixelMutex = xSemaphoreCreateMutex();
    pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);
    pinMode(POT_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200); Serial.println("\nStarting: Directory MP3 Player V_F (MQTT)...");

    strip.begin(); 
    strip.setBrightness(100); // Heller kurzer Blitz zum Startup
    strip.fill(strip.Color(255, 255, 255));
    strip.show();
    delay(200);
    strip.clear();
    strip.setBrightness(50); // Zurück auf Normalhelligkeit für Status-LEDs
    strip.show();

    lastPirActivityTime = millis();
    randomSeed(esp_random());
    spi_onboardSD->begin();

    Serial.println("Init SD...");
    if (!SD.begin(SD_CS_PIN, *spi_onboardSD)) {
        setBootStatusLeds(1, false);
        Serial.println("SD FAIL!"); 
        delay(3000);
        while (true); 
    }
    setBootStatusLeds(1, true);
    Serial.println("SD OK."); digitalWrite(LED_BUILTIN, HIGH);

    // --- Konfiguration laden ---
    loadConfig();

    // --- WLAN verbinden (wenn eine der Integrationen aktiviert ist) ---
    if (homeassistant_mqtt_enabled || friendlamp_mqtt_enabled) {
        setup_wifi();
    }
 
    // --- MQTT Setup für Home Assistant ---
    if (homeassistant_mqtt_enabled) {
        if (mqtt_server != "") {
            mqttClient.setServer(mqtt_server.c_str(), mqtt_port);
            mqttClient.setCallback(mqttCallback);
            Serial.println("Internal MQTT (Home Assistant) Server configured.");
        } else {
             Serial.println("WARNING: Internal MQTT Server not configured, disabling HA integration.");
             homeassistant_mqtt_enabled = false;
        }
    }

    // --- MQTT Setup für Freundschaftslampe ---
    if (friendlamp_mqtt_enabled && friendlamp_enabled && friendlamp_mqtt_server != "") {
        // Unterscheidung der Verschlüsselung: Nur der Friendlamp-Server nutzt TLS, wenn aktiviert
        if (friendlamp_mqtt_tls_enabled) {
            if (mqtt_root_ca_content.length() > 20) {
                espClientSecureLamp.setCACert(mqtt_root_ca_content.c_str());
            } else {
                espClientSecureLamp.setInsecure(); // Fallback, falls kein Zertifikat vorhanden ist
            }
            mqttClientLamp.setClient(espClientSecureLamp);
        } else {
            mqttClientLamp.setClient(espClientLamp); // Standard unverschlüsselt
        }
        mqttClientLamp.setServer(friendlamp_mqtt_server.c_str(), friendlamp_mqtt_port);
        mqttClientLamp.setCallback(mqttLampCallback);
        Serial.println("Friendship Lamp MQTT Server configured.");
    }

    // --- LED_Ring Setup ---
    if (friendlamp_enabled) {
        // updateLength wird ans Ende des Setups verschoben, da es den LED Buffer sofort löscht!
        strip.setBrightness(led_brightness); // Set brightness from config
        Serial.println("Friendship Lamp (LED Ring) configured. Brightness: " + String(led_brightness));
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

    // --- Boot Status abwarten und LEDs prüfen ---
    if ((homeassistant_mqtt_enabled || friendlamp_mqtt_enabled) && WiFi.status() == WL_CONNECTED) {
        mqtt_reconnect(); // Trigger initial MQTT connect to show LED 1 status
    }
    
    // Lass die Boot-Status LEDs für 2 Sekunden leuchten, um sie zu überprüfen
    delay(2000); 

    // --- LED_Ring finale Einstellung nach Boot Sequence ---
    if (friendlamp_enabled && led_count > 0 && led_count != DEFAULT_LED_COUNT) {
        strip.updateLength(led_count);
    }
    
    if (!apMode) {
        strip.clear(); 
        strip.show();
    } else {
        // Falls updateLength die LEDs gelöscht hat, setzen wir sie für den AP-Modus wieder rot
        setApModeLed(true);
    }

    Serial.println("Setup complete.");
    if (homeassistant_mqtt_enabled && mqttClient.connected()) publishMqtt(mqtt_topic_status, "Initialized", true); // Bereit-Status
    Serial.println("Waiting for PIR or button...");
}

// --- Loop --- (Speichert Zustand vor Deep Sleep)
void loop() {
    
    // NEU: Verzögerter Neustart nachdem eine Konfiguration gespeichert wurde
    if (pendingRestart) {
        if (millis() > restartTime) {
            ESP.restart();
        }
        return; // Im Warte-Zustand soll das System alles andere ignorieren
    }
    if (apMode) {
        dns.processNextRequest();
        return; // Im AP-Modus nichts anderes tun
    }
    updateFade();
    // --- MQTT Verbindungs-Handling ---
    if ((homeassistant_mqtt_enabled || friendlamp_mqtt_enabled) && WiFi.status() == WL_CONNECTED) {
        // Versuche zu verbinden/wiederzuverbinden (beide Broker)
        mqtt_reconnect(); 
        
        if (homeassistant_mqtt_enabled && mqttClient.connected()) {
            mqttClient.loop(); // Interner MQTT Client am Leben halten
        }
        if (friendlamp_mqtt_enabled && friendlamp_enabled && friendlamp_mqtt_server != "" && mqttClientLamp.connected()) {
            mqttClientLamp.loop(); // Lampen MQTT Client am Leben halten
        }
    } else if ((homeassistant_mqtt_enabled || friendlamp_mqtt_enabled) && WiFi.status() != WL_CONNECTED) {
        // Optional: WLAN Reconnect versuchen, wenn Verbindung verloren geht
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
            if (isStandby) {
                isStandby = false;
                Serial.println("Woke up from Standby (PIR)");
                publishMqtt(mqtt_topic_status, "Woke up from Standby");
            }
            Serial.println("\n+++ PIR TRIGGER +++");
            publishMqtt(mqtt_topic_status, "PIR Triggered"); // Status senden

            // Freundschaftslampe: Sende Signal und leuchte auf
            if (friendlamp_enabled) {
                JsonDocument doc;
                doc["client_id"] = mqtt_client_id;
                doc["color"] = friendlamp_color;
                doc["effect"] = friendlamp_color.equalsIgnoreCase("RAINBOW") ? "rainbow" : "fade";
                doc["duration"] = 30000; // 30 sec standard duration
                String payload;
                serializeJson(doc, payload);
                publishMqttLamp(zwitscherbox_topic, payload, false);
                Serial.println("Zwitscherbox: Sent JSON to topic " + zwitscherbox_topic + ": " + payload);
            }

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


    // --- Standby Check (Ersetzt Deep Sleep) --- 
    if (!playing && !playingIntro && !inPlaybackSession && !isStandby) {
        unsigned long inactivityDuration = millis() - lastPirActivityTime;
        if (inactivityDuration >= deepSleepInactivityTimeout) {
            preferences.begin("appState", false);
            preferences.putInt("volume", lastVolume);
            preferences.end();
            Serial.printf("\n--- Saved state to NVS: DirIndex=%d, Volume=%d ---\n", currentDirectoryIndex, lastVolume);

            Serial.println("--- Inactivity detected. Entering Standby Mode. ---");
            publishMqtt(mqtt_topic_status, "Entering Standby", true); // Letzten Status senden (retained)
            Serial.println("WLAN & MQTT remain active. Awaiting PIR or MQTT messages.");
            
            audio.stopSong();

            // LEDs ausblenden, wenn in den Standby-Modus gewechselt wird
            if (friendlamp_enabled) {
                startFadeOut();
            }

            // Setzen des Standby-Flags
            isStandby = true;
        }
    }

    if (friendlamp_enabled) {
        if (ledActive && millis() > ledTimeout) {
            ledActive = false;
            startFadeOut();
        }
    }

    audio.loop();
    vTaskDelay(1);
}
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
#include "LedController.h"
#include "WebManager.h"
#include "GlobalConfig.h"

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
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;

// MQTT Client Objekte (Freundschaftslampe Broker)
WiFiClient espClientLamp;
WiFiClientSecure espClientSecureLamp; // NEU für TLS
PubSubClient mqttClientLamp(espClientLamp);
unsigned long lastLampMqttReconnectAttempt = 0;

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
void handleFreundschaftMessage(String payload) {
    Serial.println("--> handleFreundschaftMessage: Received " + payload);

    String colorStr = "";
    String effect = "fade";
    long duration = 30000;
    String senderId = "";
    uint32_t parsedColor = 0;
    bool hasLegacyRGB = false;

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (!error && !doc["client_id"].isNull()) {
        senderId = doc["client_id"] | "";
        colorStr = doc["color"] | "";
        effect = doc["effect"] | "fade";
        duration = doc["duration"] | 30000;
    } else {
        if (payload.equalsIgnoreCase("RAINBOW")) {
            colorStr = "RAINBOW";
        } else {
            // Legacy "r,g,b"
            int firstComma = payload.indexOf(',');
            int secondComma = payload.lastIndexOf(',');
            if (firstComma != -1 && secondComma != -1 && firstComma != secondComma) {
                int r = payload.substring(0, firstComma).toInt();
                int g = payload.substring(firstComma + 1, secondComma).toInt();
                int b = payload.substring(secondComma + 1).toInt();
                parsedColor = strip.Color(r, g, b);
                hasLegacyRGB = true;
            } else {
                // Legacy "Sender:HEX" or raw HEX
                int colonPos = payload.indexOf(':');
                if (colonPos != -1) {
                    senderId = payload.substring(0, colonPos);
                    colorStr = payload.substring(colonPos + 1);
                } else {
                    colorStr = payload;
                }
            }
        }
    }

    if (senderId.length() > 0 && senderId == mqtt_client_id) {
        Serial.println("--> handleFreundschaftMessage: Ignored own message.");
        return;
    }

    bool isRainbow = effect.equalsIgnoreCase("rainbow") || colorStr.equalsIgnoreCase("RAINBOW");
    bool isBlink = effect.equalsIgnoreCase("blink");
    
    uint32_t finalColor = 0;
    if (isRainbow) {
        finalColor = 0;
    } else if (hasLegacyRGB) {
        finalColor = parsedColor;
    } else {
        if (colorStr.startsWith("#")) colorStr = colorStr.substring(1);
        finalColor = strtol(colorStr.c_str(), NULL, 16);
    }
    
    Serial.printf("--> handleFreundschaftMessage: Activated 3rd LED mode. final value: %u\n", finalColor);

    startFadeIn(finalColor, 1, isRainbow, isBlink);
    ledTimeout = millis() + duration;
    ledActive = true; 
}
// --- Funktion zum Laden der Konfiguration von SD ---

// --- Webserver & Config Portal ---


// --- MQTT Hilfsfunktion zum Publizieren ---
void publishMqtt(const String& topic, const String& payload, bool retain = false) {
    if (!homeassistant_mqtt_enabled || !mqttClient.connected()) {
        // Nicht senden, wenn deaktiviert oder nicht verbunden
        return;
    }
    mqttClient.publish(topic.c_str(), payload.c_str(), retain);
}

void publishMqttLamp(const String& topic, const String& payload, bool retain = false) {
    if (!friendlamp_mqtt_enabled || !friendlamp_enabled) return;
    
    // Wenn ein separater Server konfiguriert ist, nutze diesen, ansonsten den internen
    if (friendlamp_mqtt_server != "") {
        if (mqttClientLamp.connected()) {
            mqttClientLamp.publish(topic.c_str(), payload.c_str(), retain);
        }
    } else {
        // Nutze den internen Broker, wenn für HA aktiviert
        if (homeassistant_mqtt_enabled && mqttClient.connected()) {
            mqttClient.publish(topic.c_str(), payload.c_str(), retain);
        }
    }
}

// --- Funktion zum Aufbau der WLAN-Verbindung ---
void setup_wifi() {
    if (!homeassistant_mqtt_enabled && !friendlamp_mqtt_enabled) {
        WiFi.mode(WIFI_OFF);
        Serial.println("All MQTT integrations disabled. WiFi is OFF.");
        return; 
    }

    if (wifi_ssid == "") {
        Serial.println("WiFi SSID not configured. Starting Config Portal.");
        startConfigPortal();
        return;
    }

    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(wifi_ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) { // 15 Sek Timeout
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi connection failed! Starting Config Portal.");
        WiFi.disconnect(true);
        startConfigPortal();
    } else {
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        publishMqtt(mqtt_topic_ip, WiFi.localIP().toString(), true);

        // --- Zusätzliche Ausgabe für den Benutzer ---
        Serial.println("----------------------------------------");
        Serial.println("Netzwerk & MQTT Status:");
        Serial.println("  WLAN (SSID): " + wifi_ssid);
        Serial.println("  IP Adresse:  " + WiFi.localIP().toString());
        if (homeassistant_mqtt_enabled) {
            Serial.println("  HA MQTT:     " + mqtt_server + ":" + String(mqtt_port));
        }
        if (friendlamp_mqtt_enabled && friendlamp_mqtt_server != "") {
            Serial.println("  Friend-MQTT: " + friendlamp_mqtt_server + ":" + String(friendlamp_mqtt_port));
        }
        Serial.println("----------------------------------------");
        
        // Webserver auch im normalen WLAN-Modus unter der lokalen IP-Adresse starten
        setupWebServer();
    }
}

void performOtaUpdate(const char* url, const char* version) {
    Serial.println("OTA Update Prozess gestartet...");
    Serial.printf("Update-URL: %s\n", url);

    if (playing || playingIntro || inPlaybackSession) {
        audio.stopSong();
        playing = false; playingIntro = false; inPlaybackSession = false;
    }

    String statusTopic = "zwitscherbox/status/" + mqtt_client_id;
    String startMsg = "Updating to " + String(version);
    
    // Status an beide Broker (falls verbunden)
    if (homeassistant_mqtt_enabled && mqttClient.connected()) {
        mqttClient.publish(statusTopic.c_str(), (String(FW_VERSION) + ":" + startMsg).c_str(), true);
        mqttClient.publish("zwitscherbox/update/status", startMsg.c_str());
    }
    if (friendlamp_mqtt_enabled && friendlamp_mqtt_server != "" && mqttClientLamp.connected()) {
        mqttClientLamp.publish(statusTopic.c_str(), (String(FW_VERSION) + ":" + startMsg).c_str(), true);
        mqttClientLamp.publish("zwitscherbox/update/status", startMsg.c_str());
    }

    WiFiClientSecure otaClient;
    otaClient.setInsecure();

    if (friendlamp_enabled) {
        for (int i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, strip.Color(0, 0, 255));
        }
        strip.show();
    }

    httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

    httpUpdate.onProgress([](int cur, int total) {
        static int lastPercent = -1;
        int percent = (cur * 100) / total;
        if (percent % 10 == 0 && percent != lastPercent) {
            lastPercent = percent;
            Serial.printf("OTA Download: %d%%\n", percent);
        }
    });

    t_httpUpdate_return ret = httpUpdate.update(otaClient, url);

    switch (ret) {
        case HTTP_UPDATE_FAILED: {
            String errorMsg = "V" + String(FW_VERSION) + ":" + String(mqtt_client_id) + " - Update failed: " + httpUpdate.getLastErrorString();
            Serial.println(errorMsg);
            if (homeassistant_mqtt_enabled && mqttClient.connected()) mqttClient.publish(statusTopic.c_str(), errorMsg.c_str(), false);
            if (friendlamp_mqtt_enabled && friendlamp_mqtt_server != "" && mqttClientLamp.connected()) mqttClientLamp.publish(statusTopic.c_str(), errorMsg.c_str(), false);
            if (friendlamp_enabled) {
                for (int i = 0; i < strip.numPixels(); i++) {
                    strip.setPixelColor(i, strip.Color(255, 0, 0));
                }
                strip.show();
                delay(2000);
                strip.clear();
                strip.show();
            }
            break;
        }
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("Keine Updates verfügbar.");
            break;
        case HTTP_UPDATE_OK: {
            Serial.println("Update erfolgreich! ESP32 startet neu...");
            String okMsg = "V" + String(FW_VERSION) + ":" + String(mqtt_client_id) + " - Success! Rebooting...";
            if (homeassistant_mqtt_enabled && mqttClient.connected()) mqttClient.publish(statusTopic.c_str(), okMsg.c_str(), false);
            if (friendlamp_mqtt_enabled && friendlamp_mqtt_server != "" && mqttClientLamp.connected()) mqttClientLamp.publish(statusTopic.c_str(), okMsg.c_str(), false);
            delay(1000);
            ESP.restart();
            break;
        }
    }
}

// --- MQTT Callbacks ---
void handleLampMessage(char* topic, byte* payload, unsigned int length) {
    Serial.println("--> handleLampMessage: Function called!");
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.printf("Friendlamp MQTT Received [%s]: %s\n", topic, message.c_str());

    String otaTriggerTopic = "zwitscherbox/update/trigger";
    if (strcmp(topic, otaTriggerTopic.c_str()) == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (!error) {
            const char* url = doc["url"] | "";
            const char* version = doc["version"] | "";
            if (strlen(url) > 0 && strlen(version) > 0) {
                if (strcmp(version, FW_VERSION) != 0) {
                    performOtaUpdate(url, version);
                } else {
                    String statusTopic = "zwitscherbox/update/status";
                    String okMsg = "V" + String(FW_VERSION) + ":" + String(mqtt_client_id) + " - Already up to date";
                    if (homeassistant_mqtt_enabled && mqttClient.connected()) mqttClient.publish(statusTopic.c_str(), okMsg.c_str(), false);
                    if (friendlamp_mqtt_enabled && friendlamp_mqtt_server != "" && mqttClientLamp.connected()) mqttClientLamp.publish(statusTopic.c_str(), okMsg.c_str(), false);
                }
            }
        }
        return;
    }

    // Use strcmp for safe C-string comparison
    if (friendlamp_enabled && strcmp(topic, zwitscherbox_topic.c_str()) == 0) {
        Serial.println("--> handleLampMessage: Topic matches zwitscherbox_topic!");
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error && !doc["client_id"].isNull()) {
            String senderId = doc["client_id"] | "";
            String colorStr = doc["color"] | "";
            String effect = doc["effect"] | "fade";
            long duration = doc["duration"] | 30000;
            
            Serial.println("--> handleLampMessage (JSON): Sender ID is '" + senderId + "', my ID is '" + mqtt_client_id + "'");
            if (senderId != mqtt_client_id && senderId.length() > 0) {
                Serial.println("--> handleLampMessage (JSON): Sender ID is different, proceeding to light up LED.");
                bool isRainbow = effect.equalsIgnoreCase("rainbow") || colorStr.equalsIgnoreCase("RAINBOW");
                bool isBlink = effect.equalsIgnoreCase("blink");
                
                if (colorStr.startsWith("#")) colorStr = colorStr.substring(1);
                currentLedColor = isRainbow ? 0 : strtol(colorStr.c_str(), NULL, 16);
                ledTimeout = millis() + duration;
                ledActive = true;
                if (friendlamp_enabled) {
                    Serial.printf("--> handleLampMessage: Starting effect %s.\n", effect.c_str());
                    startFadeIn(currentLedColor, 0, isRainbow, isBlink);
                }
            } else {
                Serial.println("--> handleLampMessage: Ignored own message.");
            }
        } else {
            // Legacy Parsing: Erwartetes Format: "ClientID:ColorHEX" (z.B. "ESP32_Zwitscher11:05AB02")
            int separatorPos = message.indexOf(':');
            if (separatorPos != -1) {
                String senderId = message.substring(0, separatorPos);
                String colorStr = message.substring(separatorPos + 1);
                
                Serial.println("--> handleLampMessage (Legacy): Sender ID is '" + senderId + "', my ID is '" + mqtt_client_id + "'");
                // Reagiere nicht auf die eigene Nachricht
                if (senderId != mqtt_client_id) {
                    Serial.println("--> handleLampMessage (Legacy): Sender ID is different, proceeding to light up LED (Full).");
                    bool isRainbow = colorStr.equalsIgnoreCase("RAINBOW");
                    
                    if (colorStr.startsWith("#")) colorStr = colorStr.substring(1);
                    currentLedColor = isRainbow ? 0 : strtol(colorStr.c_str(), NULL, 16);
                    ledTimeout = millis() + 30000; // 30 Sekunden leuchten
                    ledActive = true;
                    if (friendlamp_enabled) {
                        Serial.println("--> handleLampMessage: Starting Fade-In.");
                        startFadeIn(currentLedColor, 0, isRainbow, false);
                    }
                    Serial.printf("Zwitscherbox: Received color %s from %s\n", colorStr.c_str(), senderId.c_str());
                } else {
                    Serial.println("--> handleLampMessage: Ignored own message.");
                }
            } else {
                Serial.println("--> handleLampMessage: Invalid format for Zwitscherbox. Expected Sender:HexColor or valid JSON.");
            }
        }
    } else if (friendlamp_enabled && strcmp(topic, friendlamp_topic.c_str()) == 0) {
        Serial.println("--> handleLampMessage: Topic matches Freundschaft!");
        handleFreundschaftMessage(message);
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // Wird für den internen Broker genutzt. 
    // Wir leiten alle internen Messages an handleLampMessage weiter, da diese
    // sowohl die OTA Updates als auch die Freundschaftslampe (falls aktiv) abwickelt.
    handleLampMessage(topic, payload, length);
}

void mqttLampCallback(char* topic, byte* payload, unsigned int length) {
    // Wird für den öffentlichen Broker genutzt
    handleLampMessage(topic, payload, length);
}
// -----------------------------------------------

// --- MQTT Wiederverbindungslogik ---
void mqtt_reconnect() {
    String statusTopic = "zwitscherbox/status/" + mqtt_client_id;
    String lwtMessage = "offline";

    // --- Wiederverbindung für den internen Broker (Home Assistant) ---
    if (homeassistant_mqtt_enabled && !mqttClient.connected() && millis() - lastMqttReconnectAttempt > mqttReconnectInterval) {
        lastMqttReconnectAttempt = millis();

        mqttClient.setClient(espClient); // Immer den ungesicherten Client verwenden

        Serial.print("Attempting Internal (HA) MQTT connection...");
        bool connected = false;
        if (mqtt_user.length() > 0) {
             connected = mqttClient.connect(mqtt_client_id.c_str(), mqtt_user.c_str(), mqtt_pass.c_str(),
                                          statusTopic.c_str(), 1, true, lwtMessage.c_str());
        } else {
             connected = mqttClient.connect(mqtt_client_id.c_str(), statusTopic.c_str(), 1, true, lwtMessage.c_str());
        }

        if (connected) {
            Serial.println("connected");
            publishMqtt(mqtt_topic_ip, WiFi.localIP().toString(), true);
            
            // Zentraler Status für das Dashboard (Retained)
            mqttClient.publish(statusTopic.c_str(), (String(FW_VERSION) + ":online").c_str(), true);
            publishMqtt(mqtt_topic_status, "Online", true); 
             // Immer OTA Update Trigger Topic abonnieren
             mqttClient.subscribe("zwitscherbox/update/trigger");
             mqttClient.publish("zwitscherbox/update/status", ("V" + String(FW_VERSION) + ":" + String(mqtt_client_id)).c_str(), false);
             
             // Subscribe auf dem internen Broker für die Lampe, falls diese auch dort läuft
             if (friendlamp_mqtt_enabled && friendlamp_enabled && friendlamp_mqtt_server == "") {
                 mqttClient.subscribe(friendlamp_topic.c_str());
                 mqttClient.subscribe(zwitscherbox_topic.c_str());
                 Serial.println("Subscribed to Friendlamp topics on internal broker.");
             }
        } else {
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again later");
        }
    }

    // --- Wiederverbindung für den Freundschaftslampen-Broker (falls konfiguriert) ---
    if (friendlamp_mqtt_enabled && friendlamp_enabled && friendlamp_mqtt_server != "" && !mqttClientLamp.connected()) {
        if (millis() - lastLampMqttReconnectAttempt > mqttReconnectInterval) {
            lastLampMqttReconnectAttempt = millis();
            Serial.print("Attempting Lamp MQTT connection...");
            bool connected = false;
            
            // Um Kollisionen zu vermeiden, fügen wir _Lamp zur Client-ID hinzu, falls sie nicht einzigartig ist
            String lampClientId = mqtt_client_id + "_Lamp";
            
            if (friendlamp_mqtt_user.length() > 0) {
                 connected = mqttClientLamp.connect(lampClientId.c_str(), friendlamp_mqtt_user.c_str(), friendlamp_mqtt_pass.c_str(),
                                              statusTopic.c_str(), 1, true, lwtMessage.c_str());
            } else {
                 connected = mqttClientLamp.connect(lampClientId.c_str(), statusTopic.c_str(), 1, true, lwtMessage.c_str());
            }

            if (connected) {
                Serial.println("connected");
                // Zentraler Status für das Dashboard (Retained)
                mqttClientLamp.publish(statusTopic.c_str(), (String(FW_VERSION) + ":online").c_str(), true);
                
                Serial.println("--> LAMP MQTT: Subscribing to topics: " + friendlamp_topic + " and " + zwitscherbox_topic);
                mqttClientLamp.subscribe(friendlamp_topic.c_str());
                mqttClientLamp.subscribe(zwitscherbox_topic.c_str());
                mqttClientLamp.subscribe("zwitscherbox/update/trigger");
                mqttClientLamp.publish("zwitscherbox/update/status", ("V" + String(FW_VERSION) + ":" + String(mqtt_client_id)).c_str(), false);
            } else {
                Serial.print("failed, rc=");
                Serial.print(mqttClientLamp.state());
                Serial.println(" try again later");
            }
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

// --- Funktion zum Laden der MP3-Dateien aus dem aktuellen Verzeichnis ---
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
                if (isStandby) {
                    isStandby = false;
                    Serial.println("Woke up from Standby (Button)");
                    publishMqtt(mqtt_topic_status, "Woke up from Standby");
                }
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
    neoPixelMutex = xSemaphoreCreateMutex();
    pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);
    pinMode(POT_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200); Serial.println("\nStarting: Directory MP3 Player V_F (MQTT)...");

    lastPirActivityTime = millis();
    randomSeed(esp_random());
    spi_onboardSD->begin();

    Serial.println("Init SD...");
    if (!SD.begin(SD_CS_PIN, *spi_onboardSD)) { Serial.println("SD FAIL!"); while (true); }
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
        strip.updateLength(led_count > 0 ? led_count : DEFAULT_LED_COUNT);
        strip.begin();
        strip.show(); // Initialize all pixels to 'off'
        strip.setBrightness(led_brightness); // Set brightness from config
        Serial.println("Friendship Lamp (LED Ring) initialized. Brightness: " + String(led_brightness));
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
#include "MqttHandler.h"
#include "GlobalConfig.h"
#include "LedController.h"
#include "AudioEngine.h"
#include "WebManager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "Audio.h"

extern AudioEngine audioEngine;

// --- Globals extracted from main ---
WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMqttReconnectAttempt = 0;
WiFiClient espClientLamp;
WiFiClientSecure espClientSecureLamp;
PubSubClient mqttClientLamp(espClientLamp);
unsigned long lastLampMqttReconnectAttempt = 0;
const long mqttReconnectInterval = 5000;

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
                parsedColor = Adafruit_NeoPixel::Color(r, g, b);
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

    if (senderId.length() > 0 && senderId == config.mqtt_client_id) {
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

    ledCtrl.startFadeIn(finalColor, 1, isRainbow, isBlink);
    ledCtrl.ledTimeout = millis() + duration;
    ledCtrl.ledActive = true; 
}

void publishMqtt(const String& topic, const String& payload, bool retain) {
    if (!config.homeassistant_mqtt_enabled || !mqttClient.connected()) {
        // Nicht senden, wenn deaktiviert oder nicht verbunden
        return;
    }
    mqttClient.publish(topic.c_str(), payload.c_str(), retain);
}

void publishMqttLamp(const String& topic, const String& payload, bool retain) {
    if (!config.friendlamp_mqtt_enabled || !config.friendlamp_enabled) return;
    
    // Wenn ein separater Server konfiguriert ist, nutze diesen, ansonsten den internen
    if (config.friendlamp_mqtt_server != "") {
        if (mqttClientLamp.connected()) {
            mqttClientLamp.publish(topic.c_str(), payload.c_str(), retain);
        }
    } else {
        // Nutze den internen Broker, wenn für HA aktiviert
        if (config.homeassistant_mqtt_enabled && mqttClient.connected()) {
            mqttClient.publish(topic.c_str(), payload.c_str(), retain);
        }
    }
}

void setup_wifi() {
    if (!config.homeassistant_mqtt_enabled && !config.friendlamp_mqtt_enabled) {
        WiFi.mode(WIFI_OFF);
        Serial.println("All MQTT integrations disabled. WiFi is OFF.");
        return; 
    }

    if (config.wifi_ssid == "") {
        Serial.println("WiFi SSID not configured. Starting Config Portal.");
        startConfigPortal();
        return;
    }

    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(config.wifi_ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) { // 15 Sek Timeout
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
        ledCtrl.setBootStatusLeds(0, false);
        Serial.println("\nWiFi connection failed! Starting Config Portal.");
        WiFi.disconnect(true);
        startConfigPortal();
    } else {
        ledCtrl.setBootStatusLeds(0, true);
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        publishMqtt(config.getTopicIp(), WiFi.localIP().toString(), true);

        // --- Zusätzliche Ausgabe für den Benutzer ---
        Serial.println("----------------------------------------");
        Serial.println("Netzwerk & MQTT Status:");
        Serial.println("  WLAN (SSID): " + config.wifi_ssid);
        Serial.println("  IP Adresse:  " + WiFi.localIP().toString());
        if (config.homeassistant_mqtt_enabled) {
            Serial.println("  HA MQTT:     " + config.mqtt_server + ":" + String(config.mqtt_port));
        }
        if (config.friendlamp_mqtt_enabled && config.friendlamp_mqtt_server != "") {
            Serial.println("  Friend-MQTT: " + config.friendlamp_mqtt_server + ":" + String(config.friendlamp_mqtt_port));
        }
        Serial.println("----------------------------------------");
        
        // Webserver auch im normalen WLAN-Modus unter der lokalen IP-Adresse starten
        setupWebServer();
    }
}

void performOtaUpdate(const char* url, const char* version) {
    Serial.println("OTA Update Prozess gestartet...");
    Serial.printf("Update-URL: %s\n", url);

    if (audioEngine.getState() != PlaybackState::IDLE && audioEngine.getState() != PlaybackState::STANDBY && audioEngine.getState() != PlaybackState::INITIALIZING) {
        audioEngine.stopPlayback();
    }

    String statusTopic = "zwitscherbox/status/" + config.mqtt_client_id;
    String startMsg = "Updating to " + String(version);
    
    // Status an beide Broker (falls verbunden)
    if (config.homeassistant_mqtt_enabled && mqttClient.connected()) {
        mqttClient.publish(statusTopic.c_str(), (String(FW_VERSION) + ":" + startMsg).c_str(), true);
        mqttClient.publish("zwitscherbox/update/status", startMsg.c_str());
    }
    if (config.friendlamp_mqtt_enabled && config.friendlamp_mqtt_server != "" && mqttClientLamp.connected()) {
        mqttClientLamp.publish(statusTopic.c_str(), (String(FW_VERSION) + ":" + startMsg).c_str(), true);
        mqttClientLamp.publish("zwitscherbox/update/status", startMsg.c_str());
    }

    WiFiClientSecure otaClient;
    otaClient.setInsecure();

    if (config.friendlamp_enabled) {
        ledCtrl.startFadeIn(0x0000FF, 0, false, false); // Blue
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
            String errorMsg = "V" + String(FW_VERSION) + ":" + String(config.mqtt_client_id) + " - Update failed: " + httpUpdate.getLastErrorString();
            Serial.println(errorMsg);
            if (config.homeassistant_mqtt_enabled && mqttClient.connected()) mqttClient.publish(statusTopic.c_str(), errorMsg.c_str(), false);
            if (config.friendlamp_mqtt_enabled && config.friendlamp_mqtt_server != "" && mqttClientLamp.connected()) mqttClientLamp.publish(statusTopic.c_str(), errorMsg.c_str(), false);
            if (config.friendlamp_enabled) {
                ledCtrl.startFadeIn(0xFF0000, 0, false, false); // Red
                delay(2000);
                ledCtrl.turnOff();
            }
            break;
        }
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("Keine Updates verfügbar.");
            if (config.friendlamp_enabled) {
                ledCtrl.turnOff();
            }
            break;
        case HTTP_UPDATE_OK: {
            Serial.println("Update erfolgreich! ESP32 startet neu...");
            String okMsg = "V" + String(FW_VERSION) + ":" + String(config.mqtt_client_id) + " - Success! Rebooting...";
            if (config.homeassistant_mqtt_enabled && mqttClient.connected()) mqttClient.publish(statusTopic.c_str(), okMsg.c_str(), false);
            if (config.friendlamp_mqtt_enabled && config.friendlamp_mqtt_server != "" && mqttClientLamp.connected()) mqttClientLamp.publish(statusTopic.c_str(), okMsg.c_str(), false);
            if (config.friendlamp_enabled) {
                ledCtrl.turnOff();
            }
            delay(1000);
            ESP.restart();
            break;
        }
    }
}

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
            // Check if this update is targeted to a specific client_id
            const char* target = doc["target"] | "";
            if (strlen(target) > 0 && strcmp(target, config.mqtt_client_id.c_str()) != 0) {
                Serial.println("OTA: Ignored (targeted to different device: " + String(target) + ")");
                return;
            }

            const char* url = doc["url"] | "";
            const char* version = doc["version"] | "";
            if (strlen(url) > 0 && strlen(version) > 0) {
                if (strcmp(version, FW_VERSION) != 0) {
                    performOtaUpdate(url, version);
                } else {
                    String statusTopic = "zwitscherbox/update/status";
                    String okMsg = "V" + String(FW_VERSION) + ":" + String(config.mqtt_client_id) + " - Already up to date";
                    if (config.homeassistant_mqtt_enabled && mqttClient.connected()) mqttClient.publish(statusTopic.c_str(), okMsg.c_str(), false);
                    if (config.friendlamp_mqtt_enabled && config.friendlamp_mqtt_server != "" && mqttClientLamp.connected()) mqttClientLamp.publish(statusTopic.c_str(), okMsg.c_str(), false);
                }
            }
        }
        return;
    }

    // Use strcmp for safe C-string comparison
    if (config.friendlamp_enabled && strcmp(topic, config.zwitscherbox_topic.c_str()) == 0) {
        Serial.println("--> handleLampMessage: Topic matches config.zwitscherbox_topic!");
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error && !doc["client_id"].isNull()) {
            String senderId = doc["client_id"] | "";
            String colorStr = doc["color"] | "";
            String effect = doc["effect"] | "fade";
            long duration = doc["duration"] | 30000;
            
            Serial.println("--> handleLampMessage (JSON): Sender ID is '" + senderId + "', my ID is '" + config.mqtt_client_id + "'");
            if (senderId != config.mqtt_client_id && senderId.length() > 0) {
                Serial.println("--> handleLampMessage (JSON): Sender ID is different, proceeding to light up LED.");
                bool isRainbow = effect.equalsIgnoreCase("rainbow") || colorStr.equalsIgnoreCase("RAINBOW");
                bool isBlink = effect.equalsIgnoreCase("blink");
                
                if (colorStr.startsWith("#")) colorStr = colorStr.substring(1);
                ledCtrl.currentLedColor = isRainbow ? 0 : strtol(colorStr.c_str(), NULL, 16);
                ledCtrl.ledTimeout = millis() + duration;
                ledCtrl.ledActive = true;
                if (config.friendlamp_enabled) {
                    Serial.printf("--> handleLampMessage: Starting effect %s.\n", effect.c_str());
                    ledCtrl.startFadeIn(ledCtrl.currentLedColor, 0, isRainbow, isBlink);
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
                
                Serial.println("--> handleLampMessage (Legacy): Sender ID is '" + senderId + "', my ID is '" + config.mqtt_client_id + "'");
                // Reagiere nicht auf die eigene Nachricht
                if (senderId != config.mqtt_client_id) {
                    Serial.println("--> handleLampMessage (Legacy): Sender ID is different, proceeding to light up LED (Full).");
                    bool isRainbow = colorStr.equalsIgnoreCase("RAINBOW");
                    
                    if (colorStr.startsWith("#")) colorStr = colorStr.substring(1);
                    ledCtrl.currentLedColor = isRainbow ? 0 : strtol(colorStr.c_str(), NULL, 16);
                    ledCtrl.ledTimeout = millis() + 30000; // 30 Sekunden leuchten
                    ledCtrl.ledActive = true;
                    if (config.friendlamp_enabled) {
                        Serial.println("--> handleLampMessage: Starting Fade-In.");
                        ledCtrl.startFadeIn(ledCtrl.currentLedColor, 0, isRainbow, false);
                    }
                    Serial.printf("Zwitscherbox: Received color %s from %s\n", colorStr.c_str(), senderId.c_str());
                } else {
                    Serial.println("--> handleLampMessage: Ignored own message.");
                }
            } else {
                Serial.println("--> handleLampMessage: Invalid format for Zwitscherbox. Expected Sender:HexColor or valid JSON.");
            }
        }
    } else if (config.friendlamp_enabled && strcmp(topic, config.friendlamp_topic.c_str()) == 0) {
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

void mqtt_reconnect() {
    String statusTopic = "zwitscherbox/status/" + config.mqtt_client_id;
    String lwtMessage = "offline";

    // --- Wiederverbindung für den internen Broker (Home Assistant) ---
    if (config.homeassistant_mqtt_enabled && !mqttClient.connected() && millis() - lastMqttReconnectAttempt > mqttReconnectInterval) {
        lastMqttReconnectAttempt = millis();

        mqttClient.setClient(espClient); // Immer den ungesicherten Client verwenden

        Serial.print("Attempting Internal (HA) MQTT connection...");
        bool connected = false;
        if (config.mqtt_user.length() > 0) {
             connected = mqttClient.connect(config.mqtt_client_id.c_str(), config.mqtt_user.c_str(), config.mqtt_pass.c_str(),
                                          statusTopic.c_str(), 1, true, lwtMessage.c_str());
        } else {
             connected = mqttClient.connect(config.mqtt_client_id.c_str(), statusTopic.c_str(), 1, true, lwtMessage.c_str());
        }

        static bool firstHaConnectAttempt = true;
        if (connected) {
            if (firstHaConnectAttempt) { ledCtrl.setBootStatusLeds(2, true); firstHaConnectAttempt = false; }
            Serial.println("connected");
            publishMqtt(config.getTopicIp(), WiFi.localIP().toString(), true);
            
            // Zentraler Status für das Dashboard (Retained)
            mqttClient.publish(statusTopic.c_str(), (String(FW_VERSION) + ":online").c_str(), true);
            publishMqtt(config.getTopicStatus(), "Online", true); 
             // Immer OTA Update Trigger Topic abonnieren
             mqttClient.subscribe("zwitscherbox/update/trigger");
             mqttClient.publish("zwitscherbox/update/status", ("V" + String(FW_VERSION) + ":" + String(config.mqtt_client_id)).c_str(), false);
             
             // Subscribe auf dem internen Broker für die Lampe, falls diese auch dort läuft
             if (config.friendlamp_mqtt_enabled && config.friendlamp_enabled && config.friendlamp_mqtt_server == "") {
                 mqttClient.subscribe(config.friendlamp_topic.c_str());
                 mqttClient.subscribe(config.zwitscherbox_topic.c_str());
                 Serial.println("Subscribed to Friendlamp topics on internal broker.");
             }
        } else {
            if (firstHaConnectAttempt) { ledCtrl.setBootStatusLeds(2, false); firstHaConnectAttempt = false; }
            Serial.print("failed, rc=");
            Serial.print(mqttClient.state());
            Serial.println(" try again later");
        }
    }

    // --- Wiederverbindung für den Freundschaftslampen-Broker (falls konfiguriert) ---
    if (config.friendlamp_mqtt_enabled && config.friendlamp_enabled && config.friendlamp_mqtt_server != "" && !mqttClientLamp.connected()) {
        if (millis() - lastLampMqttReconnectAttempt > mqttReconnectInterval) {
            lastLampMqttReconnectAttempt = millis();
            Serial.print("Attempting Lamp MQTT connection...");
            bool connected = false;
            
            // Um Kollisionen zu vermeiden, fügen wir _Lamp zur Client-ID hinzu, falls sie nicht einzigartig ist
            String lampClientId = config.mqtt_client_id + "_Lamp";
            
            if (config.friendlamp_mqtt_user.length() > 0) {
                 connected = mqttClientLamp.connect(lampClientId.c_str(), config.friendlamp_mqtt_user.c_str(), config.friendlamp_mqtt_pass.c_str(),
                                              statusTopic.c_str(), 1, true, lwtMessage.c_str());
            } else {
                 connected = mqttClientLamp.connect(lampClientId.c_str(), statusTopic.c_str(), 1, true, lwtMessage.c_str());
            }

            static bool firstLampConnectAttempt = true;
            if (connected) {
                if (firstLampConnectAttempt) { ledCtrl.setBootStatusLeds(2, true); firstLampConnectAttempt = false; }
                Serial.println("connected");
                // Zentraler Status für das Dashboard (Retained)
                mqttClientLamp.publish(statusTopic.c_str(), (String(FW_VERSION) + ":online").c_str(), true);
                
                Serial.println("--> LAMP MQTT: Subscribing to topics: " + config.friendlamp_topic + " and " + config.zwitscherbox_topic);
                mqttClientLamp.subscribe(config.friendlamp_topic.c_str());
                mqttClientLamp.subscribe(config.zwitscherbox_topic.c_str());
                mqttClientLamp.subscribe("zwitscherbox/update/trigger");
                mqttClientLamp.publish("zwitscherbox/update/status", ("V" + String(FW_VERSION) + ":" + String(config.mqtt_client_id)).c_str(), false);
            } else {
                if (firstLampConnectAttempt) { ledCtrl.setBootStatusLeds(2, false); firstLampConnectAttempt = false; }
                Serial.print("failed, rc=");
                Serial.print(mqttClientLamp.state());
                Serial.println(" try again later");
            }
        }
    }
}


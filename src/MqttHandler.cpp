#include "MqttHandler.h"
#include "GlobalConfig.h"
#include "LedController.h"
#include "WebManager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include "Audio.h"

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

void publishMqtt(const String& topic, const String& payload, bool retain) {
    if (!homeassistant_mqtt_enabled || !mqttClient.connected()) {
        // Nicht senden, wenn deaktiviert oder nicht verbunden
        return;
    }
    mqttClient.publish(topic.c_str(), payload.c_str(), retain);
}

void publishMqttLamp(const String& topic, const String& payload, bool retain) {
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
        setBootStatusLeds(0, false);
        Serial.println("\nWiFi connection failed! Starting Config Portal.");
        WiFi.disconnect(true);
        startConfigPortal();
    } else {
        setBootStatusLeds(0, true);
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
            if (friendlamp_enabled) {
                strip.clear();
                strip.show();
            }
            break;
        case HTTP_UPDATE_OK: {
            Serial.println("Update erfolgreich! ESP32 startet neu...");
            String okMsg = "V" + String(FW_VERSION) + ":" + String(mqtt_client_id) + " - Success! Rebooting...";
            if (homeassistant_mqtt_enabled && mqttClient.connected()) mqttClient.publish(statusTopic.c_str(), okMsg.c_str(), false);
            if (friendlamp_mqtt_enabled && friendlamp_mqtt_server != "" && mqttClientLamp.connected()) mqttClientLamp.publish(statusTopic.c_str(), okMsg.c_str(), false);
            if (friendlamp_enabled) {
                strip.clear();
                strip.show();
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
            if (strlen(target) > 0 && strcmp(target, mqtt_client_id.c_str()) != 0) {
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

        static bool firstHaConnectAttempt = true;
        if (connected) {
            if (firstHaConnectAttempt) { setBootStatusLeds(2, true); firstHaConnectAttempt = false; }
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
            if (firstHaConnectAttempt) { setBootStatusLeds(2, false); firstHaConnectAttempt = false; }
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

            static bool firstLampConnectAttempt = true;
            if (connected) {
                if (firstLampConnectAttempt) { setBootStatusLeds(2, true); firstLampConnectAttempt = false; }
                Serial.println("connected");
                // Zentraler Status für das Dashboard (Retained)
                mqttClientLamp.publish(statusTopic.c_str(), (String(FW_VERSION) + ":online").c_str(), true);
                
                Serial.println("--> LAMP MQTT: Subscribing to topics: " + friendlamp_topic + " and " + zwitscherbox_topic);
                mqttClientLamp.subscribe(friendlamp_topic.c_str());
                mqttClientLamp.subscribe(zwitscherbox_topic.c_str());
                mqttClientLamp.subscribe("zwitscherbox/update/trigger");
                mqttClientLamp.publish("zwitscherbox/update/status", ("V" + String(FW_VERSION) + ":" + String(mqtt_client_id)).c_str(), false);
            } else {
                if (firstLampConnectAttempt) { setBootStatusLeds(2, false); firstLampConnectAttempt = false; }
                Serial.print("failed, rc=");
                Serial.print(mqttClientLamp.state());
                Serial.println(" try again later");
            }
        }
    }
}


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

// Global instance 
MqttHandler mqttHandler;

// --- Object Methods ---

MqttHandler::MqttHandler() : 
    mqttClient(espClient), 
    mqttClientLamp(espClientLamp),
    lastMqttReconnectAttempt(0),
    lastLampMqttReconnectAttempt(0),
    mqttReconnectInterval(5000) 
{
}

void MqttHandler::setupWifi() {
    if (!config.homeassistant_mqtt_enabled && !config.friendlamp_mqtt_enabled) {
        WiFi.mode(WIFI_OFF);
        Serial.println("All MQTT integrations disabled. WiFi is OFF.");
        return; 
    }

    if (config.wifi_ssid == "") {
        Serial.println("WiFi SSID not configured. Starting Config Portal.");
        webManager.startConfigPortal();
        return;
    }

    Serial.print("Connecting to WiFi SSID: ");
    Serial.println(config.wifi_ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(config.wifi_ssid.c_str(), config.wifi_pass.c_str());

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
        Serial.print(".");
        delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
        ledCtrl.setBootStatusLeds(0, false);
        Serial.println("\nWiFi connection failed! Starting Config Portal.");
        WiFi.disconnect(true);
        webManager.startConfigPortal();
    } else {
        ledCtrl.setBootStatusLeds(0, true);
        Serial.println("\nWiFi connected!");
        Serial.print("IP address: ");
        Serial.println(WiFi.localIP());
        
        publish(config.getTopicIp(), WiFi.localIP().toString(), true);

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
        
        webManager.setupWebServer();
    }
    
    // Config internal broker
    if (config.homeassistant_mqtt_enabled) {
        if (config.mqtt_server != "") {
            mqttClient.setServer(config.mqtt_server.c_str(), config.mqtt_port);
            mqttClient.setCallback(MqttHandler::staticMqttCallback);
            Serial.println("Internal MQTT Server configured.");
        } else {
             Serial.println("WARNING: Internal MQTT Server not configured, disabling HA integration.");
             config.homeassistant_mqtt_enabled = false;
        }
    }

    // Config Lamp broker
    if (config.friendlamp_mqtt_enabled && config.friendlamp_enabled && config.friendlamp_mqtt_server != "") {
        if (config.friendlamp_mqtt_tls_enabled) {
            if (config.mqtt_root_ca_content.length() > 20) {
                espClientSecureLamp.setCACert(config.mqtt_root_ca_content.c_str());
            } else {
                espClientSecureLamp.setInsecure();
            }
            mqttClientLamp.setClient(espClientSecureLamp);
        } else {
            mqttClientLamp.setClient(espClientLamp);
        }
        mqttClientLamp.setServer(config.friendlamp_mqtt_server.c_str(), config.friendlamp_mqtt_port);
        mqttClientLamp.setCallback(MqttHandler::staticMqttLampCallback);
        Serial.println("Friendship Lamp MQTT Server configured.");
    }
}

void MqttHandler::internalMqttReconnect() {
    String statusTopic = "zwitscherbox/status/" + config.mqtt_client_id;
    String lwtMessage = "offline";

    if (config.homeassistant_mqtt_enabled && !mqttClient.connected() && millis() - lastMqttReconnectAttempt > mqttReconnectInterval) {
        lastMqttReconnectAttempt = millis();
        mqttClient.setClient(espClient);

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
            publish(config.getTopicIp(), WiFi.localIP().toString(), true);
            
            mqttClient.publish(statusTopic.c_str(), (String(FW_VERSION) + ":online").c_str(), true);
            publish(config.getTopicStatus(), "Online", true); 
            mqttClient.subscribe("zwitscherbox/update/trigger");
            mqttClient.publish("zwitscherbox/update/status", ("V" + String(FW_VERSION) + ":" + String(config.mqtt_client_id)).c_str(), false);
             
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

    if (config.friendlamp_mqtt_enabled && config.friendlamp_enabled && config.friendlamp_mqtt_server != "" && !mqttClientLamp.connected()) {
        if (millis() - lastLampMqttReconnectAttempt > mqttReconnectInterval) {
            lastLampMqttReconnectAttempt = millis();
            Serial.print("Attempting Lamp MQTT connection...");
            bool connected = false;
            
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

void MqttHandler::update() {
    if (config.homeassistant_mqtt_enabled || config.friendlamp_mqtt_enabled) {
        if (WiFi.status() == WL_CONNECTED) {
            internalMqttReconnect(); 
            
            if (config.homeassistant_mqtt_enabled && mqttClient.connected()) {
                mqttClient.loop();
            }
            if (config.friendlamp_mqtt_enabled && config.friendlamp_enabled && config.friendlamp_mqtt_server != "" && mqttClientLamp.connected()) {
                mqttClientLamp.loop(); 
            }
        } else {
            static unsigned long lastWifiCheck = 0;
            if (millis() - lastWifiCheck > 30000) { 
                Serial.println("WiFi disconnected. Attempting reconnect...");
                WiFi.disconnect();
                WiFi.reconnect();
                lastWifiCheck = millis();
            }
        }
    }
}

void MqttHandler::forceReconnect() {
    internalMqttReconnect();
}

void MqttHandler::publish(const String& topic, const String& payload, bool retain) {
    if (!config.homeassistant_mqtt_enabled || !mqttClient.connected()) return;
    mqttClient.publish(topic.c_str(), payload.c_str(), retain);
}

void MqttHandler::publishLamp(const String& topic, const String& payload, bool retain) {
    if (!config.friendlamp_mqtt_enabled || !config.friendlamp_enabled) return;
    
    if (config.friendlamp_mqtt_server != "") {
        if (mqttClientLamp.connected()) {
            mqttClientLamp.publish(topic.c_str(), payload.c_str(), retain);
        }
    } else {
        if (config.homeassistant_mqtt_enabled && mqttClient.connected()) {
            mqttClient.publish(topic.c_str(), payload.c_str(), retain);
        }
    }
}

void MqttHandler::handleFreundschaftMessage(String payload) {
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
            int firstComma = payload.indexOf(',');
            int secondComma = payload.lastIndexOf(',');
            if (firstComma != -1 && secondComma != -1 && firstComma != secondComma) {
                int r = payload.substring(0, firstComma).toInt();
                int g = payload.substring(firstComma + 1, secondComma).toInt();
                int b = payload.substring(secondComma + 1).toInt();
                parsedColor = Adafruit_NeoPixel::Color(r, g, b);
                hasLegacyRGB = true;
            } else {
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

void MqttHandler::performOtaUpdate(const char* url, const char* version) {
    Serial.println("OTA Update Prozess gestartet...");
    Serial.printf("Update-URL: %s\n", url);

    if (audioEngine.getState() != PlaybackState::IDLE && audioEngine.getState() != PlaybackState::STANDBY && audioEngine.getState() != PlaybackState::INITIALIZING) {
        audioEngine.stopPlayback();
    }

    String statusTopic = "zwitscherbox/status/" + config.mqtt_client_id;
    String startMsg = "Updating to " + String(version);
    
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

void MqttHandler::handleLampCallback(char* topic, byte* payload, unsigned int length) {
    Serial.println("--> handleLampMessage: Function called!");
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.printf("Friendlamp MQTT Received [%s]: %s\n", topic, message.c_str());

    String otaTriggerTopic = "zwitscherbox/update/trigger";
    if (strcmp(topic, otaTriggerTopic.c_str()) == 0) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        if (!error) {
            const char* target = doc["target"] | "";
            if (strlen(target) > 0 && strcmp(target, config.mqtt_client_id.c_str()) != 0) {
                Serial.println("OTA: Ignored (targeted to different device)");
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

    if (config.friendlamp_enabled && strcmp(topic, config.zwitscherbox_topic.c_str()) == 0) {
        Serial.println("--> handleLampMessage: Topic matches config.zwitscherbox_topic!");
        
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, message);
        
        if (!error && !doc["client_id"].isNull()) {
            String senderId = doc["client_id"] | "";
            String colorStr = doc["color"] | "";
            String effect = doc["effect"] | "fade";
            long duration = doc["duration"] | 30000;
            
            if (senderId != config.mqtt_client_id && senderId.length() > 0) {
                bool isRainbow = effect.equalsIgnoreCase("rainbow") || colorStr.equalsIgnoreCase("RAINBOW");
                bool isBlink = effect.equalsIgnoreCase("blink");
                
                if (colorStr.startsWith("#")) colorStr = colorStr.substring(1);
                ledCtrl.currentLedColor = isRainbow ? 0 : strtol(colorStr.c_str(), NULL, 16);
                ledCtrl.ledTimeout = millis() + duration;
                ledCtrl.ledActive = true;
                if (config.friendlamp_enabled) {
                    ledCtrl.startFadeIn(ledCtrl.currentLedColor, 0, isRainbow, isBlink);
                }
            }
        } else {
            int separatorPos = message.indexOf(':');
            if (separatorPos != -1) {
                String senderId = message.substring(0, separatorPos);
                String colorStr = message.substring(separatorPos + 1);
                
                if (senderId != config.mqtt_client_id) {
                    bool isRainbow = colorStr.equalsIgnoreCase("RAINBOW");
                    if (colorStr.startsWith("#")) colorStr = colorStr.substring(1);
                    ledCtrl.currentLedColor = isRainbow ? 0 : strtol(colorStr.c_str(), NULL, 16);
                    ledCtrl.ledTimeout = millis() + 30000; 
                    ledCtrl.ledActive = true;
                    if (config.friendlamp_enabled) {
                        ledCtrl.startFadeIn(ledCtrl.currentLedColor, 0, isRainbow, false);
                    }
                }
            }
        }
    } else if (config.friendlamp_enabled && strcmp(topic, config.friendlamp_topic.c_str()) == 0) {
        handleFreundschaftMessage(message);
    }
}

void MqttHandler::handleStandardCallback(char* topic, byte* payload, unsigned int length) {
    handleLampCallback(topic, payload, length);
}

// Static Bridges
void MqttHandler::staticMqttCallback(char* topic, byte* payload, unsigned int length) {
    mqttHandler.handleStandardCallback(topic, payload, length);
}

void MqttHandler::staticMqttLampCallback(char* topic, byte* payload, unsigned int length) {
    mqttHandler.handleLampCallback(topic, payload, length);
}

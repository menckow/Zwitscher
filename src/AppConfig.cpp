#include "AppConfig.h"
#include <SD.h>

void AppConfig::load() {
    File configFile = SD.open("/config.txt");
    if (!configFile) {
        Serial.println("ERROR: config.txt not found on SD card!");
        return;
    }

    Serial.println("Reading config.txt...");
    bool inCertBlock = false;

    while (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        line.trim();

        if (inCertBlock) {
            if (line == "END_CERT") {
                inCertBlock = false;
                Serial.println("...End of certificate.");
            } else {
                mqtt_root_ca_content += line + "\n";
            }
            continue;
        }

        if (line.length() == 0 || line.startsWith("#")) {
            continue;
        }

        if (line == "BEGIN_CERT") {
            inCertBlock = true;
            mqtt_root_ca_content = "";
            Serial.println("Reading certificate block...");
            continue;
        }

        int separatorPos = line.indexOf('=');
        if (separatorPos == -1) {
            Serial.println("Skipping invalid line in config: " + line);
            continue;
        }

        String key = line.substring(0, separatorPos);
        String value = line.substring(separatorPos + 1);
        key.trim();
        value.trim();
        key.toUpperCase();

        if (key == "ADMIN_PASS") admin_pass = value;
        else if (key == "WIFI_SSID") wifi_ssid = value;
        else if (key == "WIFI_PASS") wifi_pass = value;
        else if (key == "MQTT_SERVER") mqtt_server = value;
        else if (key == "MQTT_PORT") { mqtt_port = value.toInt(); if (mqtt_port == 0) mqtt_port = 1883; }
        else if (key == "MQTT_USER") mqtt_user = value;
        else if (key == "MQTT_PASS") mqtt_pass = value;
        else if (key == "MQTT_CLIENT_ID") mqtt_client_id = value;
        else if (key == "MQTT_BASE_TOPIC") mqtt_base_topic = value;
        else if (key == "MQTT_INTEGRATION") homeassistant_mqtt_enabled = (value == "1");
        else if (key == "FRIENDLAMP_MQTT_TLS_ENABLED") friendlamp_mqtt_tls_enabled = (value == "1");
        else if (key == "FRIENDLAMP_MQTT_INTEGRATION") friendlamp_mqtt_enabled = (value == "1");
        else if (key == "FRIENDLAMP_ENABLE") friendlamp_enabled = (value == "1");
        else if (key == "FRIENDLAMP_COLOR") friendlamp_color = value;
        else if (key == "FRIENDLAMP_TOPIC") friendlamp_topic = value;
        else if (key == "ZWITSCHERBOX_TOPIC") zwitscherbox_topic = value;
        else if (key == "FRIENDLAMP_MQTT_SERVER") friendlamp_mqtt_server = value;
        else if (key == "FRIENDLAMP_MQTT_PORT") { friendlamp_mqtt_port = value.toInt(); if (friendlamp_mqtt_port == 0) friendlamp_mqtt_port = 1883; }
        else if (key == "FRIENDLAMP_MQTT_USER") friendlamp_mqtt_user = value;
        else if (key == "FRIENDLAMP_MQTT_PASS") friendlamp_mqtt_pass = value;
        else if (key == "LED_FADE_EFFECT") led_fade_effect = (value == "1");
        else if (key == "LED_FADE_DURATION") { int new_dur = value.toInt(); if (new_dur > 0) fadeDuration = new_dur; }
        else if (key == "LED_BRIGHTNESS") { int new_br = value.toInt(); if (new_br >= 0 && new_br <= 255) led_brightness = new_br; }
        else if (key == "LED_COUNT") { int new_cnt = value.toInt(); if (new_cnt > 0) led_count = new_cnt; }
    }
    configFile.close();

    if (homeassistant_mqtt_enabled) {
        Serial.println("Home Assistant MQTT Integration: ENABLED");
        Serial.println("  HA-MQTT Server: " + mqtt_server + ":" + String(mqtt_port));
        Serial.println("  HA-MQTT User: " + mqtt_user);
        Serial.println("  HA-MQTT Client ID: " + mqtt_client_id);
        Serial.println("  HA-MQTT Base Topic: " + mqtt_base_topic);
        Serial.println("  HA-MQTT TLS: DISABLED");
    } else {
         Serial.println("Home Assistant MQTT Integration: DISABLED");
    }

    if (friendlamp_mqtt_enabled) {
        Serial.println("Friendship Lamp MQTT Integration: ENABLED");
         if(friendlamp_mqtt_tls_enabled) {
            Serial.println("  Friend-MQTT TLS: ENABLED");
            if(mqtt_root_ca_content.length() > 20) {
                 Serial.println("  Friend-MQTT Root CA: Loaded");
            } else {
                 Serial.println("  Friend-MQTT Root CA: NOT loaded or empty!");
            }
        } else {
            Serial.println("  Friend-MQTT TLS: DISABLED");
        }
    } else {
        Serial.println("Friendship Lamp MQTT Integration: DISABLED");
    }

    if (!homeassistant_mqtt_enabled && !friendlamp_mqtt_enabled) {
        Serial.println("All MQTT Integrations disabled. Device will remain offline.");
    } else {
        Serial.println("WiFi SSID: " + wifi_ssid);
    }
}

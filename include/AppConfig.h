#pragma once

#include <Arduino.h>

class AppConfig {
public:
    // WLAN & Auth
    String wifi_ssid = "";
    String wifi_pass = "";
    String admin_pass = ""; // Optional web interface password

    // MQTT (Home Assistant / Internal)
    String mqtt_server = "";
    int    mqtt_port = 1883;
    String mqtt_user = "";
    String mqtt_pass = "";
    String mqtt_client_id = "Zwitscherbox";
    String mqtt_base_topic = "zwitscherbox";
    bool   homeassistant_mqtt_enabled = false;

    // MQTT (Friendship Lamp / External)
    bool   friendlamp_mqtt_enabled = false;
    bool   friendlamp_mqtt_tls_enabled = false;
    String mqtt_root_ca_content = "";
    String friendlamp_mqtt_server = "";
    int    friendlamp_mqtt_port = 1883;
    String friendlamp_mqtt_user = "";
    String friendlamp_mqtt_pass = "";

    // Hardware & Friendship Lamp Settings
    bool   friendlamp_enabled = false; // Is the LED ring attached?
    bool   led_fade_effect = true;
    int    led_brightness = 50;
    int    led_count = 16;
    int    fadeDuration = 1000;
    String friendlamp_color = "0000FF";
    String friendlamp_topic = "friendlamp/group";
    String zwitscherbox_topic = "zwitscherbox/group";

    // Dynamic MQTT Topics
    String getTopicStatus() const { return mqtt_base_topic + "/status/" + mqtt_client_id; }
    String getTopicError() const { return mqtt_base_topic + "/error/" + mqtt_client_id; }
    String getTopicDebug() const { return mqtt_base_topic + "/debug/" + mqtt_client_id; }
    String getTopicVolume() const { return mqtt_base_topic + "/volume/" + mqtt_client_id; }
    String getTopicDirectory() const { return mqtt_base_topic + "/directory/" + mqtt_client_id; }
    String getTopicPlaying() const { return mqtt_base_topic + "/playing/" + mqtt_client_id; }
    String getTopicIp() const { return mqtt_base_topic + "/ip/" + mqtt_client_id; }

    // Methods
    void load();
    void saveFromWeb(class AsyncWebServerRequest* request);
};

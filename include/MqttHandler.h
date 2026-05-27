#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <vector>
#include <time.h>

// Geraete-Typ-Konstante fuer das v2-Schema (siehe fl/.../<type>-Topics).
#define DEVICE_TYPE_BOX "box"

class MqttHandler {
private:
    WiFiClient espClient;
    PubSubClient mqttClient;

    WiFiClient espClientLamp;
    WiFiClientSecure espClientSecureLamp;
    PubSubClient mqttClientLamp;

    unsigned long lastMqttReconnectAttempt;
    unsigned long lastLampMqttReconnectAttempt;
    const unsigned long mqttReconnectInterval;

    // v2: Liste der Familien-Signal-Topics (eines pro Familie), aus config.family_ids
    std::vector<String> _familySignalTopics;

    void handleFreundschaftMessage(String payload);
    void handleLampCallback(char* topic, byte* payload, unsigned int length);
    void handleStandardCallback(char* topic, byte* payload, unsigned int length);

    void verifyMqttConnection();
    void internalMqttReconnect();

    bool isFamilySignalTopic(const char* topic);

public:
    MqttHandler();
    ~MqttHandler() = default;

    void setupWifi();
    void update(); // Main loop tick for MQTT
    void forceReconnect(); // Alias for old mqtt_reconnect()

    void publish(const String& topic, const String& payload, bool retain = false);
    void publishLamp(const String& topic, const String& payload, bool retain = false);

    // v2-Helper: aus config.family_ids parsen.
    std::vector<String> getFamilies();
    // Familien als JSON-Array-Fragment, z.B. ["schmidt","lieblings"].
    String getFamiliesJsonArray();
    // v2-Statuspublish auf fl/device/<id>/status (JSON, retained) auf beiden Brokern.
    void publishStatusV2(const char* state, const char* extra = nullptr);
    // v2-Statustopic einer Box.
    String getStatusTopicV2();

    void performOtaUpdate(const char* url, const char* version, const char* md5 = nullptr);

    bool isQuietTime();

    // Static callbacks required by PubSubClient
    static void staticMqttCallback(char* topic, byte* payload, unsigned int length);
    static void staticMqttLampCallback(char* topic, byte* payload, unsigned int length);
};

extern MqttHandler mqttHandler;

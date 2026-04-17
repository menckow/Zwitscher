#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>

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

    void handleFreundschaftMessage(String payload);
    void handleLampCallback(char* topic, byte* payload, unsigned int length);
    void handleStandardCallback(char* topic, byte* payload, unsigned int length);

    void verifyMqttConnection();
    void internalMqttReconnect();
    bool isQuietTime();

public:
    MqttHandler();
    ~MqttHandler() = default;

    void setupWifi();
    void update(); // Main loop tick for MQTT
    void forceReconnect(); // Alias for old mqtt_reconnect()
    
    void publish(const String& topic, const String& payload, bool retain = false);
    void publishLamp(const String& topic, const String& payload, bool retain = false);
    
    void performOtaUpdate(const char* url, const char* version, const char* md5 = nullptr);

    // Static callbacks required by PubSubClient
    static void staticMqttCallback(char* topic, byte* payload, unsigned int length);
    static void staticMqttLampCallback(char* topic, byte* payload, unsigned int length);
};

extern MqttHandler mqttHandler;

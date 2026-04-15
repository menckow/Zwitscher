#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

extern WiFiClient espClient;
extern PubSubClient mqttClient;

extern WiFiClient espClientLamp;
extern WiFiClientSecure espClientSecureLamp;
extern PubSubClient mqttClientLamp;

void setup_wifi();
void mqtt_reconnect();
void publishMqtt(const String& topic, const String& payload, bool retain = false);
void publishMqttLamp(const String& topic, const String& payload, bool retain = false);
void performOtaUpdate(const char* url, const char* version);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void mqttLampCallback(char* topic, byte* payload, unsigned int length);

#endif

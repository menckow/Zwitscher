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

const char* FW_VERSION = "7.1.0";
#include <ESPAsyncWebServer.h> // For Webserver
#include <AsyncTCP.h>          // For Webserver
#include <DNSServer.h>         // For Captive Portal
#include "freertos/semphr.h"
#include "MqttHandler.h"
#include "LedController.h"
#include "WebManager.h"
#include "GlobalConfig.h"




// --- Pin-Definitionen ---

const int POT_PIN = 4;
const int PIR_PIN = 18;
const int SD_CS_PIN = SS;
const int BUTTON_PIN = 17; //38
const int LED_PIN = 16;
const int DEFAULT_LED_COUNT = 16;
// config.led_count wird in der AppConfig verwaltet

Adafruit_NeoPixel strip(DEFAULT_LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
LedController ledCtrl(strip);

// --- Timeout-Konstanten ---
const unsigned long maxPlaybackDuration = 5 * 60 * 1000UL;
const unsigned long deepSleepInactivityTimeout = 5 * 60 * 1000UL;

// --- Globale Objekte und Variablen ---
SPIClass *spi_onboardSD = new SPIClass(FSPI);
Audio audio;

// Instantiate Application Configuration
AppConfig config;

#include "AudioEngine.h"

AudioEngine audioEngine(audio);

// --- Lautstärke prüfen --- 

// --- Taster prüfen --- 
// --- Setup --- 
void setup() {
    ledCtrl.begin();
    pinMode(LED_BUILTIN, OUTPUT); digitalWrite(LED_BUILTIN, LOW);
    pinMode(POT_PIN, INPUT);
    pinMode(PIR_PIN, INPUT);
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    Serial.begin(115200); Serial.println("\nStarting: Directory MP3 Player V_F (MQTT)...");

    strip.updateLength(64); // Kurzzeitig auf 64 LEDs erweitern, um alle alten Zustände eines Soft-Reboots (OTA/Lokal) zu killen!
    strip.begin(); 
    strip.clear();
    strip.show();
    
    strip.updateLength(DEFAULT_LED_COUNT); // Für den weißen Startblitz auf Standard zurück
    strip.setBrightness(100); // Heller kurzer Blitz zum Startup
    strip.fill(strip.Color(255, 255, 255));
    strip.show();
    delay(200);
    strip.clear();
    strip.setBrightness(50); // Zurück auf Normalhelligkeit für Status-LEDs
    strip.show();

    audioEngine.updatePirActivity();
    randomSeed(esp_random());
    spi_onboardSD->begin();

    Serial.println("Init SD...");
    if (!SD.begin(SD_CS_PIN, *spi_onboardSD)) {
        ledCtrl.setBootStatusLeds(1, false);
        Serial.println("SD FAIL!"); 
        delay(3000);
        while (true); 
    }
    ledCtrl.setBootStatusLeds(1, true);
    Serial.println("SD OK."); digitalWrite(LED_BUILTIN, HIGH);

    // --- Konfiguration laden ---
    config.load();

    // --- WLAN verbinden (wenn eine der Integrationen aktiviert ist) ---
    if (config.homeassistant_mqtt_enabled || config.friendlamp_mqtt_enabled) {
        mqttHandler.setupWifi();
    }
 

    // --- LED_Ring Setup ---
    if (config.friendlamp_enabled) {
        // updateLength wird ans Ende des Setups verschoben, da es den LED Buffer sofort löscht!
        strip.setBrightness(config.led_brightness); // Set brightness from config
        Serial.println("Friendship Lamp (LED Ring) configured. Brightness: " + String(config.led_brightness));
    }

    Serial.println("Scanning directories...");
    File root = SD.open("/"); if (!root) { Serial.println("ROOT FAIL!"); while (true); }
    audioEngine.findMp3Directories(root);
    root.close();

    audioEngine.init();

    audio.setPinout(I2S_BCLK, I2S_LRCLK, I2S_DOUT);
    
    // The initial volume is initialized via audioEngine internally now, 
    // but Audio hardware init is here.
    Serial.println("Audio hardware pins configured");


    // --- Boot Status abwarten und LEDs prüfen ---
    if ((config.homeassistant_mqtt_enabled || config.friendlamp_mqtt_enabled) && WiFi.status() == WL_CONNECTED) {
        mqttHandler.forceReconnect(); // Trigger initial MQTT connect to show LED 1 status
    }
    
    // Lass die Boot-Status LEDs für 2 Sekunden leuchten, um sie zu überprüfen
    delay(2000); 

    // --- LED_Ring finale Einstellung nach Boot Sequence ---
    if (config.friendlamp_enabled && config.led_count > 0 && config.led_count != DEFAULT_LED_COUNT) {
        strip.updateLength(config.led_count);
    }
    
    if (!webManager.apMode) {
        strip.clear(); 
        strip.show();
    } else {
        // Falls updateLength die LEDs gelöscht hat, setzen wir sie für den AP-Modus wieder rot
        ledCtrl.setApModeLed(true);
    }

    Serial.println("Setup complete.");
    mqttHandler.publish(config.getTopicStatus(), "Initialized", true); // Bereit-Status
    Serial.println("Waiting for PIR or button...");
}

// --- Loop --- 
void loop() {
    if (webManager.pendingRestart) {
        if (millis() > webManager.restartTime) {
            ESP.restart();
        }
        return; 
    }
    if (webManager.apMode) {
        webManager.processDns();
        return; 
    }
    
    ledCtrl.update();
    
    mqttHandler.update();
    audioEngine.update();
    
    if (config.friendlamp_enabled) {
        if (ledCtrl.ledActive && millis() > ledCtrl.ledTimeout) {
            ledCtrl.ledActive = false;
            ledCtrl.startFadeOut();
        }
    }

    vTaskDelay(1);
}

void audio_eof_mp3(const char *info) {
    audioEngine.onAudioEof();
}
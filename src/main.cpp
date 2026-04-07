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
#include <ESPAsyncWebServer.h> // For Webserver
#include <AsyncTCP.h>          // For Webserver
#include <DNSServer.h>         // For Captive Portal
#include "freertos/semphr.h"

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
enum FadeState { FADE_NONE, FADE_IN, FADE_OUT, FADE_RAINBOW_SPIN, FADE_RAINBOW_OUT, FADE_BLINK };
FadeState fadeState = FADE_NONE;
uint32_t fadeColor = 0;
unsigned long fadeStartTime = 0;
int fadeDuration = 1000; // 1 second
// 0 = Normaler Ring, 1 = Jede 3. LED komplementär
int fadeRingMode = 0;
void startFadeIn(uint32_t color, int mode = 0, bool isRainbow = false, bool isBlink = false) {
    if (isRainbow) {
        fadeState = FADE_RAINBOW_SPIN;
        fadeStartTime = millis();
        return;
    }
    if (isBlink) {
        fadeColor = color;
        fadeState = FADE_BLINK;
        fadeStartTime = millis();
        return;
    }
    fadeRingMode = mode; 
    
    if (led_fade_effect) {
        fadeColor = color;
        fadeState = FADE_IN;
        fadeStartTime = millis();
    } else {
        if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
            strip.clear();
            
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            // Errechne die Komplementärfarbe (255 - Farbwert)
            uint32_t compColor = strip.Color(255 - r, 255 - g, 255 - b);
            for(int i = 0; i < strip.numPixels(); i++) {
                 if (fadeRingMode == 1 && (i % 3 == 0)) {
                     strip.setPixelColor(i, compColor);
                 } else {
                     strip.setPixelColor(i, color);
                 }
            }
            strip.show();
            xSemaphoreGive(neoPixelMutex);
        }
    }
}
void startFadeOut() {
    if (led_fade_effect) {
        if (fadeState == FADE_RAINBOW_SPIN) {
             fadeState = FADE_RAINBOW_OUT;
        } else if (fadeState == FADE_BLINK) {
             fadeState = FADE_OUT;
        } else {
             fadeState = FADE_OUT;
        }
        fadeStartTime = millis();
    } else {
        if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
            strip.clear();
            strip.show();
            xSemaphoreGive(neoPixelMutex);
        }
    }
}
void updateFade() {
    if (fadeState == FADE_NONE) return;
    unsigned long currentTime = millis();
    
    if (fadeState == FADE_BLINK) {
        if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
            uint8_t r = (fadeColor >> 16) & 0xFF;
            uint8_t g = (fadeColor >> 8) & 0xFF;
            uint8_t b = fadeColor & 0xFF;
            bool isOn = ((currentTime - fadeStartTime) % 1000) < 500;
            if (isOn) {
                uint32_t c = strip.Color(r * led_brightness / 255, g * led_brightness / 255, b * led_brightness / 255);
                strip.fill(c);
            } else {
                strip.clear();
            }
            strip.show();
            xSemaphoreGive(neoPixelMutex);
        }
        return;
    }

    if (fadeState == FADE_RAINBOW_SPIN || fadeState == FADE_RAINBOW_OUT) {
        if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
            float brightness = 1.0;
            if (fadeState == FADE_RAINBOW_OUT) {
                float rainbowProgress = (float)(currentTime - fadeStartTime) / fadeDuration;
                if (rainbowProgress >= 1.0) {
                     strip.clear();
                     strip.show();
                     fadeState = FADE_NONE;
                     xSemaphoreGive(neoPixelMutex);
                     return;
                }
                brightness = 1.0 - rainbowProgress;
            }
            
            for(int i=0; i<strip.numPixels(); i++) {
                int pixelHue = (currentTime * 10) + (i * 65536L / strip.numPixels());
                uint32_t c = strip.gamma32(strip.ColorHSV(pixelHue));
                uint8_t r = ((c >> 16) & 0xFF) * brightness * led_brightness / 255;
                uint8_t g = ((c >> 8) & 0xFF) * brightness * led_brightness / 255;
                uint8_t b = (c & 0xFF) * brightness * led_brightness / 255;
                strip.setPixelColor(i, strip.Color(r,g,b));
            }
            strip.show();
            xSemaphoreGive(neoPixelMutex);
        }
        return;
    }

    float progress = (float)(currentTime - fadeStartTime) / fadeDuration;
    if (progress >= 1.0) {
        progress = 1.0;
    }
    if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
        strip.clear(); 
        uint8_t r = (fadeColor >> 16) & 0xFF;
        uint8_t g = (fadeColor >> 8) & 0xFF;
        uint8_t b = fadeColor & 0xFF;
        
        uint8_t r_comp = 255 - r;
        uint8_t g_comp = 255 - g;
        uint8_t b_comp = 255 - b;
        if (fadeState == FADE_IN) {
            uint32_t currentColor = strip.Color((uint8_t)(r * progress), (uint8_t)(g * progress), (uint8_t)(b * progress));
            uint32_t currentCompColor = strip.Color((uint8_t)(r_comp * progress), (uint8_t)(g_comp * progress), (uint8_t)(b_comp * progress));
            for (int i = 0; i < strip.numPixels(); i++) {
                 if (fadeRingMode == 1 && (i % 3 == 0)) {
                     strip.setPixelColor(i, currentCompColor);
                 } else {
                     strip.setPixelColor(i, currentColor);
                 }
            }
            strip.show();
        } else if (fadeState == FADE_OUT) {
            uint32_t currentColor = strip.Color((uint8_t)(r * (1.0 - progress)), (uint8_t)(g * (1.0 - progress)), (uint8_t)(b * (1.0 - progress)));
            uint32_t currentCompColor = strip.Color((uint8_t)(r_comp * (1.0 - progress)), (uint8_t)(g_comp * (1.0 - progress)), (uint8_t)(b_comp * (1.0 - progress)));
            for (int i = 0; i < strip.numPixels(); i++) {
                 if (fadeRingMode == 1 && (i % 3 == 0)) {
                     strip.setPixelColor(i, currentCompColor);
                 } else {
                     strip.setPixelColor(i, currentColor);
                 }
            }
            strip.show();
        }
        xSemaphoreGive(neoPixelMutex);
    }
    if (progress == 1.0) {
        if (fadeState == FADE_OUT) {
             if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
                strip.clear();
                strip.show();
                xSemaphoreGive(neoPixelMutex);
             }
        } else if (fadeState == FADE_IN) {
            if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
                strip.clear();
                uint8_t r = (fadeColor >> 16) & 0xFF;
                uint8_t g = (fadeColor >> 8) & 0xFF;
                uint8_t b = fadeColor & 0xFF;
                uint32_t compColor = strip.Color(255 - r, 255 - g, 255 - b);
                for(int i = 0; i < strip.numPixels(); i++) {
                    if (fadeRingMode == 1 && (i % 3 == 0)) {
                         strip.setPixelColor(i, compColor);
                    } else {
                         strip.setPixelColor(i, fadeColor);
                    }
                }
                strip.show();
                xSemaphoreGive(neoPixelMutex);
            }
        }
        fadeState = FADE_NONE;
    }
}
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
    
    fadeColor = finalColor;
    Serial.printf("--> handleFreundschaftMessage: Activated 3rd LED mode. final value: %u\n", finalColor);

    startFadeIn(finalColor, 1, isRainbow, isBlink);
    ledTimeout = millis() + duration;
    ledActive = true; 
}
// --- Funktion zum Laden der Konfiguration von SD ---

// --- Webserver & Config Portal ---
const char* DEFAULT_ROOT_CA = \
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

String getHtmlPage() {
    String page;
    page.reserve(12000);
    page += "<html><head><title>Zwitscherbox Konfiguration</title>";
    page += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    page += "<meta charset='UTF-8'><style>";
    
    // Modernes CSS
    page += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background-color:#f0f2f5;color:#1c1e21;margin:0;padding:20px;}";
    page += "h1{text-align:center;color:#2e7d32;margin-bottom:30px;font-weight:300;}";
    page += "form{max-width:600px;margin:0 auto;}";
    page += ".card{background:#fff;padding:20px;border-radius:12px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin-bottom:20px;}";
    page += "h2{font-size:1.2rem;color:#2e7d32;margin-top:0;border-bottom:2px solid #e8f5e9;padding-bottom:10px;margin-bottom:20px;}";
    page += ".field{margin-bottom:15px;}";
    page += "label{display:block;margin-bottom:6px;font-weight:600;font-size:0.9rem;}";
    page += "input[type=text],input[type=password],input[type=number],textarea{width:100%;padding:12px;border:1px solid #ddd;border-radius:8px;font-size:16px;transition:border-color 0.3s;}";
    page += "input:focus{outline:none;border-color:#4CAF50;background:#fafafa;}";
    page += "input[type=color]{width:100%;height:45px;border:1px solid #ddd;border-radius:8px;cursor:pointer;background:white;padding:4px;}";
    page += "textarea{height:150px;font-family:monospace;font-size:12px;resize:vertical;}";
    page += ".help-text{font-size:0.8rem;color:#666;margin-top:-8px;margin-bottom:15px;line-height:1.3;}";
    
    // Checkbox Styling
    page += ".row{display:flex;align-items:center;gap:10px;margin:15px 0 5px 0;}";
    page += "input[type=checkbox]{width:20px;height:20px;accent-color:#4CAF50;}";
    
    // Button Styling
    page += "input[type=submit]{background-color:#2e7d32;color:white;padding:15px;border:none;border-radius:8px;cursor:pointer;width:100%;font-size:18px;font-weight:bold;margin-top:10px;box-shadow:0 4px 6px rgba(46,125,50,0.2);transition:0.2s;}";
    page += "input[type=submit]:hover{background-color:#1b5e20;transform:translateY(-1px);}";
    page += "input[type=submit]:active{transform:translateY(1px);}";
    
    page += "</style></head><body>";
    page += "<h1>Zwitscherbox</h1>";
    page += "<div style='text-align:center;margin-bottom:20px;'><a href='/files' style='display:inline-block;background:#1976D2;color:white;padding:10px 20px;text-decoration:none;border-radius:8px;font-weight:bold;'>🎵 Zum SD-Karten Dateimanager</a></div>";
    page += "<form action='/save' method='POST'>";

    // Hilfsfunktionen (angepasst an das neue Layout mit Erklärungen)
    auto addTextField = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<input type='text' id='" + id + "' name='" + id + "' value='" + value + "'></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addPasswordField = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<input type='password' id='" + id + "' name='" + id + "' value='" + value + "'></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addNumberField = [&](const String& id, const String& label, int value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<input type='number' id='" + id + "' name='" + id + "' value='" + String(value) + "'></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addCheckbox = [&](const String& id, const String& label, bool checked, const String& desc = "") {
        page += "<div class='row'><input type='checkbox' id='" + id + "' name='" + id + "' value='1' " + (checked ? "checked" : "") + ">";
        page += "<label for='" + id + "'> " + label + "</label></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addTextArea = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<textarea id='" + id + "' name='" + id + "'>" + value + "</textarea></div>";
        if (desc.length() > 0) page += "<div class='help-text'>" + desc + "</div>";
    };
    auto addColorPicker = [&](const String& id, const String& label, const String& value, const String& desc = "") {
        String hexColor = value;
        if (hexColor.equalsIgnoreCase("RAINBOW")) {
            hexColor = "#000000";
        } else if (!hexColor.startsWith("#") && hexColor.length() > 0) {
            while (hexColor.length() < 6) hexColor = "0" + hexColor;
            hexColor = "#" + hexColor;
        }
        page += "<div class='field'><label for='" + id + "'>" + label + "</label>";
        page += "<div style='display:flex; gap:10px;'>";
        page += "<input type='color' style='width:60px; height:45px; padding:0; border:1px solid #ddd; border-radius:8px; cursor:pointer;' id='" + id + "_color' value='" + hexColor + "'";
        page += " oninput=\"document.getElementById('" + id + "').value = this.value\">";
        page += "<input type='text' id='" + id + "' name='" + id + "' value='" + value + "'";
        page += " oninput=\"if(this.value.match(/^#[0-9a-fA-F]{6}$/)) document.getElementById('" + id + "_color').value = this.value\">";
        page += "</div>";
        if (desc.length() > 0) page += "<div class='help-text' style='margin-top:5px;'>" + desc + "</div>";
        page += "</div>";
    };

    // Sektionen
    page += "<div class='card'><h2>WLAN Einstellungen</h2>";
    int n = WiFi.scanNetworks();
    page += "<datalist id='wifi-networks'>";
    for (int i = 0; i < n; ++i) {
        page += "<option value='" + WiFi.SSID(i) + "'>";
    }
    page += "</datalist>";
    WiFi.scanDelete();

    page += "<div class='field'><label for='WIFI_SSID'>Netzwerk Name</label>";
    page += "<input type='text' id='WIFI_SSID' name='WIFI_SSID' value='" + wifi_ssid + "' list='wifi-networks'></div>";
    page += "<div class='help-text'>Wie heißt dein normales WLAN zu Hause?</div>";
    addPasswordField("WIFI_PASS", "Passwort", wifi_pass, "Das Passwort für dein WLAN, damit die Box online gehen kann.");
    page += "</div>";

    page += "<div class='card'><h2>Administrator & Sicherheit</h2>";
    addPasswordField("ADMIN_PASS", "Webinterface Passwort", admin_pass, "Sichert diese Weboberfläche mit einem Passwort ab (optional, Nutzername ist immer 'admin').");
    page += "</div>";

    page += "<div class='card'><h2>Home Assistant (MQTT)</h2>";
    addCheckbox("MQTT_INTEGRATION", "MQTT Aktivieren", homeassistant_mqtt_enabled, "Aktiviert die Steuerung und Statusmeldungen für Smart Home Systeme.");
    addTextField("MQTT_SERVER", "Broker Adresse", mqtt_server);
    addNumberField("MQTT_PORT", "Port", mqtt_port);
    addTextField("MQTT_USER", "Benutzername", mqtt_user);
    addPasswordField("MQTT_PASS", "Passwort", mqtt_pass);
    addTextField("MQTT_CLIENT_ID", "Client ID", mqtt_client_id, "Einzigartiger Name dieser Box im Netzwerk.");
    addTextField("MQTT_BASE_TOPIC", "Basis-Pfad (Topic)", mqtt_base_topic, "Der Haupt-Pfad, über den Home Assistant mit der Box spricht.");
    page += "</div>";

    page += "<div class='card'><h2>Freundschaftslampe</h2>";
    addCheckbox("FRIENDLAMP_ENABLE", "LED Hardware aktivieren", friendlamp_enabled, "Nur anhaken, wenn ein LED-Ring angeschlossen ist!");
    addCheckbox("FRIENDLAMP_MQTT_INTEGRATION", "MQTT Modus aktivieren", friendlamp_mqtt_enabled, "Vernetzt deine Box über das Internet mit den Boxen deiner Freunde.");
    addColorPicker("FRIENDLAMP_COLOR", "Wähle deine Farbe", friendlamp_color, "In dieser Farbe leuchten die Lampen deiner Freunde, wenn DU vor deiner Box stehst.");
    addTextField("FRIENDLAMP_TOPIC", "Topic Freundschaft", friendlamp_topic, "Das Topic zum Senden/Empfangen der Signale deiner Freunde wenn sie eine Freundschaftslampe haben.");
    addTextField("ZWITSCHERBOX_TOPIC", "Topic Zwitscherbox", zwitscherbox_topic, "Das Topic zum Senden/Empfangen der Signale deiner Freunde wenn sie eine Zwitscherbox haben.");
    addCheckbox("LED_FADE_EFFECT", "Sanftes Ein-/Ausblenden", led_fade_effect, "Nutzt weiche Übergänge für die LEDs anstatt sie hart ein- und auszuschalten.");
    addNumberField("LED_FADE_DURATION", "Dauer (ms)", fadeDuration, "Dauer des Farbwechsels in Millisekunden (1000 = 1 Sekunde).");
    addNumberField("LED_BRIGHTNESS", "Helligkeit (0-255)", led_brightness, "Maximale Helligkeit des LED-Rings.");
    addNumberField("LED_COUNT", "Anzahl NeoPixel LEDs", led_count, "Anzahl der verlöteten LEDs auf dem verbauten Ring.");
    page += "</div>";

    page += "<div class='card'><h2>Externer Broker (Optional)</h2>";
    addTextField("FRIENDLAMP_MQTT_SERVER", "Server-URL", friendlamp_mqtt_server, "Trage hier deinen eigenen Internet-Broker ein (falls genutzt).");
    addNumberField("FRIENDLAMP_MQTT_PORT", "Port", friendlamp_mqtt_port);
    addTextField("FRIENDLAMP_MQTT_USER", "Benutzer", friendlamp_mqtt_user);
    addPasswordField("FRIENDLAMP_MQTT_PASS", "Passwort", friendlamp_mqtt_pass);
    addCheckbox("FRIENDLAMP_MQTT_TLS_ENABLED", "TLS Verschlüsselung nutzen", friendlamp_mqtt_tls_enabled, "Sichert die Verbindung ab. In der Regel für öffentliche MQTT Broker empfohlen!");
    String ca = mqtt_root_ca_content.length() > 0 ? mqtt_root_ca_content : DEFAULT_ROOT_CA;
    addTextArea("FRIENDLAMP_MQTT_ROOT_CA", "Root CA Zertifikat", ca, "Das Stammzertifikat des Servers für die verschlüsselte Verbindung.");
    page += "</div>";

    page += "<input type='submit' value='Konfiguration Speichern'>";
    page += "</form><div style='height:40px;'></div></body></html>";
    return page;
}

String getFileManagerHtml() {
    String page;
    page.reserve(8000);
    page += "<!DOCTYPE html><html><head><title>Dateimanager</title>";
    page += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    page += "<style>";
    page += "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background-color:#f0f2f5;color:#1c1e21;margin:0;padding:20px;}";
    page += "h1{color:#2e7d32;margin-bottom:10px;}";
    page += ".card{background:#fff;padding:20px;border-radius:12px;box-shadow:0 2px 4px rgba(0,0,0,0.1);margin-bottom:20px;}";
    page += ".btn{background:#2e7d32;color:white;padding:10px 15px;border:none;border-radius:8px;cursor:pointer;font-weight:bold;margin-right:10px;}";
    page += ".btn-danger{background:#d32f2f;}";
    page += "li{padding:10px;border-bottom:1px solid #ddd;display:flex;justify-content:space-between;align-items:center;}";
    page += "ul{list-style:none;padding:0;}";
    page += "#progress{width:100%;background:#e0e0e0;border-radius:8px;margin-top:10px;display:none;}";
    page += "#bar{width:0%;height:20px;background:#4CAF50;border-radius:8px;}";
    page += "input[type=file], input[type=text]{padding:10px;margin-bottom:10px;width:calc(100% - 20px);border:1px solid #ccc;border-radius:8px;}";
    page += "</style></head><body>";
    page += "<div class='card'>";
    page += "<div style='display:flex;justify-content:space-between;align-items:center;'><h1>SD-Karten Manager</h1><a href='/' class='btn' style='text-decoration:none;'>Setup</a></div>";
    page += "<h3>Aktueller Pfad: <span id='currentPath'>/</span></h3>";
    page += "<button class='btn' onclick='loadDir(\"/\")'>Root</button>";
    page += "<button class='btn' onclick='goUp()'>Nach Oben</button></div>";

    page += "<div class='card'><h3>📂 Ordner erstellen</h3>";
    page += "<input type='text' id='newDirName' placeholder='Name des neuen Ordners'>";
    page += "<button class='btn' onclick='createFolder()'>Erstellen</button></div>";

    page += "<div class='card'><h3>⬆️ MP3 Hochladen</h3>";
    page += "<input type='file' id='fileInput'>";
    page += "<button class='btn' onclick='uploadFile()'>Hochladen</button>";
    page += "<div id='progress'><div id='bar'></div></div></div>";

    page += "<div class='card'><h3>Inhalt</h3>";
    page += "<ul id='fileList'><li>Lade...</li></ul></div>";

    page += "<script>";
    page += "let currentPath = '/';";
    page += "function goUp() { let parts = currentPath.split('/'); parts.pop(); parts.pop(); let p = parts.join('/') + '/'; loadDir(p.length > 1 ? p : '/'); }";
    page += "function loadDir(path) { currentPath = path.endsWith('/') ? path : path + '/'; document.getElementById('currentPath').innerText = currentPath;";
    page += "fetch('/api/list?dir=' + encodeURIComponent(currentPath)).then(r=>r.json()).then(data=>{";
    page += "let html=''; data.forEach(item=>{";
    page += "let isDir = item.isDir; let size = item.size?Math.round(item.size/1024)+' KB':'';";
    page += "let icon = isDir ? '📁' : '🎵';";
    page += "let action = isDir ? `<button class='btn' onclick='loadDir(\"${currentPath}${item.name}\")'>&Ouml;ffnen</button>` : '';";
    page += "html+=`<li><span>${icon} ${item.name} <small>${size}</small></span><div>${action}<button class='btn btn-danger' onclick='deleteItem(\"${currentPath}${item.name}\")'>X</button></div></li>`;";
    page += "}); document.getElementById('fileList').innerHTML=html; }); }";
    
    page += "function deleteItem(path) { if(confirm('Sicher l\\u00f6schen? ' + path)){ fetch('/api/delete?path='+encodeURIComponent(path),{method:'DELETE'}).then(r=>{if(r.ok)loadDir(currentPath);else alert('Fehler beim L\\u00f6schen');}); } }";
    
    page += "function createFolder() { let name = document.getElementById('newDirName').value; if(name){ fetch('/api/mkdir?path='+encodeURIComponent(currentPath+name),{method:'POST'}).then(r=>{if(r.ok){document.getElementById('newDirName').value='';loadDir(currentPath);}else alert('Fehler');}); } }";
    
    page += "function uploadFile() { let file = document.getElementById('fileInput').files[0]; if(!file)return; let data = new FormData(); data.append('file', file, currentPath + file.name);";
    page += "let r = new XMLHttpRequest(); r.open('POST', '/api/upload');";
    page += "r.upload.onprogress = function(e){ if(e.lengthComputable){ document.getElementById('progress').style.display='block'; let p = (e.loaded/e.total)*100; document.getElementById('bar').style.width=Math.round(p)+'%'; } };";
    page += "r.onload = function(){ document.getElementById('progress').style.display='none'; document.getElementById('bar').style.width='0%'; loadDir(currentPath); };";
    page += "r.send(data); }";

    page += "window.onload = function() { loadDir('/'); };";
    page += "</script></body></html>";
    return page;
}

void handleSave(AsyncWebServerRequest *request) {
    File configFile = SD.open("/config.txt", FILE_WRITE);
    if (!configFile) {
        request->send(500, "text/plain", "Fehler beim Öffnen der config.txt zum Schreiben.");
        return;
    }
    configFile.println("# Zwitscher - Configuration File (Web-Generated)");
    
    auto writeParam = [&](const String& key, const String& webName) {
        if (request->hasParam(webName.c_str(), true)) {
            String value = request->getParam(webName.c_str(), true)->value();
            configFile.println(key + "=" + value);
        }
    };
    auto writeCheckbox = [&](const String& key, const String& webName) {
        String value = "0";
        if (request->hasParam(webName.c_str(), true)) {
             if(request->getParam(webName.c_str(), true)->value() == "1") value = "1";
        }
         configFile.println(key + "=" + value);
    };

    // WLAN
    writeParam("WIFI_SSID", "WIFI_SSID");
    writeParam("WIFI_PASS", "WIFI_PASS");
    
    // Administrator
    writeParam("ADMIN_PASS", "ADMIN_PASS");

    // Home Assistant
    writeCheckbox("MQTT_INTEGRATION", "MQTT_INTEGRATION");
    writeParam("MQTT_SERVER", "MQTT_SERVER");
    writeParam("MQTT_PORT", "MQTT_PORT");
    writeParam("MQTT_USER", "MQTT_USER");
    writeParam("MQTT_PASS", "MQTT_PASS");
    writeParam("MQTT_CLIENT_ID", "MQTT_CLIENT_ID");
    writeParam("MQTT_BASE_TOPIC", "MQTT_BASE_TOPIC");

    // Freundschaftslampe
    writeCheckbox("FRIENDLAMP_ENABLE", "FRIENDLAMP_ENABLE");
    writeCheckbox("FRIENDLAMP_MQTT_INTEGRATION", "FRIENDLAMP_MQTT_INTEGRATION");
    if (request->hasParam("FRIENDLAMP_COLOR", true)) {
        String colorValue = request->getParam("FRIENDLAMP_COLOR", true)->value();
        if (colorValue.startsWith("#")) colorValue = colorValue.substring(1); // '#' entfernen
        colorValue.toUpperCase(); // Konsistent in Großbuchstaben umwandeln
        configFile.println("FRIENDLAMP_COLOR=" + colorValue);
    }
    writeParam("FRIENDLAMP_TOPIC", "FRIENDLAMP_TOPIC");
    writeParam("ZWITSCHERBOX_TOPIC", "ZWITSCHERBOX_TOPIC");
    writeCheckbox("LED_FADE_EFFECT", "LED_FADE_EFFECT");
    writeParam("LED_FADE_DURATION", "LED_FADE_DURATION");
    writeParam("LED_BRIGHTNESS", "LED_BRIGHTNESS");
    writeParam("LED_COUNT", "LED_COUNT");

    // Externer Broker
    writeParam("FRIENDLAMP_MQTT_SERVER", "FRIENDLAMP_MQTT_SERVER");
    writeParam("FRIENDLAMP_MQTT_PORT", "FRIENDLAMP_MQTT_PORT");
    writeParam("FRIENDLAMP_MQTT_USER", "FRIENDLAMP_MQTT_USER");
    writeParam("FRIENDLAMP_MQTT_PASS", "FRIENDLAMP_MQTT_PASS");
    writeCheckbox("FRIENDLAMP_MQTT_TLS_ENABLED", "FRIENDLAMP_MQTT_TLS_ENABLED");
    if (request->hasParam("FRIENDLAMP_MQTT_ROOT_CA", true)) {
        configFile.println("BEGIN_CERT");
        configFile.print(request->getParam("FRIENDLAMP_MQTT_ROOT_CA", true)->value());
        configFile.println("\nEND_CERT");
    }

    configFile.close();
    
    String response = "<html><head><title>Gespeichert</title><meta charset='UTF-8'></head><body><h1>Konfiguration gespeichert!</h1><p>Das Ger&auml;t wird jetzt neu gestartet. Bitte verbinde dich mit deinem normalen WLAN und schlie&szlig;e dieses Fenster.</p>";
    response += "<p>In 5 Sekunden wird versucht, die Seite neu zu laden...</p><meta http-equiv='refresh' content='5;url=/' /></body></html>";
    request->send(200, "text/html", response);

    // KORREKTUR: Den ESP über den main loop neu starten, damit der Browser die Bestätigung empfangen kann!
    pendingRestart = true;
    restartTime = millis() + 2000; // Dem Server 2 Sekunden Zeit geben, die Seite zu schicken
}

bool checkAuth(AsyncWebServerRequest *request) {
    if (admin_pass.length() > 0 && !request->authenticate("admin", admin_pass.c_str())) {
        request->requestAuthentication();
        return false;
    }
    return true;
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(200, "text/html", getHtmlPage());
  });

  server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    handleSave(request);
  });

  server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(200, "text/html", getFileManagerHtml());
  });

  server.on("/api/list", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    String dirPath = request->hasParam("dir") ? request->getParam("dir")->value() : "/";
    File dir = SD.open(dirPath);
    if (!dir || !dir.isDirectory()) return request->send(400, "application/json", "[]");
    String json = "[";
    File file = dir.openNextFile();
    bool first = true;
    while(file){
        if(!first) json += ",";
        String name = file.name();
        if(name.lastIndexOf('/') >= 0) name = name.substring(name.lastIndexOf('/')+1);
        json += "{\"name\":\"" + name + "\",\"isDir\":" + (file.isDirectory()?"true":"false") + ",\"size\":" + String(file.size()) + "}";
        first = false;
        file = dir.openNextFile();
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/api/mkdir", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    if(request->hasParam("path")){
        String path = request->getParam("path")->value();
        if(SD.mkdir(path)) request->send(200);
        else request->send(500);
    } else request->send(400);
  });

  server.on("/api/delete", HTTP_DELETE, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    if(request->hasParam("path")){
        String path = request->getParam("path")->value();
        File f = SD.open(path);
        bool success = false;
        if(f){
            bool isDir = f.isDirectory();
            f.close();
            if(isDir) success = SD.rmdir(path);
            else success = SD.remove(path);
        }
        if(success) request->send(200);
        else request->send(500);
    } else request->send(400);
  });

  server.on("/api/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    if (!checkAuth(request)) return;
    request->send(200, "text/plain", "Upload done");
  }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
    if (admin_pass.length() > 0 && !request->authenticate("admin", admin_pass.c_str())) return;
    File *f = (File *)request->_tempObject;
    if(!index){
        if(!filename.startsWith("/")) filename = "/" + filename;
        f = new File(SD.open(filename, FILE_WRITE));
        request->_tempObject = (void*)f;
    }
    if (f) {
        if(len) f->write(data, len);
        if(final){
            f->close();
            delete f;
            request->_tempObject = NULL;
        }
    }
  });
  
  server.onNotFound([](AsyncWebServerRequest *request) {
    if (!checkAuth(request)) return;
    request->send(200, "text/html", getHtmlPage());
  });

  server.begin();
  Serial.println("HTTP server started.");
}

void startConfigPortal(){
  apMode = true;
  Serial.println("Starting Access Point 'Zwitscherbox'");
  Serial.println("Password for Access Point is: 12345678");
  WiFi.softAP("Zwitscherbox", "12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(IP);

  dns.start(53, "*", IP);

  setupWebServer();
}

void loadConfig() {
    File configFile = SD.open("/config.txt");
    if (!configFile) {
        Serial.println("ERROR: config.txt not found on SD card!");
        return;
    }

    Serial.println("Reading config.txt...");
    // Setze Standardwerte für den Fall, dass Keys fehlen
    homeassistant_mqtt_enabled = false;
    friendlamp_mqtt_enabled = false;
    friendlamp_mqtt_tls_enabled = false;
    mqtt_root_ca_content = "";
    mqtt_client_id = "ESP32_AudioPlayer";
    mqtt_base_topic = "audioplayer";
    mqtt_port = 1883;

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
            continue; // Leere Zeilen oder Kommentare ignorieren
        }

        if (line == "BEGIN_CERT") {
            inCertBlock = true;
            mqtt_root_ca_content = ""; // Clear previous content
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

        if (key == "ADMIN_PASS") {
            admin_pass = value;
        } else if (key == "WIFI_SSID") {
            wifi_ssid = value;
        } else if (key == "WIFI_PASS") {
            wifi_pass = value;
        } else if (key == "MQTT_SERVER") {
            mqtt_server = value;
        } else if (key == "MQTT_PORT") {
            mqtt_port = value.toInt();
            if (mqtt_port == 0) mqtt_port = 1883;
        } else if (key == "MQTT_USER") {
            mqtt_user = value;
        } else if (key == "MQTT_PASS") {
            mqtt_pass = value;
        } else if (key == "MQTT_CLIENT_ID") {
            mqtt_client_id = value;
        } else if (key == "MQTT_BASE_TOPIC") {
            mqtt_base_topic = value;
        } else if (key == "MQTT_INTEGRATION") {
            homeassistant_mqtt_enabled = (value == "1");
        } else if (key == "FRIENDLAMP_MQTT_TLS_ENABLED") {
            friendlamp_mqtt_tls_enabled = (value == "1");
        } else if (key == "FRIENDLAMP_MQTT_INTEGRATION") {
            friendlamp_mqtt_enabled = (value == "1");
        } else if (key == "FRIENDLAMP_ENABLE") {
            friendlamp_enabled = (value == "1");
        } else if (key == "FRIENDLAMP_COLOR") {
            friendlamp_color = value;
        } else if (key == "FRIENDLAMP_TOPIC") {
            friendlamp_topic = value;
        } else if (key == "ZWITSCHERBOX_TOPIC") {
            zwitscherbox_topic = value;
        } else if (key == "FRIENDLAMP_MQTT_SERVER") {
            friendlamp_mqtt_server = value;
        } else if (key == "FRIENDLAMP_MQTT_PORT") {
            friendlamp_mqtt_port = value.toInt();
            if (friendlamp_mqtt_port == 0) friendlamp_mqtt_port = 1883;
        } else if (key == "FRIENDLAMP_MQTT_USER") {
            friendlamp_mqtt_user = value;
        } else if (key == "FRIENDLAMP_MQTT_PASS") {
            friendlamp_mqtt_pass = value;
        } else if (key == "LED_FADE_EFFECT") {
            led_fade_effect = (value == "1");
        } else if (key == "LED_FADE_DURATION") {
            int new_duration = value.toInt();
            if (new_duration > 0) fadeDuration = new_duration;
        } else if (key == "LED_BRIGHTNESS") {
            int new_brightness = value.toInt();
            if (new_brightness >= 0 && new_brightness <= 255) {
                led_brightness = new_brightness;
            }
        } else if (key == "LED_COUNT") {
            int new_count = value.toInt();
            if (new_count > 0) {
                led_count = new_count;
            }
        }
    }
    configFile.close();

    if (homeassistant_mqtt_enabled) {
        Serial.println("Home Assistant MQTT Integration: ENABLED");
        Serial.println("  HA-MQTT Server: " + mqtt_server + ":" + String(mqtt_port));
        Serial.println("  HA-MQTT User: " + mqtt_user);
        Serial.println("  HA-MQTT Client ID: " + mqtt_client_id);
        Serial.println("  HA-MQTT Base Topic: " + mqtt_base_topic);
        Serial.println("  HA-MQTT TLS: DISABLED");
        // Dynamische Topics erstellen
        mqtt_topic_status = mqtt_base_topic + "/status";
        mqtt_topic_error = mqtt_base_topic + "/error";
        mqtt_topic_debug = mqtt_base_topic + "/debug";
        mqtt_topic_volume = mqtt_base_topic + "/volume";
        mqtt_topic_directory = mqtt_base_topic + "/directory";
        mqtt_topic_playing = mqtt_base_topic + "/playing";
        mqtt_topic_ip = mqtt_base_topic + "/ip_address";
    } else {
         Serial.println("Home Assistant MQTT Integration: DISABLED");
    }

    if (friendlamp_mqtt_enabled) {
        Serial.println("Friendship Lamp MQTT Integration: ENABLED");
         if(friendlamp_mqtt_tls_enabled) {
            Serial.println("  Friend-MQTT TLS: ENABLED");
            if(mqtt_root_ca_content.length() > 20) {
                 Serial.println("  Friend-MQTT Root CA: Loaded (" + String(mqtt_root_ca_content.length()) + " bytes)");
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

// --- MQTT Callbacks ---
void handleLampMessage(char* topic, byte* payload, unsigned int length) {
    Serial.println("--> handleLampMessage: Function called!");
    String message = "";
    for (int i = 0; i < length; i++) {
        message += (char)payload[i];
    }
    Serial.printf("Friendlamp MQTT Received [%s]: %s\n", topic, message.c_str());

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
    // Wird für den internen Broker genutzt (HA und ggf. Lampe)
    if (friendlamp_mqtt_enabled) {
        handleLampMessage(topic, payload, length);
    }
    // Hier könnten weitere Callbacks für die HA-Integration hin
}

void mqttLampCallback(char* topic, byte* payload, unsigned int length) {
    // Wird für den öffentlichen Broker genutzt
    handleLampMessage(topic, payload, length);
}
// -----------------------------------------------

// --- MQTT Wiederverbindungslogik ---
void mqtt_reconnect() {
    // --- Wiederverbindung für den internen Broker (Home Assistant) ---
    if (homeassistant_mqtt_enabled && !mqttClient.connected() && millis() - lastMqttReconnectAttempt > mqttReconnectInterval) {
        lastMqttReconnectAttempt = millis();

        mqttClient.setClient(espClient); // Immer den ungesicherten Client verwenden

        Serial.print("Attempting Internal (HA) MQTT connection...");
        bool connected = false;
        if (mqtt_user.length() > 0) {
             connected = mqttClient.connect(mqtt_client_id.c_str(), mqtt_user.c_str(), mqtt_pass.c_str());
        } else {
             connected = mqttClient.connect(mqtt_client_id.c_str());
        }

        if (connected) {
            Serial.println("connected");
            publishMqtt(mqtt_topic_ip, WiFi.localIP().toString(), true);
            publishMqtt(mqtt_topic_status, "Online", true); 
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
                 connected = mqttClientLamp.connect(lampClientId.c_str(), friendlamp_mqtt_user.c_str(), friendlamp_mqtt_pass.c_str());
            } else {
                 connected = mqttClientLamp.connect(lampClientId.c_str());
            }

            if (connected) {
                Serial.println("connected");
                Serial.println("--> LAMP MQTT: Subscribing to topics: " + friendlamp_topic + " and " + zwitscherbox_topic);
                mqttClientLamp.subscribe(friendlamp_topic.c_str());
                mqttClientLamp.subscribe(zwitscherbox_topic.c_str());
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
    randomSeed(analogRead(0));
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
#ifndef GLOBAL_CONFIG_H
#define GLOBAL_CONFIG_H

#include <Arduino.h>
// Audio und System
class Audio; // Forward declaration
extern Audio audio;
extern bool playing;
extern bool playingIntro;
extern bool inPlaybackSession;
extern const char* FW_VERSION;

#include <Preferences.h>
extern Preferences preferences;

extern std::vector<String> directoryList;
extern std::vector<String> currentMp3Files;
extern int currentDirectoryIndex;
extern String introFileName;

extern unsigned long playbackStartTime;
extern bool isStandby;

extern unsigned long lastPotReadTime;
extern const unsigned long potReadInterval;
extern int lastVolume;

extern int lastButtonState;
extern int buttonState;
extern unsigned long lastDebounceTime;
extern unsigned long debounceDelay;
extern unsigned long lastPirActivityTime;

extern float smoothedPotValue;
extern const float potSmoothingFactor;
extern const int POT_PIN;
extern const int PIR_PIN;
extern const int BUTTON_PIN;

// Webserver und Captive Portal State
extern bool apMode;
extern bool pendingRestart;
extern unsigned long restartTime;

// WLAN & Auth
extern String wifi_ssid;
extern String wifi_pass;
extern String admin_pass;

// MQTT
extern String mqtt_server;
extern int    mqtt_port;
extern String mqtt_user;
extern String mqtt_pass;
extern String mqtt_client_id;
extern String mqtt_base_topic;
extern bool   homeassistant_mqtt_enabled;
extern bool   friendlamp_mqtt_enabled;

extern bool   friendlamp_mqtt_tls_enabled;
extern String mqtt_root_ca_content;

extern String friendlamp_mqtt_server;
extern int    friendlamp_mqtt_port;
extern String friendlamp_mqtt_user;
extern String friendlamp_mqtt_pass;

// Hardware & Lampe
extern bool   friendlamp_enabled;
extern bool   led_fade_effect;
extern int    led_brightness;
extern int    led_count;
extern int    fadeDuration;
extern String friendlamp_color;
extern String friendlamp_topic;
extern String zwitscherbox_topic;
extern unsigned long ledTimeout;
extern uint32_t currentLedColor;
extern bool ledActive;

// Dynamische MQTT Topics
extern String mqtt_topic_status;
extern String mqtt_topic_error;
extern String mqtt_topic_debug;
extern String mqtt_topic_volume;
extern String mqtt_topic_directory;
extern String mqtt_topic_playing;
extern String mqtt_topic_ip;

#endif

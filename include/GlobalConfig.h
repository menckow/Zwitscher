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

#include "AppConfig.h"
extern AppConfig config;
extern unsigned long ledTimeout;
extern uint32_t currentLedColor;
extern bool ledActive;

// Dynamische MQTT Topics (jetzt in config.getTopic...())

#endif

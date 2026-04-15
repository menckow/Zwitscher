#pragma once
#include <Arduino.h>

// --- Central Hardware Pin Definitions ---
// Keeping pins centralized prevents duplicate values and bugs
// when changing hardware connections.

const int POT_PIN = 4;
const int PIR_PIN = 18;
const int BUTTON_PIN = 17; 
const int LED_PIN = 16;
const int SD_CS_PIN = SS;

// Default count, dynamic value is stored in AppConfig.led_count
const int DEFAULT_LED_COUNT = 16;

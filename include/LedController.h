#ifndef LED_CONTROLLER_H
#define LED_CONTROLLER_H

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "freertos/semphr.h"

extern Adafruit_NeoPixel strip;
extern SemaphoreHandle_t neoPixelMutex;

void startFadeIn(uint32_t color, int mode = 0, bool isRainbow = false, bool isBlink = false);
void startFadeOut();
void updateFade();

#endif

#include "LedController.h"
#include "GlobalConfig.h"

unsigned long ledTimeout = 0;
uint32_t currentLedColor = 0;
bool ledActive = false;

enum FadeState { FADE_NONE, FADE_IN, FADE_OUT, FADE_RAINBOW_SPIN, FADE_RAINBOW_OUT, FADE_BLINK };
FadeState fadeState = FADE_NONE;
uint32_t fadeColor = 0;
unsigned long fadeStartTime = 0;
// 0 = Normaler Ring, 1 = Jede 3. LED komplementär
int fadeRingMode = 0;

void startFadeIn(uint32_t color, int mode, bool isRainbow, bool isBlink) {
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
    
    if (config.led_fade_effect) {
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
    if (config.led_fade_effect) {
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
                uint32_t c = strip.Color(r * config.led_brightness / 255, g * config.led_brightness / 255, b * config.led_brightness / 255);
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
                float rainbowProgress = (float)(currentTime - fadeStartTime) / config.fadeDuration;
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
                uint8_t r = ((c >> 16) & 0xFF) * brightness * config.led_brightness / 255;
                uint8_t g = ((c >> 8) & 0xFF) * brightness * config.led_brightness / 255;
                uint8_t b = (c & 0xFF) * brightness * config.led_brightness / 255;
                strip.setPixelColor(i, strip.Color(r,g,b));
            }
            strip.show();
            xSemaphoreGive(neoPixelMutex);
        }
        return;
    }

    float progress = (float)(currentTime - fadeStartTime) / config.fadeDuration;
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

void setBootStatusLeds(int step, bool success) {
    if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
        if (step >= 0 && step < strip.numPixels()) {
            uint32_t color = success ? strip.Color(0, 255, 0) : strip.Color(255, 0, 0);
            strip.setPixelColor(step, color);
            strip.show();
        }
        xSemaphoreGive(neoPixelMutex);
    }
}

void setApModeLed(bool active) {
    if (xSemaphoreTake(neoPixelMutex, (TickType_t)10) == pdTRUE) {
        if (active) {
            for (int i = 0; i < strip.numPixels(); i++) {
                strip.setPixelColor(i, strip.Color(255, 0, 0));
            }
        } else {
            strip.clear();
        }
        strip.show();
        xSemaphoreGive(neoPixelMutex);
    }
}

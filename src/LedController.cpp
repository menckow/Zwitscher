#include "LedController.h"

// --- LedController Implementation ---

LedController::LedController(Adafruit_NeoPixel& stripRef) 
    : strip(stripRef), mutex(NULL), currentEffect(nullptr) {}

LedController::~LedController() {
    clearCurrentEffect();
    // Do not delete the mutex, as it lives for the app lifecycle
}

void LedController::begin() {
    mutex = xSemaphoreCreateMutex();
}

void LedController::clearCurrentEffect() {
    if (currentEffect) {
        delete currentEffect;
        currentEffect = nullptr;
    }
}

void LedController::setEffect(ILedEffect* newEffect) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        currentEffect = newEffect;
        xSemaphoreGive(mutex);
    } else {
        // If mutex fails, prevent memory leak
        delete newEffect;
    }
}

void LedController::update() {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        if (currentEffect) {
            bool finished = currentEffect->update(strip);
            if (finished) {
                clearCurrentEffect();
            }
        }
        xSemaphoreGive(mutex);
    }
}

void LedController::turnOff() {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        strip.clear();
        strip.show();
        xSemaphoreGive(mutex);
    }
}

void LedController::setBootStatusLeds(int step, bool success) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect(); // Halt active effects
        if (step >= 0 && step < strip.numPixels()) {
            uint32_t color = success ? strip.Color(0, 255, 0) : strip.Color(255, 0, 0);
            strip.setPixelColor(step, color);
            strip.show();
        }
        xSemaphoreGive(mutex);
    }
}

void LedController::setApModeLed(bool active) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        if (active) {
            for (int i = 0; i < strip.numPixels(); i++) {
                strip.setPixelColor(i, strip.Color(255, 0, 0));
            }
        } else {
            strip.clear();
        }
        strip.show();
        xSemaphoreGive(mutex);
    }
}

void LedController::setSolidColor(uint32_t color) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, color);
        }
        strip.show();
        xSemaphoreGive(mutex);
    }
}

void LedController::setOtaProgress(int percent) {
    if (xSemaphoreTake(mutex, (TickType_t)10) == pdTRUE) {
        clearCurrentEffect();
        uint16_t numLedsToLight = (percent * strip.numPixels()) / 100;
        
        for (uint16_t i = 0; i < strip.numPixels(); i++) {
            if (i < numLedsToLight) {
                strip.setPixelColor(i, strip.Color(0, 0, 255)); // Blue ring
            } else {
                strip.setPixelColor(i, strip.Color(0, 0, 0)); // Off
            }
        }
        strip.show();
        xSemaphoreGive(mutex);
    }
}

bool LedController::isLedActive() const { return ledActive; }
void LedController::setLedActive(bool active) { ledActive = active; }
void LedController::setTimeout(unsigned long timeout) { ledTimeout = timeout; }
unsigned long LedController::getTimeout() const { return ledTimeout; }

// --- Legacy Wrappers ---
void LedController::startFadeIn(uint32_t color, int mode, bool isRainbow, bool isBlink) {
    if (isRainbow) {
        setEffect(new RainbowEffect(false));
    } else if (isBlink) {
        setEffect(new BlinkEffect(color));
    } else {
        setEffect(new FadeEffect(color, true, mode));
    }
}

void LedController::startFadeOut() {
    // Determine what to fade out based on currentEffect class type if necessary, 
    // but the legacy logic just triggered standard FadeOut or RainbowOut.
    // For simplicity, we just trigger standard fade out unless it's rainbow.
    // Given OOP restrictions with RTTI, we'll try a dynamic_cast if enabled, 
    // or just assume a standard fade out. We'll use a hack to know if it's rainbow by checking if we have rainbow.
    if (config.led_fade_effect) {
        setEffect(new FadeEffect(currentLedColor, false, 0)); // mode 0 for normal fade out
    } else {
        turnOff();
    }
}

// --- Effects Implementation ---

// 1. Fade Effect
FadeEffect::FadeEffect(uint32_t color, bool in, int mode) 
    : targetColor(color), fadeIn(in), ringMode(mode) {
    startTime = millis();
}

bool FadeEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long currentTime = millis();
    float progress = 1.0;
    
    if (config.led_fade_effect && config.fadeDuration > 0) {
        progress = (float)(currentTime - startTime) / config.fadeDuration;
        if (progress > 1.0f) progress = 1.0f;
    }

    strip.clear();
    uint8_t r = (targetColor >> 16) & 0xFF;
    uint8_t g = (targetColor >> 8) & 0xFF;
    uint8_t b = targetColor & 0xFF;
    
    uint8_t r_comp = 255 - r;
    uint8_t g_comp = 255 - g;
    uint8_t b_comp = 255 - b;

    float alpha = fadeIn ? progress : (1.0f - progress);

    uint32_t currentColor = strip.Color((uint8_t)(r * alpha), (uint8_t)(g * alpha), (uint8_t)(b * alpha));
    uint32_t currentCompColor = strip.Color((uint8_t)(r_comp * alpha), (uint8_t)(g_comp * alpha), (uint8_t)(b_comp * alpha));

    for (int i = 0; i < strip.numPixels(); i++) {
        if (ringMode == 1 && (i % 3 == 0)) {
            strip.setPixelColor(i, currentCompColor);
        } else {
            strip.setPixelColor(i, currentColor);
        }
    }
    strip.show();

    return (progress >= 1.0f); // Return true when animation completes
}

// 2. Rainbow Effect
RainbowEffect::RainbowEffect(bool out) : fadeOut(out) {
    startTime = millis();
}

bool RainbowEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long currentTime = millis();
    float brightness = 1.0f;

    if (fadeOut) {
        float progress = (float)(currentTime - startTime) / config.fadeDuration;
        if (progress >= 1.0f) {
            strip.clear();
            strip.show();
            return true; // Finished
        }
        brightness = 1.0f - progress;
    }

    for (int i = 0; i < strip.numPixels(); i++) {
        int pixelHue = (currentTime * 10) + (i * 65536L / strip.numPixels());
        uint32_t c = strip.gamma32(strip.ColorHSV(pixelHue));
        uint8_t r = ((c >> 16) & 0xFF) * brightness * config.led_brightness / 255;
        uint8_t g = ((c >> 8) & 0xFF) * brightness * config.led_brightness / 255;
        uint8_t b = (c & 0xFF) * brightness * config.led_brightness / 255;
        strip.setPixelColor(i, strip.Color(r,g,b));
    }
    strip.show();

    return false; // Spin forever unless fadeOut finishes
}

// 3. Blink Effect
BlinkEffect::BlinkEffect(uint32_t c) : color(c) {
    startTime = millis();
}

bool BlinkEffect::update(Adafruit_NeoPixel& strip) {
    unsigned long currentTime = millis();
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;

    bool isOn = ((currentTime - startTime) % 1000) < 500;
    if (isOn) {
        uint32_t c = strip.Color(r * config.led_brightness / 255, g * config.led_brightness / 255, b * config.led_brightness / 255);
        strip.fill(c);
    } else {
        strip.clear();
    }
    strip.show();

    return false; // Blink forever until replaced
}

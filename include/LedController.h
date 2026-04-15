#pragma once

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "freertos/semphr.h"
#include "AppConfig.h"

extern AppConfig config;

// --- Interfaces for Strategy Pattern ---
class ILedEffect {
public:
    virtual ~ILedEffect() = default;
    
    // Returns gracefully if the effect is complete, allowing controller to delete it
    virtual bool update(Adafruit_NeoPixel& strip) = 0; 
};

// --- Standard Controller Class ---
class LedController {
private:
    Adafruit_NeoPixel& strip;
    SemaphoreHandle_t mutex;
    ILedEffect* currentEffect;

    void clearCurrentEffect();

public:
    LedController(Adafruit_NeoPixel& stripRef);
    ~LedController();
    void begin();

    void setEffect(ILedEffect* newEffect);
    void update();
    void turnOff();

    // Specific Status LEDs (immediate effect, overwrites current)
    void setBootStatusLeds(int step, bool success);
    void setApModeLed(bool active);
    
    // Legacy Hook wrapper
    void startFadeIn(uint32_t color, int mode = 0, bool isRainbow = false, bool isBlink = false);
    void startFadeOut();
    
    bool isLedActive() const;
    void setLedActive(bool active);
    void setTimeout(unsigned long timeout);
    unsigned long getTimeout() const;
    
    uint32_t currentLedColor = 0;
    unsigned long ledTimeout = 0;
    bool ledActive = false;
};

extern LedController ledCtrl;

// --- Effects ---
class FadeEffect : public ILedEffect {
private:
    uint32_t targetColor;
    bool fadeIn;
    int ringMode;
    unsigned long startTime;
public:
    FadeEffect(uint32_t color, bool in, int mode);
    bool update(Adafruit_NeoPixel& strip) override;
};

class RainbowEffect : public ILedEffect {
private:
    bool fadeOut;
    unsigned long startTime;
public:
    RainbowEffect(bool out);
    bool update(Adafruit_NeoPixel& strip) override;
};

class BlinkEffect : public ILedEffect {
private:
    uint32_t color;
    unsigned long startTime;
public:
    BlinkEffect(uint32_t c);
    bool update(Adafruit_NeoPixel& strip) override;
};

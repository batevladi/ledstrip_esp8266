#ifndef STRIP_MANAGER_H
#define STRIP_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "config_manager.h"

class StripManager {
public:
    StripManager();
    void begin(const DeviceConfig& config);
    CRGB* getPixels(uint8_t stripIndex);
    uint16_t getNumLeds(uint8_t stripIndex) const;
    uint8_t getNumActiveStrips() const;
    void setBrightness(uint8_t stripIndex, uint8_t brightness);
    void show();

private:
    CRGB _pixels[MAX_STRIPS][MAX_LEDS_PER_STRIP];
    uint16_t _numLeds[MAX_STRIPS];
    uint8_t _brightness[MAX_STRIPS];
    bool _enabled[MAX_STRIPS];
    uint8_t _numActiveStrips;
    void addStripByPin(uint8_t pin, uint8_t index);
};

#endif // STRIP_MANAGER_H

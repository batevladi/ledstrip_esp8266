#include "strip_manager.h"

StripManager::StripManager() : _numActiveStrips(0) {
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        _numLeds[i] = 0;
        _brightness[i] = DEFAULT_BRIGHTNESS;
        _enabled[i] = false;
    }
}

void StripManager::addStripByPin(uint8_t pin, uint8_t index) {
    switch (pin) {
        case 5:  FastLED.addLeds<WS2812B, 5, GRB>(_pixels[index], _numLeds[index]); break;
        case 4:  FastLED.addLeds<WS2812B, 4, GRB>(_pixels[index], _numLeds[index]); break;
        case 14: FastLED.addLeds<WS2812B, 14, GRB>(_pixels[index], _numLeds[index]); break;
        case 12: FastLED.addLeds<WS2812B, 12, GRB>(_pixels[index], _numLeds[index]); break;
        case 13: FastLED.addLeds<WS2812B, 13, GRB>(_pixels[index], _numLeds[index]); break;
        default:
            Serial.print(F("Invalid LED pin: "));
            Serial.println(pin);
            return;
    }
    Serial.print(F("Strip "));
    Serial.print(index);
    Serial.print(F(" on GPIO"));
    Serial.print(pin);
    Serial.print(F(" with "));
    Serial.print(_numLeds[index]);
    Serial.println(F(" LEDs"));
}

void StripManager::begin(const DeviceConfig& config) {
    _numActiveStrips = 0;
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        _enabled[i] = config.strips[i].enabled;
        _numLeds[i] = config.strips[i].numLeds;
        _brightness[i] = config.strips[i].brightness;
        if (_enabled[i] && _numLeds[i] > 0) {
            addStripByPin(config.strips[i].pin, i);
            _numActiveStrips++;
        }
    }
    FastLED.setBrightness(255);
}

void StripManager::show() {
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        if (_enabled[i] && _numLeds[i] > 0) {
            nscale8_video(_pixels[i], _numLeds[i], _brightness[i]);
        }
    }
    FastLED.show();
}

CRGB* StripManager::getPixels(uint8_t stripIndex) {
    if (stripIndex >= MAX_STRIPS) return nullptr;
    return _pixels[stripIndex];
}

uint16_t StripManager::getNumLeds(uint8_t stripIndex) const {
    if (stripIndex >= MAX_STRIPS) return 0;
    return _numLeds[stripIndex];
}

uint8_t StripManager::getNumActiveStrips() const { return _numActiveStrips; }

void StripManager::setBrightness(uint8_t stripIndex, uint8_t brightness) {
    if (stripIndex >= MAX_STRIPS) return;
    _brightness[stripIndex] = brightness;
}

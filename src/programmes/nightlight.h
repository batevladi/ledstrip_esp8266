#ifndef PROGRAMME_NIGHTLIGHT_H
#define PROGRAMME_NIGHTLIGHT_H

#include "../programme_engine.h"

class NightlightProgramme : public Programme {
public:
    const char* name() const override { return "nightlight"; }
    void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) override;
};

#endif

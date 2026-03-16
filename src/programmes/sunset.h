#ifndef PROGRAMME_SUNSET_H
#define PROGRAMME_SUNSET_H

#include "../programme_engine.h"

class SunsetProgramme : public Programme {
public:
    const char* name() const override { return "sunset"; }
    void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) override;
};

#endif

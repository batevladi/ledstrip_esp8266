#ifndef PROGRAMME_RAINBOW_H
#define PROGRAMME_RAINBOW_H

#include "../programme_engine.h"

class RainbowProgramme : public Programme {
public:
    const char* name() const override { return "rainbow"; }
    void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) override;
};

#endif

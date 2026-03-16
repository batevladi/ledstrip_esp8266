#ifndef PROGRAMME_SKY_AT_NIGHT_H
#define PROGRAMME_SKY_AT_NIGHT_H

#include "../programme_engine.h"

#define MAX_STARS 15

struct Star {
    uint16_t position;
    CRGB colour;
    uint32_t startMs;
    uint16_t durationMs;
    bool active;
};

class SkyAtNightProgramme : public Programme {
public:
    SkyAtNightProgramme();
    const char* name() const override { return "sky_at_night"; }
    void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) override;

private:
    Star _stars[MAX_STARS];
    uint32_t _lastSpawnMs;
    uint16_t _lastNumLeds;
    void spawnStar(uint16_t numLeds, uint32_t elapsedMs);
    uint8_t starBrightness(const Star& star, uint32_t elapsedMs) const;
};

#endif

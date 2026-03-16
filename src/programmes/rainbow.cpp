#include "rainbow.h"

static const uint32_t CYCLE_DURATION_MS = 5000;

void RainbowProgramme::render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) {
    uint8_t hueOffset = (elapsedMs * 256 / CYCLE_DURATION_MS) % 256;
    fill_rainbow(leds, numLeds, hueOffset, 256 / numLeds);
}

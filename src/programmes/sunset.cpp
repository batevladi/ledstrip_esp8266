#include "sunset.h"

static const CRGB SUNSET_COLOURS[] = {
    CRGB(255, 100, 0),
    CRGB(255, 50, 20),
    CRGB(220, 40, 80),
    CRGB(120, 20, 140),
};
static const uint8_t NUM_COLOURS = sizeof(SUNSET_COLOURS) / sizeof(SUNSET_COLOURS[0]);
static const uint32_t STEP_DURATION_MS = 4000;
static const uint32_t CYCLE_DURATION_MS = STEP_DURATION_MS * NUM_COLOURS;

void SunsetProgramme::render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) {
    uint32_t cyclePos = elapsedMs % CYCLE_DURATION_MS;
    uint8_t colourIndex = cyclePos / STEP_DURATION_MS;
    uint8_t nextIndex = (colourIndex + 1) % NUM_COLOURS;
    uint32_t stepPos = cyclePos % STEP_DURATION_MS;
    uint8_t blendAmount = (stepPos * 255) / STEP_DURATION_MS;
    CRGB colour = blend(SUNSET_COLOURS[colourIndex], SUNSET_COLOURS[nextIndex], blendAmount);
    fill_solid(leds, numLeds, colour);
}

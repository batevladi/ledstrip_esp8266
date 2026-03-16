#include "nightlight.h"

static const uint32_t BREATHE_CYCLE_MS = 4000;
static const uint8_t MIN_BRIGHTNESS = 30;
static const uint8_t MAX_BRIGHTNESS = 100;
static const CRGB BASE_COLOUR = CRGB(255, 180, 100);

void NightlightProgramme::render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) {
    uint8_t sinInput = (elapsedMs % BREATHE_CYCLE_MS) * 255 / BREATHE_CYCLE_MS;
    uint8_t sinVal = sin8(sinInput);
    uint8_t brightness = map(sinVal, 0, 255, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
    CRGB colour = BASE_COLOUR;
    colour.nscale8_video(brightness);
    fill_solid(leds, numLeds, colour);
}

#include "sky_at_night.h"

static const CRGB SKY_COLOUR = CRGB(5, 5, 40);
static const CRGB STAR_COLOURS[] = {
    CRGB(255, 255, 255),
    CRGB(255, 255, 200),
    CRGB(255, 240, 150),
};
static const uint8_t NUM_STAR_COLOURS = sizeof(STAR_COLOURS) / sizeof(STAR_COLOURS[0]);
static const uint32_t SPAWN_INTERVAL_MS = 300;
static const uint16_t STAR_MIN_DURATION_MS = 1500;
static const uint16_t STAR_MAX_DURATION_MS = 4000;

SkyAtNightProgramme::SkyAtNightProgramme()
    : _lastSpawnMs(0), _lastNumLeds(0) {
    for (uint8_t i = 0; i < MAX_STARS; i++) {
        _stars[i].active = false;
    }
}

void SkyAtNightProgramme::spawnStar(uint16_t numLeds, uint32_t elapsedMs) {
    for (uint8_t i = 0; i < MAX_STARS; i++) {
        if (!_stars[i].active) {
            _stars[i].position = random(numLeds);
            _stars[i].colour = STAR_COLOURS[random(NUM_STAR_COLOURS)];
            _stars[i].startMs = elapsedMs;
            _stars[i].durationMs = random(STAR_MIN_DURATION_MS, STAR_MAX_DURATION_MS);
            _stars[i].active = true;
            return;
        }
    }
}

uint8_t SkyAtNightProgramme::starBrightness(const Star& star, uint32_t elapsedMs) const {
    uint32_t age = elapsedMs - star.startMs;
    if (age >= star.durationMs) return 0;
    uint32_t third = star.durationMs / 3;
    if (age < third) {
        return (age * 255) / third;
    } else if (age < third * 2) {
        return 255;
    } else {
        uint32_t fadeAge = age - (third * 2);
        uint32_t fadeLen = star.durationMs - (third * 2);
        return 255 - (fadeAge * 255) / fadeLen;
    }
}

void SkyAtNightProgramme::render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) {
    if (numLeds != _lastNumLeds) {
        for (uint8_t i = 0; i < MAX_STARS; i++) {
            _stars[i].active = false;
        }
        _lastNumLeds = numLeds;
    }

    fill_solid(leds, numLeds, SKY_COLOUR);

    if (elapsedMs - _lastSpawnMs >= SPAWN_INTERVAL_MS) {
        spawnStar(numLeds, elapsedMs);
        _lastSpawnMs = elapsedMs;
    }

    for (uint8_t i = 0; i < MAX_STARS; i++) {
        if (!_stars[i].active) continue;
        uint8_t brightness = starBrightness(_stars[i], elapsedMs);
        if (brightness == 0) {
            _stars[i].active = false;
            continue;
        }
        if (_stars[i].position < numLeds) {
            CRGB starColour = _stars[i].colour;
            starColour.nscale8_video(brightness);
            leds[_stars[i].position] = starColour;
        }
    }
}

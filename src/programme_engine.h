#ifndef PROGRAMME_ENGINE_H
#define PROGRAMME_ENGINE_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

class Programme {
public:
    virtual ~Programme() {}
    virtual const char* name() const = 0;
    virtual void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) = 0;
};

class StripManager;

class ProgrammeEngine {
public:
    ProgrammeEngine();
    uint8_t registerProgramme(Programme* prog);
    void selectProgramme(uint8_t index);
    uint8_t getActiveProgramme() const;
    const char* getProgrammeName(uint8_t index) const;
    uint8_t getProgrammeCount() const;
    void tick(StripManager& strips);
    void resetTimer();

private:
    Programme* _programmes[NUM_BUILTIN_PROGRAMMES];
    uint8_t _programmeCount;
    uint8_t _activeProgramme;
    uint32_t _startTime;
};

#endif // PROGRAMME_ENGINE_H

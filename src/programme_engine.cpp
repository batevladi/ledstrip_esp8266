#include "programme_engine.h"
#include "strip_manager.h"

ProgrammeEngine::ProgrammeEngine()
    : _programmeCount(0), _activeProgramme(0), _startTime(0) {
    for (uint8_t i = 0; i < NUM_BUILTIN_PROGRAMMES; i++) {
        _programmes[i] = nullptr;
    }
}

uint8_t ProgrammeEngine::registerProgramme(Programme* prog) {
    if (_programmeCount >= NUM_BUILTIN_PROGRAMMES) {
        Serial.println(F("Programme registry full"));
        return 255;
    }
    uint8_t idx = _programmeCount;
    _programmes[idx] = prog;
    _programmeCount++;
    Serial.print(F("Registered programme: "));
    Serial.println(prog->name());
    return idx;
}

void ProgrammeEngine::selectProgramme(uint8_t index) {
    if (index >= _programmeCount) return;
    _activeProgramme = index;
    resetTimer();
    Serial.print(F("Selected programme: "));
    Serial.println(_programmes[index]->name());
}

uint8_t ProgrammeEngine::getActiveProgramme() const { return _activeProgramme; }

const char* ProgrammeEngine::getProgrammeName(uint8_t index) const {
    if (index >= _programmeCount || _programmes[index] == nullptr) return "unknown";
    return _programmes[index]->name();
}

uint8_t ProgrammeEngine::getProgrammeCount() const { return _programmeCount; }

void ProgrammeEngine::tick(StripManager& strips) {
    if (_activeProgramme >= _programmeCount) return;
    if (_programmes[_activeProgramme] == nullptr) return;
    uint32_t elapsed = millis() - _startTime;
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        uint16_t numLeds = strips.getNumLeds(i);
        if (numLeds == 0) continue;
        CRGB* pixels = strips.getPixels(i);
        if (pixels == nullptr) continue;
        _programmes[_activeProgramme]->render(pixels, numLeds, elapsed);
    }
}

void ProgrammeEngine::resetTimer() { _startTime = millis(); }

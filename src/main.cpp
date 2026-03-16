#include <Arduino.h>
#include "config.h"
#include "config_manager.h"
#include "strip_manager.h"
#include "programme_engine.h"
#include "wifi_manager.h"
#include "web_portal.h"

#include "programmes/sunset.h"
#include "programmes/rainbow.h"
#include "programmes/nightlight.h"
#include "programmes/sky_at_night.h"

// --- Global objects ---
ConfigManager configManager;
StripManager stripManager;
ProgrammeEngine programmeEngine;
WifiManager wifiManager;

// Programmes (static allocation — no heap)
SunsetProgramme progSunset;
RainbowProgramme progRainbow;
NightlightProgramme progNightlight;
SkyAtNightProgramme progSkyAtNight;

// Web portal (constructed after globals are ready)
WebPortal* webPortal = nullptr;

// State
bool portalMode = false;
uint32_t lastFrameMs = 0;

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println(F("=== LED Controller ==="));

    // 1. Load config from LittleFS (or defaults)
    configManager.load();
    Serial.print(F("Active programme: "));
    Serial.println(configManager.config().activeProgramme);

    // 2. Init strips from config
    stripManager.begin(configManager.config());

    // 3. Register built-in programmes (order must match PROG_* constants)
    programmeEngine.registerProgramme(&progSunset);
    programmeEngine.registerProgramme(&progRainbow);
    programmeEngine.registerProgramme(&progNightlight);
    programmeEngine.registerProgramme(&progSkyAtNight);

    // 4. Restore last-selected programme
    programmeEngine.selectProgramme(configManager.config().activeProgramme);

    // 5. Create web portal (needs references to managers)
    webPortal = new WebPortal(configManager, programmeEngine, stripManager);

    // 6. Attempt Wi-Fi connection
    bool wifiOk = wifiManager.connect(
        configManager.config().wifi.ssid,
        configManager.config().wifi.password
    );

    if (!wifiOk) {
        Serial.println(F("Entering captive portal mode"));
        wifiManager.startAP();
        webPortal->begin();
        portalMode = true;
    } else {
        // Wi-Fi OK — for Phase 1, no MQTT yet. Just run normally.
        Serial.println(F("Wi-Fi connected, running normally"));
        portalMode = false;
    }

    lastFrameMs = millis();
    Serial.println(F("Setup complete"));
}

void loop() {
    uint32_t now = millis();

    if (portalMode) {
        webPortal->handleClient();
    }

    // Run programme engine at target FPS
    if (now - lastFrameMs >= FRAME_INTERVAL_MS) {
        lastFrameMs = now;
        programmeEngine.tick(stripManager);
        stripManager.show();
    }

    yield();
}

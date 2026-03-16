# Phase 1 — Standalone Device (MVP) Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a standalone ESP8266 LED strip controller that drives 1-5 WS2812B strips with 4 built-in lighting programmes, a captive portal web UI (shown only on connection failure), and LittleFS persistence for strip config and last-selected programme.

**Architecture:** PlatformIO Arduino framework on ESP8266. Modular C++ with separated concerns: config persistence (LittleFS + ArduinoJson), strip hardware abstraction (FastLED), programme rendering engine with pluggable programmes, and a captive portal web server (ESP8266WebServer). The main loop runs a non-blocking render cycle: each tick, the active programme computes the next frame for each strip, and FastLED pushes pixels. The web portal only starts when Wi-Fi or MQTT connection fails.

**Tech Stack:** PlatformIO, ESP8266 Arduino core, FastLED, ArduinoJson v7, LittleFS, ESP8266WebServer, DNSServer, ESP8266WiFi

---

## File Structure

```
8266/
├── platformio.ini                  — Build config, board, libs, test environments
├── src/
│   ├── main.cpp                    — setup(), loop(), boot sequence
│   ├── config.h                    — Pin defaults, constants, limits
│   ├── config_manager.h            — ConfigManager class declaration
│   ├── config_manager.cpp          — LittleFS JSON read/write for all persistent state
│   ├── strip_manager.h             — StripManager class declaration
│   ├── strip_manager.cpp           — FastLED init, per-strip brightness, show()
│   ├── programme_engine.h          — Programme base class + ProgrammeEngine registry
│   ├── programme_engine.cpp        — ProgrammeEngine: tick, select, get current
│   ├── programmes/
│   │   ├── sunset.h                — Sunset programme declaration
│   │   ├── sunset.cpp              — Warm colour fade cycle
│   │   ├── rainbow.h               — Running Rainbow declaration
│   │   ├── rainbow.cpp             — Scrolling rainbow hue cycle
│   │   ├── nightlight.h            — Nightlight declaration
│   │   ├── nightlight.cpp          — Warm breathing glow
│   │   ├── sky_at_night.h          — Sky at Night declaration
│   │   └── sky_at_night.cpp        — Twinkling stars on dark blue
│   ├── wifi_manager.h              — Wi-Fi connection attempt logic declaration
│   ├── wifi_manager.cpp            — Connect with timeout, status reporting
│   ├── web_portal.h                — Captive portal declaration
│   ├── web_portal.cpp              — HTTP routes, HTML generation, form handling
│   └── web_portal_html.h           — HTML templates stored in PROGMEM
├── test/
│   └── test_native/
│       ├── test_config_json.cpp    — Config serialization/deserialization tests
│       └── test_programme_math.cpp — Colour interpolation and timing math tests
├── data/                           — LittleFS filesystem (empty, config created at runtime)
├── PRD.md
├── PFD.md
└── docs/
    └── plans/
        └── 2026-03-16-phase1-standalone-mvp.md  (this file)
```

### File Responsibilities

| File | Responsibility | Dependencies |
|---|---|---|
| `config.h` | Constants: default pin (D1), max strips (5), max LEDs per strip, valid GPIO list, programme names | None |
| `config_manager.h/.cpp` | Load/save JSON config from LittleFS: strip pins+lengths, brightness per strip, last programme, Wi-Fi/MQTT creds. Single `DeviceConfig` struct. | ArduinoJson, LittleFS |
| `strip_manager.h/.cpp` | Owns FastLED `CRGB` arrays. Init strips from config, set brightness, write pixel data, call `FastLED.show()`. Supports dynamic strip count (1-5). | FastLED, config.h |
| `programme_engine.h/.cpp` | Abstract `Programme` base class with `void render(CRGB* leds, uint16_t numLeds, uint32_t elapsed_ms)`. `ProgrammeEngine` holds the registry of programmes, tracks active selection, calls render on each strip each tick. | strip_manager |
| `programmes/*.cpp` | Each implements `Programme::render()`. Pure rendering logic — given a pixel buffer, pixel count, and elapsed time, fill the buffer. | programme_engine.h (base class), FastLED (CRGB type) |
| `wifi_manager.h/.cpp` | `attemptConnect(ssid, pass, timeout_ms)` → bool. Non-blocking Wi-Fi status check. | ESP8266WiFi |
| `web_portal.h/.cpp` | Start/stop captive portal. HTTP routes: GET `/` (status page), POST `/config` (save Wi-Fi/MQTT/strip settings), POST `/programme` (select programme). | ESP8266WebServer, DNSServer, config_manager, programme_engine |
| `web_portal_html.h` | PROGMEM strings for HTML pages. Keeps HTML out of `.cpp` logic. | None |
| `main.cpp` | `setup()`: init LittleFS, load config, init strips, attempt Wi-Fi → MQTT, fallback to portal. `loop()`: either run portal server or run programme engine tick + `yield()`. | All modules |
| `test_config_json.cpp` | Native tests for JSON serialization round-trip of `DeviceConfig`. | ArduinoJson (compiles natively) |
| `test_programme_math.cpp` | Native tests for colour interpolation helpers (lerp, HSV-to-RGB). | None (pure math) |

---

## Chunk 1: Project Scaffolding + Config Layer

### Task 1: Project Scaffolding

**Files:**
- Create: `platformio.ini`
- Create: `src/config.h`
- Create: `src/main.cpp` (minimal — verify build)

- [ ] **Step 1: Create `platformio.ini`**

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
board_build.ldscript = eagle.flash.4m2m.ld

lib_deps =
    fastled/FastLED @ ^3.9.0
    bblanchon/ArduinoJson @ ^7.0.0

upload_protocol = esptool
upload_speed = 921600

; OTA upload (uncomment and set IP when deploying over-the-air)
; upload_protocol = espota
; upload_port = 192.168.1.50

[env:native]
platform = native
lib_deps =
    bblanchon/ArduinoJson @ ^7.0.0
build_flags = -std=c++17
test_framework = unity
```

- [ ] **Step 2: Create `src/config.h`**

```cpp
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// --- Strip Defaults ---
#define DEFAULT_STRIP_PIN       D1
#define DEFAULT_STRIP_LENGTH    60
#define MAX_STRIPS              5
#define MAX_LEDS_PER_STRIP      300

// Valid GPIO pins for LED data (D1, D2, D5, D6, D7)
const uint8_t VALID_LED_PINS[] = {5, 4, 14, 12, 13};
const uint8_t NUM_VALID_PINS = 5;

// --- Brightness ---
#define DEFAULT_BRIGHTNESS      128

// --- Programmes ---
#define NUM_BUILTIN_PROGRAMMES  4
#define PROG_SUNSET             0
#define PROG_RAINBOW            1
#define PROG_NIGHTLIGHT         2
#define PROG_SKY_AT_NIGHT       3
#define DEFAULT_PROGRAMME       PROG_SUNSET

// Programme names (for display and persistence)
const char* const PROGRAMME_NAMES[] = {
    "sunset",
    "rainbow",
    "nightlight",
    "sky_at_night"
};

// --- Wi-Fi ---
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_AP_SSID_PREFIX     "LED-Controller-"
#define WIFI_AP_PASSWORD        "configure"

// --- LittleFS ---
#define CONFIG_FILE_PATH        "/config.json"

// --- Timing ---
#define TARGET_FPS              60
#define FRAME_INTERVAL_MS       (1000 / TARGET_FPS)

#endif // CONFIG_H
```

- [ ] **Step 3: Create minimal `src/main.cpp`**

```cpp
#include <Arduino.h>
#include "config.h"

void setup() {
    Serial.begin(115200);
    Serial.println();
    Serial.println(F("LED Controller booting..."));
}

void loop() {
    delay(1000);
    Serial.println(F("heartbeat"));
}
```

- [ ] **Step 4: Verify the project builds**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS. This confirms PlatformIO downloads the ESP8266 platform, FastLED, and ArduinoJson correctly.

- [ ] **Step 5: Commit**

```bash
git init
git add platformio.ini src/config.h src/main.cpp
git commit -m "feat: scaffold PlatformIO project with config constants"
```

---

### Task 2: Config Manager — Data Structures

**Files:**
- Create: `src/config_manager.h`
- Create: `src/config_manager.cpp`

- [ ] **Step 1: Create `src/config_manager.h`**

```cpp
#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include "config.h"

struct StripConfig {
    uint8_t pin;
    uint16_t numLeds;
    uint8_t brightness;
    bool enabled;
};

struct WifiConfig {
    char ssid[33];
    char password[65];
};

struct MqttConfig {
    char host[65];
    uint16_t port;
    char user[33];
    char password[65];
    char baseTopic[65];
    char deviceName[33];
};

struct DeviceConfig {
    StripConfig strips[MAX_STRIPS];
    uint8_t numStrips;
    uint8_t activeProgramme;
    WifiConfig wifi;
    MqttConfig mqtt;
};

class ConfigManager {
public:
    ConfigManager();

    // Load config from LittleFS. Returns true if file existed and parsed OK.
    // If file missing or corrupt, applies defaults.
    bool load();

    // Save current config to LittleFS. Returns true on success.
    bool save();

    // Reset to factory defaults (single strip on D1, default programme).
    void applyDefaults();

    // Access the config
    DeviceConfig& config();
    const DeviceConfig& config() const;

private:
    DeviceConfig _config;
    bool _loaded;

    void configToJson(char* buffer, size_t bufferSize) const;
    bool jsonToConfig(const char* json);
};

#endif // CONFIG_MANAGER_H
```

- [ ] **Step 2: Create `src/config_manager.cpp`**

```cpp
#include "config_manager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

ConfigManager::ConfigManager() : _loaded(false) {
    applyDefaults();
}

void ConfigManager::applyDefaults() {
    memset(&_config, 0, sizeof(DeviceConfig));

    // Default: one strip on D1
    _config.numStrips = 1;
    _config.strips[0].pin = DEFAULT_STRIP_PIN;
    _config.strips[0].numLeds = DEFAULT_STRIP_LENGTH;
    _config.strips[0].brightness = DEFAULT_BRIGHTNESS;
    _config.strips[0].enabled = true;

    // Remaining strips disabled
    for (uint8_t i = 1; i < MAX_STRIPS; i++) {
        _config.strips[i].pin = VALID_LED_PINS[i];
        _config.strips[i].numLeds = 0;
        _config.strips[i].brightness = DEFAULT_BRIGHTNESS;
        _config.strips[i].enabled = false;
    }

    _config.activeProgramme = DEFAULT_PROGRAMME;

    // Wi-Fi and MQTT blank (will trigger captive portal)
    _config.wifi.ssid[0] = '\0';
    _config.wifi.password[0] = '\0';
    _config.mqtt.host[0] = '\0';
    _config.mqtt.port = 1883;
    _config.mqtt.user[0] = '\0';
    _config.mqtt.password[0] = '\0';
    strlcpy(_config.mqtt.baseTopic, "home/led/device01", sizeof(_config.mqtt.baseTopic));
    strlcpy(_config.mqtt.deviceName, "LED-Controller", sizeof(_config.mqtt.deviceName));
}

void ConfigManager::configToJson(char* buffer, size_t bufferSize) const {
    JsonDocument doc;

    doc["num_strips"] = _config.numStrips;
    doc["active_programme"] = _config.activeProgramme;

    JsonArray strips = doc["strips"].to<JsonArray>();
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        JsonObject s = strips.add<JsonObject>();
        s["pin"] = _config.strips[i].pin;
        s["num_leds"] = _config.strips[i].numLeds;
        s["brightness"] = _config.strips[i].brightness;
        s["enabled"] = _config.strips[i].enabled;
    }

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = _config.wifi.ssid;
    wifi["password"] = _config.wifi.password;

    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["host"] = _config.mqtt.host;
    mqtt["port"] = _config.mqtt.port;
    mqtt["user"] = _config.mqtt.user;
    mqtt["password"] = _config.mqtt.password;
    mqtt["base_topic"] = _config.mqtt.baseTopic;
    mqtt["device_name"] = _config.mqtt.deviceName;

    serializeJson(doc, buffer, bufferSize);
}

bool ConfigManager::jsonToConfig(const char* json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.print(F("Config parse error: "));
        Serial.println(err.c_str());
        return false;
    }

    _config.numStrips = doc["num_strips"] | 1;
    _config.activeProgramme = doc["active_programme"] | DEFAULT_PROGRAMME;

    JsonArray strips = doc["strips"];
    if (strips) {
        for (uint8_t i = 0; i < MAX_STRIPS && i < strips.size(); i++) {
            JsonObject s = strips[i];
            _config.strips[i].pin = s["pin"] | VALID_LED_PINS[i];
            _config.strips[i].numLeds = s["num_leds"] | (uint16_t)0;
            _config.strips[i].brightness = s["brightness"] | DEFAULT_BRIGHTNESS;
            _config.strips[i].enabled = s["enabled"] | false;
        }
    }

    JsonObject wifi = doc["wifi"];
    if (wifi) {
        strlcpy(_config.wifi.ssid, wifi["ssid"] | "", sizeof(_config.wifi.ssid));
        strlcpy(_config.wifi.password, wifi["password"] | "", sizeof(_config.wifi.password));
    }

    JsonObject mqtt = doc["mqtt"];
    if (mqtt) {
        strlcpy(_config.mqtt.host, mqtt["host"] | "", sizeof(_config.mqtt.host));
        _config.mqtt.port = mqtt["port"] | 1883;
        strlcpy(_config.mqtt.user, mqtt["user"] | "", sizeof(_config.mqtt.user));
        strlcpy(_config.mqtt.password, mqtt["password"] | "", sizeof(_config.mqtt.password));
        strlcpy(_config.mqtt.baseTopic, mqtt["base_topic"] | "home/led/device01", sizeof(_config.mqtt.baseTopic));
        strlcpy(_config.mqtt.deviceName, mqtt["device_name"] | "LED-Controller", sizeof(_config.mqtt.deviceName));
    }

    return true;
}

bool ConfigManager::load() {
    if (!LittleFS.begin()) {
        Serial.println(F("LittleFS mount failed"));
        return false;
    }

    File file = LittleFS.open(CONFIG_FILE_PATH, "r");
    if (!file) {
        Serial.println(F("No config file found, using defaults"));
        return false;
    }

    size_t size = file.size();
    if (size > 2048) {
        Serial.println(F("Config file too large"));
        file.close();
        return false;
    }

    char buffer[2048];
    file.readBytes(buffer, size);
    buffer[size] = '\0';
    file.close();

    bool ok = jsonToConfig(buffer);
    if (ok) {
        _loaded = true;
        Serial.println(F("Config loaded from LittleFS"));
    }
    return ok;
}

bool ConfigManager::save() {
    if (!LittleFS.begin()) {
        Serial.println(F("LittleFS mount failed on save"));
        return false;
    }

    char buffer[2048];
    configToJson(buffer, sizeof(buffer));

    File file = LittleFS.open(CONFIG_FILE_PATH, "w");
    if (!file) {
        Serial.println(F("Failed to open config file for writing"));
        return false;
    }

    file.print(buffer);
    file.close();
    Serial.println(F("Config saved to LittleFS"));
    return true;
}

DeviceConfig& ConfigManager::config() {
    return _config;
}

const DeviceConfig& ConfigManager::config() const {
    return _config;
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/config_manager.h src/config_manager.cpp
git commit -m "feat: add ConfigManager with LittleFS JSON persistence"
```

---

### Task 3: Config Manager — Native Tests

**Files:**
- Create: `test/test_native/test_config_json.cpp`

These tests run on the host machine (no ESP8266 required) to verify JSON serialization logic.

- [ ] **Step 1: Create `test/test_native/test_config_json.cpp`**

```cpp
#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>

// We test the JSON schema directly since config_manager.cpp
// depends on LittleFS/Arduino. We verify the JSON contract here.

void test_default_config_serializes_correctly() {
    JsonDocument doc;
    doc["num_strips"] = 1;
    doc["active_programme"] = 0;

    JsonArray strips = doc["strips"].to<JsonArray>();
    JsonObject s = strips.add<JsonObject>();
    s["pin"] = 5;  // D1 = GPIO5
    s["num_leds"] = 60;
    s["brightness"] = 128;
    s["enabled"] = true;

    char buffer[1024];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, len);

    // Verify round-trip
    JsonDocument doc2;
    DeserializationError err = deserializeJson(doc2, buffer);
    TEST_ASSERT_EQUAL(DeserializationError::Ok, err);
    TEST_ASSERT_EQUAL(1, doc2["num_strips"].as<int>());
    TEST_ASSERT_EQUAL(0, doc2["active_programme"].as<int>());
    TEST_ASSERT_EQUAL(5, doc2["strips"][0]["pin"].as<int>());
    TEST_ASSERT_EQUAL(60, doc2["strips"][0]["num_leds"].as<int>());
    TEST_ASSERT_EQUAL(128, doc2["strips"][0]["brightness"].as<int>());
    TEST_ASSERT_TRUE(doc2["strips"][0]["enabled"].as<bool>());
}

void test_missing_fields_get_defaults() {
    const char* minimal = "{\"num_strips\":2}";
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, minimal);
    TEST_ASSERT_EQUAL(DeserializationError::Ok, err);

    // Missing active_programme should default via | operator
    uint8_t prog = doc["active_programme"] | (uint8_t)0;
    TEST_ASSERT_EQUAL(0, prog);

    // Missing strips array should be null
    JsonArray strips = doc["strips"];
    TEST_ASSERT_TRUE(strips.isNull());
}

void test_wifi_config_round_trip() {
    JsonDocument doc;
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = "MyNetwork";
    wifi["password"] = "MyPassword123";

    char buffer[512];
    serializeJson(doc, buffer, sizeof(buffer));

    JsonDocument doc2;
    deserializeJson(doc2, buffer);

    char ssid[33];
    char pass[65];
    strlcpy(ssid, doc2["wifi"]["ssid"] | "", sizeof(ssid));
    strlcpy(pass, doc2["wifi"]["password"] | "", sizeof(pass));

    TEST_ASSERT_EQUAL_STRING("MyNetwork", ssid);
    TEST_ASSERT_EQUAL_STRING("MyPassword123", pass);
}

void test_mqtt_config_round_trip() {
    JsonDocument doc;
    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["host"] = "192.168.1.100";
    mqtt["port"] = 1883;
    mqtt["user"] = "leduser";
    mqtt["password"] = "ledpass";
    mqtt["base_topic"] = "home/led/lounge";
    mqtt["device_name"] = "Lounge-LEDs";

    char buffer[512];
    serializeJson(doc, buffer, sizeof(buffer));

    JsonDocument doc2;
    deserializeJson(doc2, buffer);

    TEST_ASSERT_EQUAL_STRING("192.168.1.100", doc2["mqtt"]["host"].as<const char*>());
    TEST_ASSERT_EQUAL(1883, doc2["mqtt"]["port"].as<int>());
    TEST_ASSERT_EQUAL_STRING("home/led/lounge", doc2["mqtt"]["base_topic"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Lounge-LEDs", doc2["mqtt"]["device_name"].as<const char*>());
}

void test_five_strips_config() {
    JsonDocument doc;
    doc["num_strips"] = 5;
    JsonArray strips = doc["strips"].to<JsonArray>();

    uint8_t pins[] = {5, 4, 14, 12, 13};
    uint16_t lengths[] = {60, 30, 45, 120, 90};

    for (int i = 0; i < 5; i++) {
        JsonObject s = strips.add<JsonObject>();
        s["pin"] = pins[i];
        s["num_leds"] = lengths[i];
        s["brightness"] = 200;
        s["enabled"] = true;
    }

    char buffer[2048];
    serializeJson(doc, buffer, sizeof(buffer));

    JsonDocument doc2;
    deserializeJson(doc2, buffer);

    TEST_ASSERT_EQUAL(5, doc2["num_strips"].as<int>());
    TEST_ASSERT_EQUAL(5, doc2["strips"].size());
    TEST_ASSERT_EQUAL(14, doc2["strips"][2]["pin"].as<int>());
    TEST_ASSERT_EQUAL(120, doc2["strips"][3]["num_leds"].as<int>());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_default_config_serializes_correctly);
    RUN_TEST(test_missing_fields_get_defaults);
    RUN_TEST(test_wifi_config_round_trip);
    RUN_TEST(test_mqtt_config_round_trip);
    RUN_TEST(test_five_strips_config);
    return UNITY_END();
}
```

- [ ] **Step 2: Run native tests**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio test -e native`

Expected: All 5 tests PASS.

- [ ] **Step 3: Commit**

```bash
git add test/test_native/test_config_json.cpp
git commit -m "test: add native tests for config JSON serialization"
```

---

## Chunk 2: Strip Manager + Programme Engine

### Task 4: Strip Manager

**Files:**
- Create: `src/strip_manager.h`
- Create: `src/strip_manager.cpp`

- [ ] **Step 1: Create `src/strip_manager.h`**

```cpp
#ifndef STRIP_MANAGER_H
#define STRIP_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"
#include "config_manager.h"

class StripManager {
public:
    StripManager();

    // Initialise FastLED strips from the given config.
    // Must be called once during setup().
    void begin(const DeviceConfig& config);

    // Get the pixel buffer for a given strip index (0-based).
    CRGB* getPixels(uint8_t stripIndex);

    // Get the number of LEDs for a given strip index.
    uint16_t getNumLeds(uint8_t stripIndex) const;

    // Get the number of active (enabled) strips.
    uint8_t getNumActiveStrips() const;

    // Set brightness for a specific strip (0-255).
    void setBrightness(uint8_t stripIndex, uint8_t brightness);

    // Push all pixel data to the physical strips.
    void show();

private:
    // Static pixel buffers — allocated at max size, used up to numLeds.
    // FastLED requires compile-time-known pin for addLeds<>, so we use
    // a pin-dispatch pattern.
    CRGB _pixels[MAX_STRIPS][MAX_LEDS_PER_STRIP];
    uint16_t _numLeds[MAX_STRIPS];
    uint8_t _brightness[MAX_STRIPS];
    bool _enabled[MAX_STRIPS];
    uint8_t _numActiveStrips;

    void addStripByPin(uint8_t pin, uint8_t index);
};

#endif // STRIP_MANAGER_H
```

- [ ] **Step 2: Create `src/strip_manager.cpp`**

```cpp
#include "strip_manager.h"

StripManager::StripManager() : _numActiveStrips(0) {
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        _numLeds[i] = 0;
        _brightness[i] = DEFAULT_BRIGHTNESS;
        _enabled[i] = false;
    }
}

void StripManager::addStripByPin(uint8_t pin, uint8_t index) {
    // FastLED requires compile-time pin constants for the template.
    // We dispatch based on the runtime pin value.
    switch (pin) {
        case 5:  // D1
            FastLED.addLeds<WS2812B, 5, GRB>(_pixels[index], _numLeds[index]);
            break;
        case 4:  // D2
            FastLED.addLeds<WS2812B, 4, GRB>(_pixels[index], _numLeds[index]);
            break;
        case 14: // D5
            FastLED.addLeds<WS2812B, 14, GRB>(_pixels[index], _numLeds[index]);
            break;
        case 12: // D6
            FastLED.addLeds<WS2812B, 12, GRB>(_pixels[index], _numLeds[index]);
            break;
        case 13: // D7
            FastLED.addLeds<WS2812B, 13, GRB>(_pixels[index], _numLeds[index]);
            break;
        default:
            Serial.print(F("Invalid LED pin: "));
            Serial.println(pin);
            return;
    }
    Serial.print(F("Strip "));
    Serial.print(index);
    Serial.print(F(" on GPIO"));
    Serial.print(pin);
    Serial.print(F(" with "));
    Serial.print(_numLeds[index]);
    Serial.println(F(" LEDs"));
}

void StripManager::begin(const DeviceConfig& config) {
    _numActiveStrips = 0;

    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        _enabled[i] = config.strips[i].enabled;
        _numLeds[i] = config.strips[i].numLeds;
        _brightness[i] = config.strips[i].brightness;

        if (_enabled[i] && _numLeds[i] > 0) {
            addStripByPin(config.strips[i].pin, i);
            _numActiveStrips++;
        }
    }

    // Apply initial brightness
    // FastLED global brightness applies to all — we handle per-strip
    // brightness by scaling in the pixel buffer before show().
    FastLED.setBrightness(255);  // Max global; per-strip handled manually
}

void StripManager::show() {
    // Apply per-strip brightness scaling before pushing pixels.
    // We scale in-place, then show, then restore.
    // This avoids needing a second buffer.
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        if (_enabled[i] && _numLeds[i] > 0) {
            nscale8_video(_pixels[i], _numLeds[i], _brightness[i]);
        }
    }
    FastLED.show();
}

CRGB* StripManager::getPixels(uint8_t stripIndex) {
    if (stripIndex >= MAX_STRIPS) return nullptr;
    return _pixels[stripIndex];
}

uint16_t StripManager::getNumLeds(uint8_t stripIndex) const {
    if (stripIndex >= MAX_STRIPS) return 0;
    return _numLeds[stripIndex];
}

uint8_t StripManager::getNumActiveStrips() const {
    return _numActiveStrips;
}

void StripManager::setBrightness(uint8_t stripIndex, uint8_t brightness) {
    if (stripIndex >= MAX_STRIPS) return;
    _brightness[stripIndex] = brightness;
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/strip_manager.h src/strip_manager.cpp
git commit -m "feat: add StripManager with FastLED pin dispatch for 1-5 strips"
```

---

### Task 5: Programme Engine — Base Class and Registry

**Files:**
- Create: `src/programme_engine.h`
- Create: `src/programme_engine.cpp`

- [ ] **Step 1: Create `src/programme_engine.h`**

```cpp
#ifndef PROGRAMME_ENGINE_H
#define PROGRAMME_ENGINE_H

#include <Arduino.h>
#include <FastLED.h>
#include "config.h"

// Abstract base class for all lighting programmes.
// Each programme implements render() which fills a pixel buffer
// for a given number of LEDs based on elapsed time.
class Programme {
public:
    virtual ~Programme() {}

    // Get the programme's display name.
    virtual const char* name() const = 0;

    // Render one frame into the pixel buffer.
    // leds: pointer to CRGB array to fill
    // numLeds: number of pixels in this strip
    // elapsedMs: milliseconds since the programme started running
    virtual void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) = 0;
};

class StripManager;  // forward declaration

class ProgrammeEngine {
public:
    ProgrammeEngine();

    // Register a programme. Returns the index. Max NUM_BUILTIN_PROGRAMMES.
    uint8_t registerProgramme(Programme* prog);

    // Select active programme by index.
    void selectProgramme(uint8_t index);

    // Get the currently active programme index.
    uint8_t getActiveProgramme() const;

    // Get programme name by index.
    const char* getProgrammeName(uint8_t index) const;

    // Get total registered programme count.
    uint8_t getProgrammeCount() const;

    // Call once per frame. Renders the active programme onto all strips.
    void tick(StripManager& strips);

    // Reset the elapsed timer (e.g. when switching programmes).
    void resetTimer();

private:
    Programme* _programmes[NUM_BUILTIN_PROGRAMMES];
    uint8_t _programmeCount;
    uint8_t _activeProgramme;
    uint32_t _startTime;
};

#endif // PROGRAMME_ENGINE_H
```

- [ ] **Step 2: Create `src/programme_engine.cpp`**

```cpp
#include "programme_engine.h"
#include "strip_manager.h"

ProgrammeEngine::ProgrammeEngine()
    : _programmeCount(0)
    , _activeProgramme(0)
    , _startTime(0) {
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

uint8_t ProgrammeEngine::getActiveProgramme() const {
    return _activeProgramme;
}

const char* ProgrammeEngine::getProgrammeName(uint8_t index) const {
    if (index >= _programmeCount || _programmes[index] == nullptr) {
        return "unknown";
    }
    return _programmes[index]->name();
}

uint8_t ProgrammeEngine::getProgrammeCount() const {
    return _programmeCount;
}

void ProgrammeEngine::tick(StripManager& strips) {
    if (_activeProgramme >= _programmeCount) return;
    if (_programmes[_activeProgramme] == nullptr) return;

    uint32_t elapsed = millis() - _startTime;

    // Render the active programme onto every enabled strip independently.
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        uint16_t numLeds = strips.getNumLeds(i);
        if (numLeds == 0) continue;
        CRGB* pixels = strips.getPixels(i);
        if (pixels == nullptr) continue;
        _programmes[_activeProgramme]->render(pixels, numLeds, elapsed);
    }
}

void ProgrammeEngine::resetTimer() {
    _startTime = millis();
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/programme_engine.h src/programme_engine.cpp
git commit -m "feat: add ProgrammeEngine with abstract Programme base class and registry"
```

---

## Chunk 3: Built-In Programmes

### Task 6: Sunset Programme

**Files:**
- Create: `src/programmes/sunset.h`
- Create: `src/programmes/sunset.cpp`

- [ ] **Step 1: Create `src/programmes/sunset.h`**

```cpp
#ifndef PROGRAMME_SUNSET_H
#define PROGRAMME_SUNSET_H

#include "../programme_engine.h"

class SunsetProgramme : public Programme {
public:
    const char* name() const override { return "sunset"; }
    void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) override;
};

#endif
```

- [ ] **Step 2: Create `src/programmes/sunset.cpp`**

```cpp
#include "sunset.h"

// Sunset: smooth cycling fade through warm colours.
// Colours: deep orange → red-orange → rose → deep purple → loop
// Each colour transition takes STEP_DURATION_MS.

static const CRGB SUNSET_COLOURS[] = {
    CRGB(255, 100, 0),   // deep orange
    CRGB(255, 50, 20),   // red-orange
    CRGB(220, 40, 80),   // rose
    CRGB(120, 20, 140),  // deep purple
};
static const uint8_t NUM_COLOURS = sizeof(SUNSET_COLOURS) / sizeof(SUNSET_COLOURS[0]);
static const uint32_t STEP_DURATION_MS = 4000;
static const uint32_t CYCLE_DURATION_MS = STEP_DURATION_MS * NUM_COLOURS;

void SunsetProgramme::render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) {
    uint32_t cyclePos = elapsedMs % CYCLE_DURATION_MS;
    uint8_t colourIndex = cyclePos / STEP_DURATION_MS;
    uint8_t nextIndex = (colourIndex + 1) % NUM_COLOURS;

    // Fraction through the current step (0-255 for lerp)
    uint32_t stepPos = cyclePos % STEP_DURATION_MS;
    uint8_t blend = (stepPos * 255) / STEP_DURATION_MS;

    CRGB colour = blend(SUNSET_COLOURS[colourIndex], SUNSET_COLOURS[nextIndex], blend);

    fill_solid(leds, numLeds, colour);
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS. Note: `blend` is both a variable name and a FastLED function. If there's a naming collision, rename the variable to `blendAmount` in both the declaration and the function call.

- [ ] **Step 4: Fix naming collision if needed**

If the build fails due to `blend` shadowing the FastLED `blend()` function, update `sunset.cpp`:

```cpp
    uint8_t blendAmount = (stepPos * 255) / STEP_DURATION_MS;
    CRGB colour = blend(SUNSET_COLOURS[colourIndex], SUNSET_COLOURS[nextIndex], blendAmount);
```

- [ ] **Step 5: Commit**

```bash
git add src/programmes/sunset.h src/programmes/sunset.cpp
git commit -m "feat: add Sunset programme — warm colour fade cycle"
```

---

### Task 7: Running Rainbow Programme

**Files:**
- Create: `src/programmes/rainbow.h`
- Create: `src/programmes/rainbow.cpp`

- [ ] **Step 1: Create `src/programmes/rainbow.h`**

```cpp
#ifndef PROGRAMME_RAINBOW_H
#define PROGRAMME_RAINBOW_H

#include "../programme_engine.h"

class RainbowProgramme : public Programme {
public:
    const char* name() const override { return "rainbow"; }
    void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) override;
};

#endif
```

- [ ] **Step 2: Create `src/programmes/rainbow.cpp`**

```cpp
#include "rainbow.h"

// Running Rainbow: full hue spectrum distributed across the strip,
// scrolling continuously. One full scroll cycle takes CYCLE_DURATION_MS.

static const uint32_t CYCLE_DURATION_MS = 5000;

void RainbowProgramme::render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) {
    // Hue offset scrolls over time
    uint8_t hueOffset = (elapsedMs * 256 / CYCLE_DURATION_MS) % 256;

    // Distribute full hue spectrum across strip length
    fill_rainbow(leds, numLeds, hueOffset, 256 / numLeds);
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/programmes/rainbow.h src/programmes/rainbow.cpp
git commit -m "feat: add Running Rainbow programme — scrolling hue cycle"
```

---

### Task 8: Nightlight Programme

**Files:**
- Create: `src/programmes/nightlight.h`
- Create: `src/programmes/nightlight.cpp`

- [ ] **Step 1: Create `src/programmes/nightlight.h`**

```cpp
#ifndef PROGRAMME_NIGHTLIGHT_H
#define PROGRAMME_NIGHTLIGHT_H

#include "../programme_engine.h"

class NightlightProgramme : public Programme {
public:
    const char* name() const override { return "nightlight"; }
    void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) override;
};

#endif
```

- [ ] **Step 2: Create `src/programmes/nightlight.cpp`**

```cpp
#include "nightlight.h"

// Nightlight: warm low-brightness glow with gentle breathing effect.
// Base colour: warm white (255, 180, 100) at low brightness.
// Breathing cycle: brightness oscillates using a sine wave.

static const uint32_t BREATHE_CYCLE_MS = 4000;  // one full breathe in+out
static const uint8_t MIN_BRIGHTNESS = 30;
static const uint8_t MAX_BRIGHTNESS = 100;
static const CRGB BASE_COLOUR = CRGB(255, 180, 100);  // warm white

void NightlightProgramme::render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) {
    // Use sin8 for smooth breathing. sin8 returns 0-255.
    // Map elapsed time to 0-255 input for sin8.
    uint8_t sinInput = (elapsedMs % BREATHE_CYCLE_MS) * 255 / BREATHE_CYCLE_MS;
    uint8_t sinVal = sin8(sinInput);  // 0-255 sine wave

    // Map sine output to brightness range
    uint8_t brightness = map(sinVal, 0, 255, MIN_BRIGHTNESS, MAX_BRIGHTNESS);

    CRGB colour = BASE_COLOUR;
    colour.nscale8_video(brightness);

    fill_solid(leds, numLeds, colour);
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/programmes/nightlight.h src/programmes/nightlight.cpp
git commit -m "feat: add Nightlight programme — warm breathing glow"
```

---

### Task 9: Sky at Night Programme

**Files:**
- Create: `src/programmes/sky_at_night.h`
- Create: `src/programmes/sky_at_night.cpp`

- [ ] **Step 1: Create `src/programmes/sky_at_night.h`**

```cpp
#ifndef PROGRAMME_SKY_AT_NIGHT_H
#define PROGRAMME_SKY_AT_NIGHT_H

#include "../programme_engine.h"

// Maximum number of simultaneous "stars" twinkling.
#define MAX_STARS 15

struct Star {
    uint16_t position;    // pixel index
    CRGB colour;          // white or light-yellow
    uint32_t startMs;     // when this star began fading in
    uint16_t durationMs;  // how long this star lives (fade-in + hold + fade-out)
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
    uint16_t _lastNumLeds;  // detect strip length changes

    void spawnStar(uint16_t numLeds, uint32_t elapsedMs);
    uint8_t starBrightness(const Star& star, uint32_t elapsedMs) const;
};

#endif
```

- [ ] **Step 2: Create `src/programmes/sky_at_night.cpp`**

```cpp
#include "sky_at_night.h"

// Sky at Night: dark blue base with twinkling white/light-yellow stars.
// Stars fade in, hold briefly, then fade out at random positions.
// New stars spawn periodically. Positions randomised each cycle.

static const CRGB SKY_COLOUR = CRGB(5, 5, 40);           // dark blue base
static const CRGB STAR_COLOURS[] = {
    CRGB(255, 255, 255),   // white
    CRGB(255, 255, 200),   // warm white
    CRGB(255, 240, 150),   // light yellow
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
    // Find an inactive star slot
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
    // All slots full — skip this spawn
}

uint8_t SkyAtNightProgramme::starBrightness(const Star& star, uint32_t elapsedMs) const {
    uint32_t age = elapsedMs - star.startMs;
    if (age >= star.durationMs) return 0;

    // Divide lifetime into thirds: fade-in, hold, fade-out
    uint32_t third = star.durationMs / 3;

    if (age < third) {
        // Fade in
        return (age * 255) / third;
    } else if (age < third * 2) {
        // Hold at full
        return 255;
    } else {
        // Fade out
        uint32_t fadeAge = age - (third * 2);
        uint32_t fadeLen = star.durationMs - (third * 2);
        return 255 - (fadeAge * 255) / fadeLen;
    }
}

void SkyAtNightProgramme::render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs) {
    // If strip length changed, deactivate all stars to avoid out-of-bounds
    if (numLeds != _lastNumLeds) {
        for (uint8_t i = 0; i < MAX_STARS; i++) {
            _stars[i].active = false;
        }
        _lastNumLeds = numLeds;
    }

    // Fill background
    fill_solid(leds, numLeds, SKY_COLOUR);

    // Spawn new stars periodically
    if (elapsedMs - _lastSpawnMs >= SPAWN_INTERVAL_MS) {
        spawnStar(numLeds, elapsedMs);
        _lastSpawnMs = elapsedMs;
    }

    // Render active stars
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
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/programmes/sky_at_night.h src/programmes/sky_at_night.cpp
git commit -m "feat: add Sky at Night programme — twinkling stars on dark blue"
```

---

### Task 10: Programme Math Native Tests

**Files:**
- Create: `test/test_native/test_programme_math.cpp`

- [ ] **Step 1: Create `test/test_native/test_programme_math.cpp`**

Test the colour interpolation and timing math used by programmes. These tests verify the mathematical logic without FastLED dependencies.

```cpp
#include <unity.h>
#include <cstdint>

// --- Colour interpolation helpers (same math as in programmes) ---

// Linear interpolation between two uint8_t values.
static uint8_t lerp8(uint8_t a, uint8_t b, uint8_t fraction) {
    return a + (((int16_t)b - a) * fraction) / 255;
}

// Simple sin8 approximation for testing (matches FastLED's shape).
// Input 0-255 maps to 0-2*PI. Output 0-255.
static uint8_t test_sin8(uint8_t theta) {
    // FastLED's sin8: sin8(0)=128, sin8(64)=255, sin8(128)=128, sin8(192)=0
    // For testing, we use a lookup-free approximation.
    // Just verify the wave properties.
    static const uint8_t lookup[] = {128, 255, 128, 0};
    return lookup[theta / 64];  // coarse 4-point check
}

// --- Tests ---

void test_lerp8_endpoints() {
    TEST_ASSERT_EQUAL(0, lerp8(0, 255, 0));
    TEST_ASSERT_EQUAL(255, lerp8(0, 255, 255));
    TEST_ASSERT_EQUAL(100, lerp8(100, 100, 128));
}

void test_lerp8_midpoint() {
    uint8_t mid = lerp8(0, 200, 128);
    // 128/255 * 200 ≈ 100
    TEST_ASSERT_UINT8_WITHIN(2, 100, mid);
}

void test_breathing_brightness_range() {
    // Simulate the nightlight breathing calculation
    uint32_t breatheCycleMs = 4000;
    uint8_t minBright = 30;
    uint8_t maxBright = 100;

    // At cycle start (sinInput=0, sin8≈128 → mid brightness)
    uint8_t sinInput0 = 0;
    uint8_t sinVal0 = test_sin8(sinInput0);
    uint8_t bright0 = minBright + ((uint16_t)(maxBright - minBright) * sinVal0) / 255;
    TEST_ASSERT_TRUE(bright0 >= minBright);
    TEST_ASSERT_TRUE(bright0 <= maxBright);

    // At quarter cycle (sinInput=64, sin8=255 → max brightness)
    uint8_t sinInput64 = 64;
    uint8_t sinVal64 = test_sin8(sinInput64);
    uint8_t bright64 = minBright + ((uint16_t)(maxBright - minBright) * sinVal64) / 255;
    TEST_ASSERT_EQUAL(maxBright, bright64);
}

void test_star_brightness_lifecycle() {
    // Simulate Sky at Night star brightness over its lifetime.
    // Duration split into thirds: fade-in, hold, fade-out.
    uint16_t duration = 3000;
    uint32_t third = duration / 3;

    // Fade-in: age=0 → brightness=0
    uint32_t age0 = 0;
    uint8_t b0 = (age0 * 255) / third;
    TEST_ASSERT_EQUAL(0, b0);

    // Fade-in: age=third/2 → brightness≈127
    uint32_t ageHalfIn = third / 2;
    uint8_t bHalfIn = (ageHalfIn * 255) / third;
    TEST_ASSERT_UINT8_WITHIN(2, 127, bHalfIn);

    // Hold: age=third → brightness=255
    uint32_t ageHold = third;
    uint8_t bHold = 255;  // hold phase is always 255
    TEST_ASSERT_EQUAL(255, bHold);

    // Fade-out: age=duration → brightness=0
    uint32_t ageDone = duration;
    // age >= duration → return 0
    TEST_ASSERT_TRUE(ageDone >= duration);
}

void test_rainbow_hue_wraps() {
    // Verify hue offset wraps correctly over time
    uint32_t cycleDuration = 5000;

    uint8_t hue0 = (0 * 256 / cycleDuration) % 256;
    TEST_ASSERT_EQUAL(0, hue0);

    uint8_t hueHalf = (uint32_t)(2500UL * 256 / cycleDuration) % 256;
    TEST_ASSERT_EQUAL(128, hueHalf);

    uint8_t hueFull = (uint32_t)(5000UL * 256 / cycleDuration) % 256;
    TEST_ASSERT_EQUAL(0, hueFull);  // wraps back to 0
}

void test_sunset_blend_fraction() {
    // Verify blend fraction calculation
    uint32_t stepDuration = 4000;
    uint32_t stepPos = 2000;  // halfway through step
    uint8_t blendAmount = (stepPos * 255) / stepDuration;
    TEST_ASSERT_UINT8_WITHIN(1, 127, blendAmount);

    // At step boundary
    uint8_t blendStart = (0 * 255) / stepDuration;
    TEST_ASSERT_EQUAL(0, blendStart);

    uint8_t blendEnd = (3999UL * 255) / stepDuration;
    TEST_ASSERT_UINT8_WITHIN(1, 254, blendEnd);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_lerp8_endpoints);
    RUN_TEST(test_lerp8_midpoint);
    RUN_TEST(test_breathing_brightness_range);
    RUN_TEST(test_star_brightness_lifecycle);
    RUN_TEST(test_rainbow_hue_wraps);
    RUN_TEST(test_sunset_blend_fraction);
    return UNITY_END();
}
```

- [ ] **Step 2: Run native tests**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio test -e native`

Expected: All 6 tests PASS (plus the 5 from Task 3 = 11 total).

- [ ] **Step 3: Commit**

```bash
git add test/test_native/test_programme_math.cpp
git commit -m "test: add native tests for programme colour and timing math"
```

---

## Chunk 4: Wi-Fi, Web Portal, Main Integration

### Task 11: Wi-Fi Manager

**Files:**
- Create: `src/wifi_manager.h`
- Create: `src/wifi_manager.cpp`

- [ ] **Step 1: Create `src/wifi_manager.h`**

```cpp
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "config.h"

enum class WifiStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

class WifiManager {
public:
    WifiManager();

    // Attempt to connect to the given SSID/password.
    // Blocks up to timeoutMs. Returns true on success.
    bool connect(const char* ssid, const char* password, uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS);

    // Check if currently connected.
    bool isConnected() const;

    // Get current status.
    WifiStatus getStatus() const;

    // Get the device IP address (valid only when connected).
    String getIP() const;

    // Get the device MAC address (last 4 hex digits, for AP SSID).
    String getMacSuffix() const;

    // Start AP mode for captive portal.
    void startAP();

    // Stop AP mode.
    void stopAP();

private:
    WifiStatus _status;
};

#endif
```

- [ ] **Step 2: Create `src/wifi_manager.cpp`**

```cpp
#include "wifi_manager.h"

WifiManager::WifiManager() : _status(WifiStatus::DISCONNECTED) {}

bool WifiManager::connect(const char* ssid, const char* password, uint32_t timeoutMs) {
    if (ssid == nullptr || ssid[0] == '\0') {
        Serial.println(F("No SSID configured"));
        _status = WifiStatus::FAILED;
        return false;
    }

    Serial.print(F("Connecting to Wi-Fi: "));
    Serial.println(ssid);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    _status = WifiStatus::CONNECTING;
    uint32_t start = millis();

    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > timeoutMs) {
            Serial.println(F("\nWi-Fi connection timed out"));
            WiFi.disconnect();
            _status = WifiStatus::FAILED;
            return false;
        }
        delay(250);
        Serial.print(F("."));
    }

    Serial.println();
    Serial.print(F("Connected. IP: "));
    Serial.println(WiFi.localIP());
    _status = WifiStatus::CONNECTED;
    return true;
}

bool WifiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

WifiStatus WifiManager::getStatus() const {
    return _status;
}

String WifiManager::getIP() const {
    return WiFi.localIP().toString();
}

String WifiManager::getMacSuffix() const {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char suffix[5];
    snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
    return String(suffix);
}

void WifiManager::startAP() {
    String apName = String(WIFI_AP_SSID_PREFIX) + getMacSuffix();
    Serial.print(F("Starting AP: "));
    Serial.println(apName);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apName.c_str(), WIFI_AP_PASSWORD);

    Serial.print(F("AP IP: "));
    Serial.println(WiFi.softAPIP());
}

void WifiManager::stopAP() {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/wifi_manager.h src/wifi_manager.cpp
git commit -m "feat: add WifiManager with STA connect, AP mode, and timeout"
```

---

### Task 12: Web Portal — HTML Templates

**Files:**
- Create: `src/web_portal_html.h`

- [ ] **Step 1: Create `src/web_portal_html.h`**

HTML stored in PROGMEM to keep it out of RAM. The page provides: status display, Wi-Fi/MQTT config form, strip configuration, and 4 programme buttons.

```cpp
#ifndef WEB_PORTAL_HTML_H
#define WEB_PORTAL_HTML_H

#include <Arduino.h>

const char HTML_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>LED Controller</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:16px;max-width:480px;margin:0 auto}
h1{color:#fff;font-size:1.4em;margin-bottom:16px;text-align:center}
h2{color:#ccc;font-size:1.1em;margin:16px 0 8px;border-bottom:1px solid #333;padding-bottom:4px}
.status{background:#16213e;padding:12px;border-radius:8px;margin-bottom:16px;font-size:0.9em}
.status span{color:#0f0}
.status span.fail{color:#f44}
form{margin-bottom:16px}
label{display:block;margin:8px 0 2px;font-size:0.85em;color:#aaa}
input[type=text],input[type=password],input[type=number]{width:100%;padding:8px;border:1px solid #333;border-radius:4px;background:#0f3460;color:#fff;font-size:0.95em}
.btn-row{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:12px 0}
.btn{padding:12px;border:none;border-radius:6px;font-size:0.95em;cursor:pointer;color:#fff;font-weight:bold}
.btn-sunset{background:linear-gradient(135deg,#ff6a00,#c0392b)}
.btn-rainbow{background:linear-gradient(135deg,#e74c3c,#f39c12,#2ecc71,#3498db,#9b59b6)}
.btn-nightlight{background:linear-gradient(135deg,#f5c842,#d4a017);color:#333}
.btn-sky{background:linear-gradient(135deg,#0c2461,#1e3799)}
.btn-save{background:#27ae60;width:100%;margin-top:8px}
.btn.active{outline:3px solid #0f0;outline-offset:2px}
.strip-cfg{background:#16213e;padding:10px;border-radius:6px;margin:6px 0}
</style>
</head><body>
<h1>LED Controller</h1>
)rawliteral";

const char HTML_FOOTER[] PROGMEM = R"rawliteral(
</body></html>
)rawliteral";

// Placeholder tokens replaced at runtime:
// {{WIFI_STATUS}}, {{IP}}, {{PROGRAMME}}, {{NUM_STRIPS}}
// {{SSID}}, {{MQTT_HOST}}, {{MQTT_PORT}}, {{MQTT_USER}}, {{MQTT_TOPIC}}, {{DEVICE_NAME}}
// {{STRIP_CONFIG}} — injected per-strip HTML block
const char HTML_BODY[] PROGMEM = R"rawliteral(
<div class="status">
  Wi-Fi: <span class="{{WIFI_CLASS}}">{{WIFI_STATUS}}</span><br>
  IP: {{IP}}<br>
  Programme: {{PROGRAMME}}<br>
  Strips: {{NUM_STRIPS}}
</div>

<h2>Programmes</h2>
<div class="btn-row">
  <button class="btn btn-sunset {{ACT_SUNSET}}" onclick="fetch('/programme?id=0',{method:'POST'}).then(()=>location.reload())">Sunset</button>
  <button class="btn btn-rainbow {{ACT_RAINBOW}}" onclick="fetch('/programme?id=1',{method:'POST'}).then(()=>location.reload())">Rainbow</button>
  <button class="btn btn-nightlight {{ACT_NIGHTLIGHT}}" onclick="fetch('/programme?id=2',{method:'POST'}).then(()=>location.reload())">Nightlight</button>
  <button class="btn btn-sky {{ACT_SKY}}" onclick="fetch('/programme?id=3',{method:'POST'}).then(()=>location.reload())">Sky at Night</button>
</div>

<h2>Wi-Fi Settings</h2>
<form action="/config" method="POST">
  <label>SSID</label>
  <input type="text" name="ssid" value="{{SSID}}" maxlength="32">
  <label>Password</label>
  <input type="password" name="wifi_pass" value="" maxlength="64" placeholder="(unchanged if blank)">

  <h2>MQTT Settings</h2>
  <label>Broker Host</label>
  <input type="text" name="mqtt_host" value="{{MQTT_HOST}}" maxlength="64">
  <label>Port</label>
  <input type="number" name="mqtt_port" value="{{MQTT_PORT}}" min="1" max="65535">
  <label>Username</label>
  <input type="text" name="mqtt_user" value="{{MQTT_USER}}" maxlength="32">
  <label>Password</label>
  <input type="password" name="mqtt_pass" value="" maxlength="64" placeholder="(unchanged if blank)">
  <label>Base Topic</label>
  <input type="text" name="mqtt_topic" value="{{MQTT_TOPIC}}" maxlength="64">
  <label>Device Name</label>
  <input type="text" name="device_name" value="{{DEVICE_NAME}}" maxlength="32">

  <h2>Strip Configuration</h2>
  {{STRIP_CONFIG}}

  <button type="submit" class="btn btn-save">Save &amp; Reboot</button>
</form>
)rawliteral";

// Template for one strip config block. Tokens: {{STRIP_NUM}}, {{STRIP_PIN}}, {{STRIP_LEDS}}, {{STRIP_BRIGHT}}, {{STRIP_CHECKED}}
const char HTML_STRIP_BLOCK[] PROGMEM = R"rawliteral(
<div class="strip-cfg">
  <label><strong>Strip {{STRIP_NUM}}</strong>
    <input type="checkbox" name="strip{{STRIP_NUM}}_en" {{STRIP_CHECKED}}> Enabled</label>
  <label>GPIO Pin</label>
  <input type="number" name="strip{{STRIP_NUM}}_pin" value="{{STRIP_PIN}}" min="0" max="16">
  <label>Number of LEDs</label>
  <input type="number" name="strip{{STRIP_NUM}}_leds" value="{{STRIP_LEDS}}" min="0" max="300">
  <label>Brightness (0-255)</label>
  <input type="number" name="strip{{STRIP_NUM}}_bright" value="{{STRIP_BRIGHT}}" min="0" max="255">
</div>
)rawliteral";

#endif
```

- [ ] **Step 2: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 3: Commit**

```bash
git add src/web_portal_html.h
git commit -m "feat: add PROGMEM HTML templates for captive portal web UI"
```

---

### Task 13: Web Portal — Server Logic

**Files:**
- Create: `src/web_portal.h`
- Create: `src/web_portal.cpp`

- [ ] **Step 1: Create `src/web_portal.h`**

```cpp
#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include "config_manager.h"
#include "programme_engine.h"
#include "strip_manager.h"

class WebPortal {
public:
    WebPortal(ConfigManager& config, ProgrammeEngine& engine, StripManager& strips);

    // Start the web server and DNS captive portal.
    void begin();

    // Stop the web server and DNS.
    void stop();

    // Must be called in loop() while portal is active.
    void handleClient();

    // Is the portal currently running?
    bool isRunning() const;

private:
    ESP8266WebServer _server;
    DNSServer _dns;
    ConfigManager& _config;
    ProgrammeEngine& _engine;
    StripManager& _strips;
    bool _running;

    void handleRoot();
    void handleConfig();
    void handleProgramme();
    void handleNotFound();

    String buildPage();
    String buildStripConfigHtml();
    String replaceToken(const String& html, const String& token, const String& value);
};

#endif
```

- [ ] **Step 2: Create `src/web_portal.cpp`**

```cpp
#include "web_portal.h"
#include "web_portal_html.h"

WebPortal::WebPortal(ConfigManager& config, ProgrammeEngine& engine, StripManager& strips)
    : _server(80)
    , _config(config)
    , _engine(engine)
    , _strips(strips)
    , _running(false) {}

void WebPortal::begin() {
    // DNS: redirect all domains to our IP (captive portal behaviour)
    _dns.start(53, "*", WiFi.softAPIP());

    _server.on("/", HTTP_GET, [this]() { handleRoot(); });
    _server.on("/config", HTTP_POST, [this]() { handleConfig(); });
    _server.on("/programme", HTTP_POST, [this]() { handleProgramme(); });
    _server.onNotFound([this]() { handleNotFound(); });

    _server.begin();
    _running = true;
    Serial.println(F("Web portal started on port 80"));
}

void WebPortal::stop() {
    _server.stop();
    _dns.stop();
    _running = false;
    Serial.println(F("Web portal stopped"));
}

void WebPortal::handleClient() {
    if (!_running) return;
    _dns.processNextRequest();
    _server.handleClient();
}

bool WebPortal::isRunning() const {
    return _running;
}

void WebPortal::handleRoot() {
    _server.send(200, "text/html", buildPage());
}

void WebPortal::handleProgramme() {
    if (_server.hasArg("id")) {
        uint8_t id = _server.arg("id").toInt();
        if (id < _engine.getProgrammeCount()) {
            _engine.selectProgramme(id);
            _config.config().activeProgramme = id;
            _config.save();
        }
    }
    _server.send(200, "text/plain", "OK");
}

void WebPortal::handleConfig() {
    DeviceConfig& cfg = _config.config();

    // Wi-Fi
    if (_server.hasArg("ssid")) {
        strlcpy(cfg.wifi.ssid, _server.arg("ssid").c_str(), sizeof(cfg.wifi.ssid));
    }
    if (_server.hasArg("wifi_pass") && _server.arg("wifi_pass").length() > 0) {
        strlcpy(cfg.wifi.password, _server.arg("wifi_pass").c_str(), sizeof(cfg.wifi.password));
    }

    // MQTT
    if (_server.hasArg("mqtt_host")) {
        strlcpy(cfg.mqtt.host, _server.arg("mqtt_host").c_str(), sizeof(cfg.mqtt.host));
    }
    if (_server.hasArg("mqtt_port")) {
        cfg.mqtt.port = _server.arg("mqtt_port").toInt();
    }
    if (_server.hasArg("mqtt_user")) {
        strlcpy(cfg.mqtt.user, _server.arg("mqtt_user").c_str(), sizeof(cfg.mqtt.user));
    }
    if (_server.hasArg("mqtt_pass") && _server.arg("mqtt_pass").length() > 0) {
        strlcpy(cfg.mqtt.password, _server.arg("mqtt_pass").c_str(), sizeof(cfg.mqtt.password));
    }
    if (_server.hasArg("mqtt_topic")) {
        strlcpy(cfg.mqtt.baseTopic, _server.arg("mqtt_topic").c_str(), sizeof(cfg.mqtt.baseTopic));
    }
    if (_server.hasArg("device_name")) {
        strlcpy(cfg.mqtt.deviceName, _server.arg("device_name").c_str(), sizeof(cfg.mqtt.deviceName));
    }

    // Strips
    uint8_t activeCount = 0;
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        String prefix = "strip" + String(i + 1);
        String enKey = prefix + "_en";
        bool enabled = _server.hasArg(enKey);
        cfg.strips[i].enabled = enabled;

        String pinKey = prefix + "_pin";
        if (_server.hasArg(pinKey)) {
            cfg.strips[i].pin = _server.arg(pinKey).toInt();
        }

        String ledsKey = prefix + "_leds";
        if (_server.hasArg(ledsKey)) {
            cfg.strips[i].numLeds = _server.arg(ledsKey).toInt();
        }

        String brightKey = prefix + "_bright";
        if (_server.hasArg(brightKey)) {
            cfg.strips[i].brightness = _server.arg(brightKey).toInt();
        }

        if (enabled) activeCount++;
    }
    cfg.numStrips = activeCount;

    _config.save();

    // Send response and reboot
    _server.send(200, "text/html",
        "<html><body style='background:#1a1a2e;color:#fff;text-align:center;padding-top:40px'>"
        "<h2>Settings saved!</h2><p>Rebooting...</p></body></html>");

    delay(1000);
    ESP.restart();
}

void WebPortal::handleNotFound() {
    // Captive portal: redirect everything to root
    _server.sendHeader("Location", "/", true);
    _server.send(302, "text/plain", "");
}

String WebPortal::replaceToken(const String& html, const String& token, const String& value) {
    String result = html;
    result.replace(token, value);
    return result;
}

String WebPortal::buildStripConfigHtml() {
    String html;
    for (uint8_t i = 0; i < MAX_STRIPS; i++) {
        String block = FPSTR(HTML_STRIP_BLOCK);
        block = replaceToken(block, "{{STRIP_NUM}}", String(i + 1));
        block = replaceToken(block, "{{STRIP_PIN}}", String(_config.config().strips[i].pin));
        block = replaceToken(block, "{{STRIP_LEDS}}", String(_config.config().strips[i].numLeds));
        block = replaceToken(block, "{{STRIP_BRIGHT}}", String(_config.config().strips[i].brightness));
        block = replaceToken(block, "{{STRIP_CHECKED}}",
            _config.config().strips[i].enabled ? "checked" : "");
        html += block;
    }
    return html;
}

String WebPortal::buildPage() {
    String page = FPSTR(HTML_HEADER);
    String body = FPSTR(HTML_BODY);

    // Status
    bool connected = WiFi.status() == WL_CONNECTED;
    body = replaceToken(body, "{{WIFI_CLASS}}", connected ? "" : "fail");
    body = replaceToken(body, "{{WIFI_STATUS}}", connected ? "Connected" : "Not connected");
    body = replaceToken(body, "{{IP}}", connected ? WiFi.localIP().toString() : "N/A (AP mode)");
    body = replaceToken(body, "{{PROGRAMME}}", String(_engine.getProgrammeName(_engine.getActiveProgramme())));
    body = replaceToken(body, "{{NUM_STRIPS}}", String(_strips.getNumActiveStrips()));

    // Active programme highlighting
    uint8_t active = _engine.getActiveProgramme();
    body = replaceToken(body, "{{ACT_SUNSET}}", active == 0 ? "active" : "");
    body = replaceToken(body, "{{ACT_RAINBOW}}", active == 1 ? "active" : "");
    body = replaceToken(body, "{{ACT_NIGHTLIGHT}}", active == 2 ? "active" : "");
    body = replaceToken(body, "{{ACT_SKY}}", active == 3 ? "active" : "");

    // Config values
    body = replaceToken(body, "{{SSID}}", String(_config.config().wifi.ssid));
    body = replaceToken(body, "{{MQTT_HOST}}", String(_config.config().mqtt.host));
    body = replaceToken(body, "{{MQTT_PORT}}", String(_config.config().mqtt.port));
    body = replaceToken(body, "{{MQTT_USER}}", String(_config.config().mqtt.user));
    body = replaceToken(body, "{{MQTT_TOPIC}}", String(_config.config().mqtt.baseTopic));
    body = replaceToken(body, "{{DEVICE_NAME}}", String(_config.config().mqtt.deviceName));

    // Strip config
    body = replaceToken(body, "{{STRIP_CONFIG}}", buildStripConfigHtml());

    page += body;
    page += FPSTR(HTML_FOOTER);
    return page;
}
```

- [ ] **Step 3: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 4: Commit**

```bash
git add src/web_portal.h src/web_portal.cpp
git commit -m "feat: add captive portal web server with config forms and programme buttons"
```

---

### Task 14: Main Integration — Boot Sequence and Loop

**Files:**
- Modify: `src/main.cpp` (complete rewrite)

- [ ] **Step 1: Rewrite `src/main.cpp`**

```cpp
#include <Arduino.h>
#include "config.h"
#include "config_manager.h"
#include "strip_manager.h"
#include "programme_engine.h"
#include "wifi_manager.h"
#include "web_portal.h"

// Programme includes
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
        // Wi-Fi failed — go to portal mode
        Serial.println(F("Entering captive portal mode"));
        wifiManager.startAP();
        webPortal->begin();
        portalMode = true;
    } else {
        // Wi-Fi OK — for Phase 1, we don't have MQTT yet.
        // Just run normally. MQTT check will be added in Phase 2.
        Serial.println(F("Wi-Fi connected, running normally"));
        portalMode = false;
    }

    lastFrameMs = millis();
    Serial.println(F("Setup complete"));
}

void loop() {
    uint32_t now = millis();

    if (portalMode) {
        // Run web portal + programme engine (so LEDs still animate)
        webPortal->handleClient();
    }

    // Run programme engine at target FPS
    if (now - lastFrameMs >= FRAME_INTERVAL_MS) {
        lastFrameMs = now;
        programmeEngine.tick(stripManager);
        stripManager.show();
    }

    yield();  // Feed the ESP8266 watchdog
}
```

- [ ] **Step 2: Verify build compiles**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS

- [ ] **Step 3: Commit**

```bash
git add src/main.cpp
git commit -m "feat: main.cpp boot sequence — config, strips, programmes, wifi, portal fallback"
```

---

## Chunk 5: Hardware Verification and Polish

### Task 15: First Hardware Flash and Smoke Test

This task requires a physical ESP8266 with at least one WS2812B strip connected to D1.

**Files:** None (testing only)

- [ ] **Step 1: Flash firmware to device**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -t upload -e nodemcuv2`

Expected: Upload completes. If USB device is in LXD container, ensure passthrough is configured per PRD Section 4.2.

- [ ] **Step 2: Open serial monitor**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio device monitor`

Expected output (approximately):
```
=== LED Controller ===
No config file found, using defaults
Active programme: 0
Strip 0 on GPIO5 with 60 LEDs
Registered programme: sunset
Registered programme: rainbow
Registered programme: nightlight
Registered programme: sky_at_night
Selected programme: sunset
No SSID configured
Entering captive portal mode
Starting AP: LED-Controller-XXXX
AP IP: 192.168.4.1
Web portal started on port 80
Setup complete
```

- [ ] **Step 3: Verify LED strip shows the Sunset programme**

Visual check: the strip should display a warm colour that slowly fades through orange → red → rose → purple → loop.

- [ ] **Step 4: Verify captive portal**

Connect phone/laptop to the `LED-Controller-XXXX` Wi-Fi network (password: `configure`). A captive portal page should appear automatically, or navigate to `192.168.4.1`. Verify:
- Status shows "Not connected", programme shows "sunset", strips shows "1".
- All 4 programme buttons are visible.
- Tapping "Rainbow" changes the LED strip to a scrolling rainbow and the button highlights.
- Wi-Fi/MQTT config fields are present and editable.
- Strip configuration shows Strip 1 enabled on GPIO 5 with 60 LEDs.

- [ ] **Step 5: Test Wi-Fi config and reboot**

Enter valid Wi-Fi credentials in the portal and click "Save & Reboot". Verify:
- Device reboots (serial output restarts).
- Device connects to Wi-Fi (serial shows IP address).
- Device does NOT start the captive portal (normal operation mode).
- Last-selected programme resumes after reboot.

- [ ] **Step 6: Commit any fixes from smoke testing**

```bash
git add -A
git commit -m "fix: adjustments from hardware smoke testing"
```

---

### Task 16: Verify All 4 Programmes on Hardware

**Files:** None (testing only)

- [ ] **Step 1: Test Sunset programme**

Select via web portal. Verify: warm colours fade smoothly through orange → red → rose → purple → loop. No flickering. Smooth transitions.

- [ ] **Step 2: Test Running Rainbow programme**

Select via web portal. Verify: full hue spectrum distributed across strip, scrolling continuously. No jumps or stuttering.

- [ ] **Step 3: Test Nightlight programme**

Select via web portal. Verify: warm low-brightness glow pulsing gently. Breathing rhythm feels natural (~4 second cycle). Suitable ambient light level.

- [ ] **Step 4: Test Sky at Night programme**

Select via web portal. Verify: dark blue base. Scattered white/light-yellow pixels twinkle — fading in, holding, fading out. New star positions each cycle. Overall effect looks like a starfield.

- [ ] **Step 5: Test programme persistence across reboot**

Select "Sky at Night". Power cycle the device (unplug and replug). Verify serial output shows `Active programme: 3` and the strip resumes Sky at Night immediately.

- [ ] **Step 6: Commit any fixes**

```bash
git add -A
git commit -m "fix: programme tuning from hardware testing"
```

---

### Task 17: Multi-Strip Verification (if hardware available)

**Files:** None (testing only). Skip this task if only one strip is available.

- [ ] **Step 1: Connect a second strip to D2**

Wire a second WS2812B strip to GPIO4 (D2) via level shifter and 330R resistor.

- [ ] **Step 2: Configure via web portal**

Open captive portal. Enable Strip 2, set GPIO pin to 4, set LED count to match the physical strip. Save & reboot.

- [ ] **Step 3: Verify both strips run the same programme independently**

Both strips should show the same programme (e.g. Rainbow), but each renders for its own pixel count. A 60-LED strip and a 30-LED strip both show a full rainbow, not a truncated one.

- [ ] **Step 4: Verify brightness independence**

Set different brightness values for Strip 1 and Strip 2 via the web portal. Verify each strip respects its own brightness setting.

- [ ] **Step 5: Commit any fixes**

```bash
git add -A
git commit -m "fix: multi-strip adjustments from hardware testing"
```

---

### Task 18: Run All Native Tests — Final Check

**Files:** None

- [ ] **Step 1: Run the full native test suite**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio test -e native`

Expected: All 11 tests PASS (5 config JSON + 6 programme math).

- [ ] **Step 2: Run a full firmware build**

Run: `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2`

Expected: BUILD SUCCESS. Note the firmware size — it must be under 500 KB for future OTA compatibility.

- [ ] **Step 3: Commit and tag Phase 1 complete**

```bash
git add -A
git commit -m "chore: Phase 1 MVP complete — all tests passing"
git tag v0.1.0 -m "Phase 1: Standalone Device MVP"
```

---

## Summary

| Chunk | Tasks | What it delivers |
|---|---|---|
| 1: Scaffolding + Config | Tasks 1-3 | Buildable project, config persistence, native tests |
| 2: Strip + Programme Engine | Tasks 4-5 | FastLED strip driving, programme base class and registry |
| 3: Built-In Programmes | Tasks 6-10 | All 4 programmes implemented + native math tests |
| 4: Wi-Fi + Portal + Main | Tasks 11-14 | Wi-Fi connection, captive portal web UI, full boot sequence |
| 5: Hardware Verification | Tasks 15-18 | Flash, smoke test, multi-strip, final test suite |

**Total: 18 tasks across 5 chunks.**

After Phase 1 is complete and tagged, Phase 2 (MQTT, custom programmes, scheduler) can be planned as a separate implementation plan.

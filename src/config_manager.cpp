#include "config_manager.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

ConfigManager::ConfigManager() : _loaded(false) {
    applyDefaults();
}

void ConfigManager::applyDefaults() {
    memset(&_config, 0, sizeof(DeviceConfig));
    _config.numStrips = 1;
    _config.strips[0].pin = DEFAULT_STRIP_PIN;
    _config.strips[0].numLeds = DEFAULT_STRIP_LENGTH;
    _config.strips[0].brightness = DEFAULT_BRIGHTNESS;
    _config.strips[0].enabled = true;

    for (uint8_t i = 1; i < MAX_STRIPS; i++) {
        _config.strips[i].pin = VALID_LED_PINS[i];
        _config.strips[i].numLeds = 0;
        _config.strips[i].brightness = DEFAULT_BRIGHTNESS;
        _config.strips[i].enabled = false;
    }

    _config.activeProgramme = DEFAULT_PROGRAMME;
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

DeviceConfig& ConfigManager::config() { return _config; }
const DeviceConfig& ConfigManager::config() const { return _config; }

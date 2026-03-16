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
    bool load();
    bool save();
    void applyDefaults();
    DeviceConfig& config();
    const DeviceConfig& config() const;

private:
    DeviceConfig _config;
    bool _loaded;
    void configToJson(char* buffer, size_t bufferSize) const;
    bool jsonToConfig(const char* json);
};

#endif // CONFIG_MANAGER_H

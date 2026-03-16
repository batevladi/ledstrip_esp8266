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
    bool connect(const char* ssid, const char* password, uint32_t timeoutMs = WIFI_CONNECT_TIMEOUT_MS);
    bool isConnected() const;
    WifiStatus getStatus() const;
    String getIP() const;
    String getMacSuffix() const;
    void startAP();
    void stopAP();

private:
    WifiStatus _status;
};

#endif

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

bool WifiManager::isConnected() const { return WiFi.status() == WL_CONNECTED; }
WifiStatus WifiManager::getStatus() const { return _status; }
String WifiManager::getIP() const { return WiFi.localIP().toString(); }

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

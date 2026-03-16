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
    void begin();
    void stop();
    void handleClient();
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

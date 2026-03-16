#include "web_portal.h"
#include "web_portal_html.h"

WebPortal::WebPortal(ConfigManager& config, ProgrammeEngine& engine, StripManager& strips)
    : _server(80), _config(config), _engine(engine), _strips(strips), _running(false) {}

void WebPortal::begin() {
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

bool WebPortal::isRunning() const { return _running; }

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

    if (_server.hasArg("ssid")) {
        strlcpy(cfg.wifi.ssid, _server.arg("ssid").c_str(), sizeof(cfg.wifi.ssid));
    }
    if (_server.hasArg("wifi_pass") && _server.arg("wifi_pass").length() > 0) {
        strlcpy(cfg.wifi.password, _server.arg("wifi_pass").c_str(), sizeof(cfg.wifi.password));
    }
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

    _server.send(200, "text/html",
        "<html><body style='background:#1a1a2e;color:#fff;text-align:center;padding-top:40px'>"
        "<h2>Settings saved!</h2><p>Rebooting...</p></body></html>");

    delay(1000);
    ESP.restart();
}

void WebPortal::handleNotFound() {
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

    bool connected = WiFi.status() == WL_CONNECTED;
    body = replaceToken(body, "{{WIFI_CLASS}}", connected ? "" : "fail");
    body = replaceToken(body, "{{WIFI_STATUS}}", connected ? "Connected" : "Not connected");
    body = replaceToken(body, "{{IP}}", connected ? WiFi.localIP().toString() : "N/A (AP mode)");
    body = replaceToken(body, "{{PROGRAMME}}", String(_engine.getProgrammeName(_engine.getActiveProgramme())));
    body = replaceToken(body, "{{NUM_STRIPS}}", String(_strips.getNumActiveStrips()));

    uint8_t active = _engine.getActiveProgramme();
    body = replaceToken(body, "{{ACT_SUNSET}}", active == 0 ? "active" : "");
    body = replaceToken(body, "{{ACT_RAINBOW}}", active == 1 ? "active" : "");
    body = replaceToken(body, "{{ACT_NIGHTLIGHT}}", active == 2 ? "active" : "");
    body = replaceToken(body, "{{ACT_SKY}}", active == 3 ? "active" : "");

    body = replaceToken(body, "{{SSID}}", String(_config.config().wifi.ssid));
    body = replaceToken(body, "{{MQTT_HOST}}", String(_config.config().mqtt.host));
    body = replaceToken(body, "{{MQTT_PORT}}", String(_config.config().mqtt.port));
    body = replaceToken(body, "{{MQTT_USER}}", String(_config.config().mqtt.user));
    body = replaceToken(body, "{{MQTT_TOPIC}}", String(_config.config().mqtt.baseTopic));
    body = replaceToken(body, "{{DEVICE_NAME}}", String(_config.config().mqtt.deviceName));

    body = replaceToken(body, "{{STRIP_CONFIG}}", buildStripConfigHtml());

    page += body;
    page += FPSTR(HTML_FOOTER);
    return page;
}

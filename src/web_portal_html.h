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

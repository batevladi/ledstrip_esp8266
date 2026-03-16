# Product Requirements Document: ESP8266 Programmable LED Strip Controller

**Version:** 1.3
**Date:** 2026-03-16
**Status:** Draft

---

## 1. Overview

A microcontroller-based system using the ESP8266 platform to manage one or more programmable LED strips (WS2812B / NeoPixel or similar addressable LEDs), up to a maximum of 5 strips per device. The system supports remote control via MQTT, local autonomous operation via timers, over-the-air (OTA) firmware updates, and persistent storage of lighting programmes. When multiple strips are connected, they can be controlled independently, mirrored to display identical effects, or grouped together to behave as a single logical strip.

---

## 2. Goals

- Control one to five addressable LED strips from a single ESP8266.
- Support mirroring of effects across multiple strips and grouping/pairing of strips.
- Provide remote state management through an MQTT broker.
- Run lighting programmes autonomously using an on-chip timer/scheduler.
- Support OTA firmware updates without physical access to the device.
- Store and recall multiple lighting programmes in non-volatile memory.
- Allow runtime configuration of strip length and the addition of further strips (up to 5).

---

## 3. Target Hardware

| Component | Specification |
|---|---|
| Microcontroller | ESP8266 (NodeMCU v3 / Wemos D1 Mini or equivalent) |
| LED Strips | WS2812B (NeoPixel) 5V addressable RGB LED strips (minimum 1, maximum 5) |
| Power Supply | 5V DC, rated for total LED current draw (approx. 60 mA per LED at full white) |
| Level Shifter | 3.3V to 5V logic level shifter (e.g. 74HCT245, SN74AHCT125) |
| Capacitor | 1000 uF electrolytic across power supply rails |
| Resistor | 330-470 ohm on each data line between ESP8266 and LED strip |

---

## 4. Build Toolchain & Environment

### 4.1 PlatformIO

**PlatformIO CLI** is the chosen build system for this and future ESP8266 projects. It was selected over Arduino CLI and Arduino IDE for the following reasons:

- Full Arduino library ecosystem compatibility (all project libraries work out of the box)
- Built-in LittleFS filesystem image building (`pio run -t buildfs`)
- Built-in OTA upload support (`upload_protocol = espota`)
- Per-project dependency management with version pinning via `platformio.ini`
- Headless CI/CD operation (`pip install platformio && pio run`)

#### Reference `platformio.ini`

```ini
[env:nodemcuv2]
platform = espressif8266
board = nodemcuv2
framework = arduino
monitor_speed = 115200
board_build.filesystem = littlefs
board_build.ldscript = eagle.flash.4m2m.ld

lib_deps =
    fastled/FastLED
    knolleary/PubSubClient
    bblanchon/ArduinoJson
    tzapu/WiFiManager
    arduino-libraries/NTPClient

upload_protocol = esptool
upload_speed = 921600

; OTA upload (uncomment and set IP when deploying over-the-air)
; upload_protocol = espota
; upload_port = 192.168.1.50
```

### 4.2 Build Environment — LXD Container

All builds run inside an **LXD container** to provide a reproducible, isolated environment. This applies to both local development and CI/CD pipelines.

| Property | Value |
|---|---|
| Container type | LXD (system container) |
| Base image | Ubuntu 24.04 LTS (or latest LTS) |
| Python | 3.x (required by PlatformIO) |
| PlatformIO install | `pip install platformio` inside the container |
| USB passthrough | Required for serial flashing (`lxc config device add <container> ttyUSB0 unix-char path=/dev/ttyUSB0`) |
| Network | Bridged — container must reach the local network for OTA uploads and MQTT broker access |

#### LXD Container Setup (reference)

```bash
# Create and launch the build container
lxc launch ubuntu:24.04 esp8266-build

# Install dependencies
lxc exec esp8266-build -- apt update
lxc exec esp8266-build -- apt install -y python3 python3-pip python3-venv git

# Install PlatformIO
lxc exec esp8266-build -- pip3 install platformio

# (Optional) Pass through USB serial device for flashing
lxc config device add esp8266-build ttyUSB0 unix-char path=/dev/ttyUSB0

# Build the project
lxc exec esp8266-build -- bash -c "cd /path/to/project && pio run"
```

**Note:** OTA uploads do not require USB passthrough — they operate over the network. USB passthrough is only needed for initial serial flashing of a new device.

### 4.3 CI/CD — GitHub Actions + LXD

The project uses **GitHub** for version control and **GitHub Actions** for CI/CD automation. Full details in `docs/ci-cd.md`.

| Runner Type | Location | Purpose |
|---|---|---|
| GitHub-hosted | GitHub cloud | Build firmware, run native tests, create releases |
| Self-hosted LXD | Local network | OTA firmware deployment to physical ESP8266 devices |

**Pipeline triggers:**
- **Push / PR**: Build firmware, check binary size against 500KB OTA limit, run native unit tests
- **Tag `v*`**: Build, test, create GitHub Release with `firmware.bin` and `littlefs.bin` attached
- **Manual dispatch**: OTA deploy a release to one or all devices on the local network via self-hosted runner

---

## 5. Functional Requirements

### 4.1 LED Strip Control

| ID | Requirement |
|---|---|
| FR-01 | The system SHALL control a minimum of 1 and a maximum of 5 addressable LED strips per ESP8266 device. |
| FR-02 | Each strip SHALL support configurable length (number of LEDs) at runtime via MQTT or stored configuration. |
| FR-03 | The system SHALL support adding additional strips up to 5 total, limited by available GPIO pins: D1 (GPIO5), D2 (GPIO4), D5 (GPIO14), D6 (GPIO12), D7 (GPIO13). |
| FR-04 | Each strip SHALL support individual pixel colour control (RGB, 8-bit per channel). |
| FR-05 | The system SHALL support brightness control per strip (0-255). |
| FR-06 | The system SHALL support **mirroring mode**: two or more strips display identical effects simultaneously. The mirror source strip's programme, colour, and brightness are replicated to all mirror target strips. If strips differ in length, the effect SHALL be scaled or truncated to fit. |
| FR-07 | The system SHALL support **strip grouping/pairing**: two or more strips are logically combined into a single virtual strip. Programmes and effects render across the group as if it were one continuous strip, with pixel addressing spanning all member strips in sequence. |
| FR-08 | Each strip SHALL operate in one of three modes: **independent** (default), **mirror source/target**, or **member of a group**. Mode is configurable via MQTT or stored configuration. |
| FR-09 | A strip SHALL belong to at most one group or mirror relationship at a time. Assigning a new mode SHALL automatically remove the strip from any previous group or mirror. |

### 4.2 MQTT Integration

| ID | Requirement |
|---|---|
| FR-10 | The system SHALL connect to a configurable MQTT broker over Wi-Fi. |
| FR-11 | The system SHALL subscribe to command topics to receive state changes for each strip. |
| FR-12 | The system SHALL publish status topics reflecting the current state of each strip. |
| FR-13 | The system SHALL support the following MQTT command types: power on/off, set colour, set brightness, select programme, set strip length, trigger OTA update. |
| FR-14 | The system SHALL reconnect to the MQTT broker automatically on connection loss. |
| FR-15 | The system SHALL support MQTT authentication (username/password). |
| FR-16 | The system SHALL use a configurable base topic (e.g. `home/led/{device_id}/`). |

#### 4.2.1 MQTT Topic Structure

```
{base_topic}/strip/{n}/command    — Incoming commands (JSON payload)
{base_topic}/strip/{n}/state      — Outgoing state (JSON payload)
{base_topic}/group/{g}/command    — Incoming commands for a strip group
{base_topic}/group/{g}/state      — Outgoing state for a strip group
{base_topic}/config/set           — Device-level configuration commands
{base_topic}/config/state         — Current device configuration
{base_topic}/ota/trigger          — Trigger OTA update
{base_topic}/status               — Device online/offline (LWT)
```

#### 4.2.2 Example Command Payload

```json
{
  "power": "on",
  "programme": "rainbow_cycle",
  "brightness": 180,
  "colour": {"r": 255, "g": 100, "b": 0},
  "speed": 50
}
```

#### 4.2.3 Mirroring Configuration Payload

Publish to `{base_topic}/config/set`:

```json
{
  "mirror": {
    "source": 1,
    "targets": [2, 3],
    "scale_mode": "stretch"
  }
}
```

`scale_mode` options: `"stretch"` (scale effect to fit target length), `"truncate"` (clip at target length), `"wrap"` (repeat effect to fill target).

#### 4.2.4 Strip Group Configuration Payload

Publish to `{base_topic}/config/set`:

```json
{
  "group": {
    "id": "A",
    "name": "living_room",
    "strips": [1, 2, 3],
    "order": [1, 2, 3]
  }
}
```

The `order` array defines the physical sequence in which strips are chained for pixel addressing. Once grouped, commands sent to `{base_topic}/group/A/command` apply across all member strips as one continuous strip.

To dissolve a group:

```json
{
  "group": {
    "id": "A",
    "action": "dissolve"
  }
}
```

### 4.3 Lighting Programmes

| ID | Requirement |
|---|---|
| FR-20 | The system SHALL store multiple lighting programmes in non-volatile memory (SPIFFS/LittleFS). |
| FR-21 | A programme SHALL define a sequence of colours, transitions, timing, and effects for a strip. |
| FR-22 | The system SHALL include a set of built-in programmes: static colour, colour fade, rainbow cycle, chase, breathe, strobe, colour wipe. |
| FR-23 | Custom programmes SHALL be uploadable via MQTT as JSON definitions. |
| FR-24 | Programmes SHALL be selectable per strip independently. |
| FR-25 | The system SHALL support a maximum of 20 stored programmes (limited by available flash). |

#### 4.3.1 Programme Definition Format

```json
{
  "name": "sunset_fade",
  "version": 1,
  "steps": [
    {"colour": {"r": 255, "g": 80, "b": 0}, "duration_ms": 3000, "transition": "fade"},
    {"colour": {"r": 200, "g": 30, "b": 50}, "duration_ms": 3000, "transition": "fade"},
    {"colour": {"r": 50, "g": 0, "b": 80}, "duration_ms": 5000, "transition": "fade"}
  ],
  "loop": true
}
```

### 4.4 Timer / Scheduler

| ID | Requirement |
|---|---|
| FR-30 | The system SHALL support time-based scheduling of strip states and programmes. |
| FR-31 | Schedules SHALL be configurable via MQTT. |
| FR-32 | The system SHALL synchronise time via NTP on boot and periodically thereafter. |
| FR-33 | The system SHALL support a minimum of 10 schedule entries. |
| FR-34 | Each schedule entry SHALL specify: time (HH:MM), days of week, target strip(s), action (programme, colour, power state). |
| FR-35 | Schedules SHALL persist across reboots (stored in non-volatile memory). |

### 4.5 OTA Updates

| ID | Requirement |
|---|---|
| FR-40 | The system SHALL support OTA firmware updates via the Arduino OTA protocol or HTTP OTA. |
| FR-41 | OTA updates SHALL be triggerable via MQTT command or physical button (if available). |
| FR-42 | The system SHALL verify firmware integrity before applying (checksum/hash). |
| FR-43 | The system SHALL report OTA progress and result via MQTT. |
| FR-44 | A failed OTA update SHALL NOT brick the device; the existing firmware SHALL remain operational. |

### 4.6 Configuration & Persistence

| ID | Requirement |
|---|---|
| FR-50 | All configuration (Wi-Fi credentials, MQTT settings, strip pin assignments, strip lengths) SHALL be stored in non-volatile memory. |
| FR-51 | The system SHALL provide a Wi-Fi captive portal (AP mode) for initial setup if no Wi-Fi credentials are stored. |
| FR-52 | Configuration SHALL be updateable via MQTT without reflashing. |
| FR-53 | The system SHALL support factory reset via a physical button hold (5+ seconds). |

---

## 6. Non-Functional Requirements

| ID | Requirement |
|---|---|
| NFR-01 | The system SHALL boot and connect to Wi-Fi and MQTT within 10 seconds under normal conditions. |
| NFR-02 | LED refresh rate SHALL be a minimum of 30 FPS for smooth animations. |
| NFR-03 | The system SHALL operate continuously without memory leaks or watchdog resets. |
| NFR-04 | MQTT message processing latency SHALL be under 100 ms from receipt to LED state change. |
| NFR-05 | The system SHALL function in ambient temperatures of 0 to 50 degrees Celsius. |
| NFR-06 | Total RAM usage SHALL remain within ESP8266 limits (~80 KB usable heap). |
| NFR-07 | The firmware binary SHALL fit within the OTA-compatible partition scheme (max ~500 KB per slot). |

---

## 7. Wiring Schematic

```
                          +5V Power Supply
                          +-----------+
                          |  +5V  GND |
                          +--+-----+--+
                             |     |
              +--------------+-----|-------------+
              |              |     |             |
         +----+----+    +---+---+ |        +----+----+
         | 1000 uF |    |       | |        |         |
         |   Cap   |    | Level | |        |  ESP8266 |
         +----+----+    | Shift | |        | (3.3V)  |
              |         | 3.3>5 | |        |         |
              +---------+       | |   3.3V-+ VIN/5V  |
                        |       | +--------+ GND     |
                        +---+---+    |     |         |
                            |        |     | GPIO D1 +--[330R]--+
                            |        |     | GPIO D2 +--[330R]--|---------+
                            |        |     |         |          |         |
                            |        |     +---------+          |         |
                            |        |                          |         |
                    5V Data |   3.3V |                          |         |
                    Lines   |   Data |                          |         |
                            |   Lines|                          |         |
                            |        |                          |         |
                       +----+--------+----+               +-----+---------+----+
                       |    Level Shifter |               |     Level Shifter  |
                       |    Ch1 Out       |               |     Ch2 Out        |
                       +--------+---------+               +---------+----------+
                                |                                   |
                    +-----------+-----------+           +-----------+-----------+
                    |  WS2812B Strip 1      |           |  WS2812B Strip 2      |
                    |  DIN   +5V   GND      |           |  DIN   +5V   GND      |
                    +--+------+-----+-------+           +--+------+-----+-------+
                       |      |     |                      |      |     |
                       |   +--+     +--+                   |   +--+     +--+
                       |   | +5V from  |                   |   | +5V from  |
                       |   | PSU rail  |                   |   | PSU rail  |
                       |   +--+     +--+                   |   +--+     +--+
                       |      |     |                      |      |     |
                      DIN    +5V   GND                    DIN    +5V   GND
```

### Wiring Notes

1. **Power:** Never power the LED strips from the ESP8266's onboard regulator. Use a dedicated 5V power supply rated for the total number of LEDs (0.06A x number of LEDs at full white).
2. **Common Ground:** The ESP8266 GND, power supply GND, and LED strip GND must all be connected together.
3. **Level Shifting:** The ESP8266 outputs 3.3V logic. WS2812B LEDs require a minimum of 0.7 x VDD (3.5V at 5V) for reliable HIGH detection. A level shifter (74HCT245 or SN74AHCT125) is strongly recommended.
4. **Data Resistor:** Place a 330-470 ohm resistor in series on each data line, as close to the first LED as possible, to prevent signal reflections.
5. **Capacitor:** Place a 1000 uF electrolytic capacitor across the +5V and GND rails at the power supply connection point to absorb inrush current.
6. **GPIO Pins:** See the Supported GPIO Pin Map table below for all 5 usable data pins and pins to avoid.

### Supported GPIO Pin Map

| Strip Slot | Default Pin | GPIO | Notes |
|---|---|---|---|
| Strip 1 | D1 | GPIO5 | Default primary strip |
| Strip 2 | D2 | GPIO4 | Default secondary strip |
| Strip 3 | D5 | GPIO14 | Additional strip |
| Strip 4 | D6 | GPIO12 | Additional strip |
| Strip 5 | D7 | GPIO13 | Additional strip (maximum) |

**Pins to avoid:** GPIO0 (D3), GPIO2 (D4), GPIO15 (D8) are boot-mode pins. GPIO16 (D0) has no hardware PWM and limited library support. Using these pins can prevent the ESP8266 from booting correctly.

### Adding More Strips

To add a third, fourth, or fifth strip:
- Connect the new strip's data line to the next available GPIO (see pin map above) via a 330 ohm resistor and level shifter channel.
- Update the device configuration via MQTT to register the new pin and strip length.
- Ensure the power supply is rated for the additional load.
- A maximum of 5 strips is supported per ESP8266 device.

---

## 8. MQTT Endpoint Configuration Guide

### 8.1 Initial Setup via Captive Portal

1. Power on the ESP8266 with no stored Wi-Fi credentials.
2. The device creates an access point named `LED-Controller-XXXX` (where XXXX is the last 4 hex digits of the MAC address).
3. Connect to this AP from a phone or laptop (default password: `configure`).
4. A captive portal page opens automatically. Fill in:
   - **Wi-Fi SSID** and **password**
   - **MQTT Broker Host** (IP address or hostname)
   - **MQTT Broker Port** (default: 1883)
   - **MQTT Username** (leave blank if no auth)
   - **MQTT Password** (leave blank if no auth)
   - **MQTT Base Topic** (default: `home/led/{device_id}`)
   - **Device Name** (friendly name for identification)
5. Save. The device reboots and connects to the specified Wi-Fi and MQTT broker.

### 8.2 Updating MQTT Settings at Runtime

Publish to `{base_topic}/config/set`:

```json
{
  "mqtt_host": "192.168.1.100",
  "mqtt_port": 1883,
  "mqtt_user": "ledcontroller",
  "mqtt_pass": "secretpassword",
  "mqtt_base_topic": "home/led/lounge"
}
```

The device will save the new settings and reconnect.

### 8.3 Default Configuration Header

For initial serial flashing (before captive portal is available), defaults can be set in a config header:

```cpp
// config.h — Default MQTT configuration
#define WIFI_SSID          "your_wifi_ssid"
#define WIFI_PASSWORD      "your_wifi_password"
#define MQTT_HOST          "192.168.1.100"
#define MQTT_PORT          1883
#define MQTT_USER          "ledcontroller"
#define MQTT_PASS          "secretpassword"
#define MQTT_BASE_TOPIC    "home/led/device01"

// Strip defaults
#define STRIP_1_PIN        D1
#define STRIP_1_LENGTH     60
#define STRIP_2_PIN        D2
#define STRIP_2_LENGTH     30
```

### 8.4 PlatformIO Build & Flash

See Section 4 for the full `platformio.ini` reference and LXD container setup.

```bash
# Build firmware
pio run

# Build LittleFS filesystem image
pio run -t buildfs

# Flash firmware via USB serial
pio run -t upload

# Flash filesystem via USB serial
pio run -t uploadfs

# Flash firmware via OTA (set upload_port in platformio.ini first)
pio run -t upload --upload-port 192.168.1.50
```

### 8.5 Required Libraries

| Library | Purpose |
|---|---|
| `ESP8266WiFi` | Wi-Fi connectivity (built-in) |
| `PubSubClient` | MQTT client |
| `ArduinoJson` | JSON parsing for MQTT payloads |
| `Adafruit_NeoPixel` or `FastLED` | LED strip control |
| `ArduinoOTA` | Over-the-air updates |
| `LittleFS` | Non-volatile file storage |
| `ESP8266WebServer` | Captive portal for initial setup |
| `DNSServer` | Captive portal DNS redirect |
| `NTPClient` | Time synchronisation for scheduler |
| `WiFiManager` | Optional — simplified captive portal setup |

### 8.6 OTA Update Procedure

**Via PlatformIO:**
1. Ensure the device is on the same network as the build environment (LXD container must have bridged networking).
2. Set the device IP in `platformio.ini` or pass it on the command line:
   ```bash
   pio run -t upload --upload-port 192.168.1.50
   ```
3. PlatformIO handles the espota protocol automatically.

**Via MQTT-triggered HTTP OTA:**
1. Host the compiled `.bin` firmware file on an HTTP server accessible to the device.
2. Publish to `{base_topic}/ota/trigger`:
   ```json
   {
     "url": "http://192.168.1.10:8080/firmware.bin",
     "checksum": "sha256:abcdef1234567890..."
   }
   ```
3. The device downloads, verifies, and applies the update, then reboots.
4. Monitor progress on `{base_topic}/ota/status`.

---

## 9. Software Architecture

```
+-------------------------------------------------------------------+
|                          Main Loop                                 |
|  +----------+  +---------+  +----------+  +-------------------+   |
|  | WiFi Mgr |  | MQTT    |  | OTA      |  | NTP/Scheduler     |   |
|  |          |  | Client  |  | Handler  |  |                   |   |
|  +----+-----+  +----+----+  +----+-----+  +--------+----------+   |
|       |              |            |                 |              |
|  +----+--------------+------------+-----------------+------------+ |
|  |                     Command Router                            | |
|  +----+----------+-------------------+-------------------+-------+ |
|       |          |                   |                   |         |
|  +----+-----+  +-+---------------+  +---------+------+  +-------+ |
|  | Strip    |  | Mirror/Group    |  | Programme      |  |Config | |
|  | Manager  |  | Manager         |  | Engine         |  |Manager| |
|  | (FastLED |  | (sync, scale,   |  | (step exec,    |  |(R/W,  | |
|  |  driver, |  |  virtual strip  |  |  transitions)  |  | JSON) | |
|  |  1-5     |  |  addressing)    |  |                |  |       | |
|  |  strips) |  +-----------------+  +----------------+  +-------+ |
|  +----------+                                                      |
+-------------------------------------------------------------------+
```

---

## 10. Constraints & Risks

| Risk | Mitigation |
|---|---|
| ESP8266 has limited GPIO (5 usable pins for LED data) | Hard maximum of 5 strips per device; pin map documented in Section 7 |
| ESP8266 has limited RAM (~80 KB) | Limit max LEDs per strip; stream large payloads; avoid String fragmentation |
| Single-core processor — blocking LED writes stall MQTT | Use non-blocking LED libraries (FastLED `show()` is blocking but brief); keep strip lengths reasonable. With 5 strips the blocking time increases proportionally. |
| Mirror/group synchronisation jitter | Mirrored and grouped strips are updated in a single render pass to minimise visible desynchronisation between strips |
| Wi-Fi and MQTT instability | Implement reconnect logic with exponential backoff |
| Power supply sizing | Document current calculation in setup guide; warn if configured LED count exceeds safe thresholds |
| OTA bricking | Use dual-partition OTA scheme; verify checksum before applying |

---

## 11. Future Considerations

- **ESP32 Migration:** For projects requiring more strips, more RAM, dual-core operation, or Bluetooth, an ESP32 variant can be substituted with minimal firmware changes.
- **Home Assistant Integration:** MQTT discovery protocol support for automatic entity creation in Home Assistant.
- **Web Dashboard:** Embedded web UI for configuration and live strip preview.
- **Music Reactivity:** Microphone input (analog pin) for audio-reactive programmes.
- **E1.31 / Art-Net Support:** For integration with professional lighting control software.

---

## 12. Success Criteria

- A single LED strip operating with stored programmes (minimum viable configuration).
- Up to 5 LED strips operating independently with different programmes.
- Mirroring mode: 2+ strips displaying identical synchronised effects.
- Group mode: 2+ strips behaving as a single continuous virtual strip.
- Full MQTT control of power, colour, brightness, programme selection, strip length, mirror, and group configuration.
- At least 5 programmes stored and recallable without reflashing.
- Timer-based automatic programme switching verified over 24-hour cycle.
- Successful OTA firmware update without physical access.
- Captive portal setup completed from a mobile device with no prior configuration.
- Full build and OTA deploy executed from within an LXD container using PlatformIO CLI.

---

## 13. Glossary

| Term | Definition |
|---|---|
| WS2812B | Addressable RGB LED with integrated controller, commonly branded as NeoPixel |
| MQTT | Message Queuing Telemetry Transport — lightweight pub/sub messaging protocol |
| OTA | Over-The-Air — firmware update delivered over Wi-Fi |
| LWT | Last Will and Testament — MQTT feature for detecting client disconnection |
| LittleFS | Lightweight filesystem for ESP8266 flash storage |
| NTP | Network Time Protocol — used for clock synchronisation |
| GPIO | General Purpose Input/Output — microcontroller pins |
| PlatformIO | Open-source build system and dependency manager for embedded development |
| LXD | Linux system container manager — used as the isolated build environment for this project |

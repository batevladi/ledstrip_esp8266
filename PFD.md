# Product Feature Document: ESP8266 Programmable LED Strip Controller

**Version:** 1.0
**Date:** 2026-03-16
**Status:** Draft
**Related:** [PRD.md](PRD.md) v1.2

---

## 1. Overview

This document defines the feature set for the ESP8266 Programmable LED Strip Controller, organised into three implementation phases. Each phase delivers a working system with incrementally richer capability. Phase 1 produces a standalone device that operates without any external automation infrastructure.

---

## 2. Phasing Strategy

| Phase | Name | Summary |
|---|---|---|
| Phase 1 | Standalone Device (MVP) | Strip control, 4 built-in programmes, web UI on connection failure, basic persistence |
| Phase 2 | MQTT Control, Programmes & Scheduling | Full MQTT integration, custom programmes, timer/scheduler, Wi-Fi/MQTT config persistence |
| Phase 3 | Multi-Strip Advanced, OTA & Hardening | Mirror/reverse mirror, strip grouping, OTA updates, factory reset |

Each phase builds on the previous — Phase 2 requires Phase 1 complete, Phase 3 requires Phase 2 complete.

---

## 3. Feature-to-Phase Dependency Map

| Feature | Phase 1 | Phase 2 | Phase 3 |
|---|---|---|---|
| Strip driving (1-5 strips, default D1) | Yes | | |
| Brightness control per strip | Yes | | |
| 4 built-in programmes | Yes | | |
| Web UI (connection failure only) | Yes | | |
| Strip config + last programme persistence | Yes | | |
| MQTT command/state topics | | Yes | |
| Wi-Fi/MQTT config persistence | | Yes | |
| Custom JSON programmes | | Yes | |
| Programme storage (LittleFS, max 20) | | Yes | |
| Timer/Scheduler + NTP | | Yes | |
| Runtime config via MQTT | | Yes | |
| Mirror mode | | | Yes |
| Reverse mirror mode | | | Yes |
| Strip grouping/pairing | | | Yes |
| OTA updates (espota + HTTP via MQTT) | | | Yes |
| Factory reset (physical button) | | | Yes |

---

## 4. Phase 1 — Standalone Device (MVP)

### 4.1 Strip Control

| ID | Feature |
|---|---|
| F1-01 | Drive 1 to 5 WS2812B addressable LED strips from a single ESP8266. |
| F1-02 | Default to Strip 1 on pin D1 (GPIO5) when no configuration exists. Additional strips require manual configuration via the web UI. |
| F1-03 | Support configurable strip length (number of LEDs) per strip via the web UI. |
| F1-04 | Support brightness control per strip (0-255) via the web UI. |
| F1-05 | Selected programme applies to all connected strips simultaneously. Each strip renders the full effect independently for its own pixel count. |

### 4.2 Built-In Programmes

Four default programmes are available from first boot with no configuration required.

| ID | Programme | Description |
|---|---|---|
| F1-10 | **Sunset** | Warm colour fade cycling through deep orange, red-orange, rose, and deep purple. Smooth fade transitions between colours. Looping. |
| F1-11 | **Running Rainbow** | Classic rainbow pattern scrolling continuously across the strip. Full hue spectrum distributed evenly across the strip length. Continuous motion, looping. |
| F1-12 | **Nightlight** | Soft, warm low-brightness glow with a gentle breathing/pulsing effect. Suitable for ambient lighting in a dark room. Looping. |
| F1-13 | **Sky at Night** | Dark blue base representing the night sky. Sparse white and light-yellow pixels fade in and out at random positions, simulating twinkling stars. Star positions and colours are randomised each cycle. New random sequence generated on each loop iteration. |

All four programmes:
- Loop continuously once selected.
- Render independently per strip based on that strip's pixel count (compressed rendering).
- Persist as the active programme across reboots (last-selected is saved).

### 4.3 Web UI (Captive Portal)

| ID | Feature |
|---|---|
| F1-20 | The web UI SHALL only be presented when the device fails to connect to Wi-Fi or fails to reach the MQTT broker. Under normal operation, no web server runs (conserving RAM). |
| F1-21 | The web UI SHALL provide Wi-Fi configuration: SSID and password entry. |
| F1-22 | The web UI SHALL provide MQTT broker configuration: host, port, username, password, base topic. |
| F1-23 | The web UI SHALL provide strip configuration: for each strip, set the GPIO pin and LED count. |
| F1-24 | The web UI SHALL provide programme selection: 4 buttons, one per built-in programme. Selecting a programme applies it to all connected strips immediately. |
| F1-25 | The web UI SHALL display the device name and current status (connected/disconnected, active programme, strip configuration). |

### 4.4 Persistence (Phase 1)

| ID | Feature |
|---|---|
| F1-30 | Strip configuration (pin assignments, LED counts) SHALL be stored in LittleFS and restored on boot. |
| F1-31 | The last-selected programme SHALL be stored in LittleFS and automatically resumed on boot. |
| F1-32 | Brightness settings per strip SHALL be stored in LittleFS and restored on boot. |

**Note:** Wi-Fi and MQTT credentials are entered via the web UI in Phase 1 but persistence and auto-reconnect behaviour for these settings is delivered in Phase 2.

### 4.5 Boot Sequence (Phase 1)

```
Power on
  → Load strip config from LittleFS (or default to Strip 1, D1)
  → Attempt Wi-Fi connection
    → Success: Attempt MQTT connection
      → Success: Normal operation, restore last programme
      → Fail: Launch captive portal web UI
    → Fail: Launch captive portal web UI
```

---

## 5. Phase 2 — MQTT Control, Programmes & Scheduling

### 5.1 MQTT Integration

| ID | Feature |
|---|---|
| F2-01 | Connect to the configured MQTT broker over Wi-Fi on boot. |
| F2-02 | Subscribe to per-strip command topics: `{base_topic}/strip/{n}/command`. |
| F2-03 | Publish per-strip state topics: `{base_topic}/strip/{n}/state`. |
| F2-04 | Support MQTT commands: power on/off, set colour (RGB), set brightness, select programme, set strip length. |
| F2-05 | Auto-reconnect to MQTT broker on connection loss with exponential backoff. |
| F2-06 | Support MQTT authentication (username/password). |
| F2-07 | Use a configurable base topic (default: `home/led/{device_id}`). |
| F2-08 | Publish device online/offline status via MQTT Last Will and Testament (LWT) on `{base_topic}/status`. |

#### 5.1.1 MQTT Topic Structure

```
{base_topic}/strip/{n}/command    — Incoming commands (JSON payload)
{base_topic}/strip/{n}/state      — Outgoing state (JSON payload)
{base_topic}/config/set           — Device-level configuration commands
{base_topic}/config/state         — Current device configuration
{base_topic}/status               — Device online/offline (LWT)
```

#### 5.1.2 Command Payload Format

```json
{
  "power": "on",
  "programme": "rainbow_cycle",
  "brightness": 180,
  "colour": {"r": 255, "g": 100, "b": 0},
  "speed": 50
}
```

All fields are optional — only included fields are applied. Omitted fields retain their current value.

### 5.2 Strip Configuration via MQTT

| ID | Feature |
|---|---|
| F2-10 | Strip pin assignments and LED counts SHALL be configurable via MQTT publish to `{base_topic}/config/set`. |
| F2-11 | Adding a new strip at runtime: publish pin and length; device begins driving the new strip immediately. |
| F2-12 | Removing a strip at runtime: publish strip removal command; device stops driving that pin. |
| F2-13 | Strip configuration changes via MQTT SHALL be persisted to LittleFS. |

### 5.3 Custom Programmes

| ID | Feature |
|---|---|
| F2-20 | Custom lighting programmes SHALL be uploadable via MQTT as JSON definitions. |
| F2-21 | A programme definition specifies: name, sequence of colour steps, transition type per step, duration per step, and loop flag. |
| F2-22 | Custom programmes SHALL be stored in LittleFS (maximum 20 stored programmes, including the 4 built-ins). |
| F2-23 | Programmes SHALL be selectable per strip independently via MQTT. |
| F2-24 | The 4 Phase 1 built-in programmes remain available and cannot be deleted. |
| F2-25 | Custom programmes SHALL be deletable via MQTT command. |

#### 5.3.1 Programme Definition Format

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

### 5.4 Timer / Scheduler

| ID | Feature |
|---|---|
| F2-30 | Synchronise time via NTP on boot and periodically thereafter. |
| F2-31 | Support up to 10 schedule entries, configurable via MQTT. |
| F2-32 | Each schedule entry specifies: time (HH:MM), days of week, target strip(s) or all, action (select programme, set colour, set power state). |
| F2-33 | Schedules SHALL persist across reboots (stored in LittleFS). |
| F2-34 | Schedule entries SHALL be creatable, updatable, and deletable via MQTT. |

#### 5.4.1 Schedule Entry Format

```json
{
  "schedule": {
    "id": 1,
    "time": "22:30",
    "days": ["mon", "tue", "wed", "thu", "fri", "sat", "sun"],
    "strips": "all",
    "action": {
      "programme": "nightlight",
      "brightness": 60
    }
  }
}
```

### 5.5 Wi-Fi & MQTT Config Persistence

| ID | Feature |
|---|---|
| F2-40 | Wi-Fi credentials (SSID, password) SHALL be stored in LittleFS and used for auto-connection on boot. |
| F2-41 | MQTT broker settings (host, port, username, password, base topic) SHALL be stored in LittleFS. |
| F2-42 | MQTT settings SHALL be updateable at runtime via MQTT publish to `{base_topic}/config/set`; the device saves and reconnects. |
| F2-43 | Device name SHALL be stored and used in MQTT client ID and LWT messages. |

---

## 6. Phase 3 — Multi-Strip Advanced, OTA & Hardening

### 6.1 Mirror Mode

| ID | Feature |
|---|---|
| F3-01 | A source strip's active programme SHALL be replicated to one or more target strips. |
| F3-02 | **Compressed rendering (implemented):** Each target strip renders the full effect independently for its own pixel count. A 60-LED source and 30-LED target both display the complete effect, calculated for their respective lengths. Same speed and duration. |
| F3-03 | Mirror configuration is set via MQTT publish to `{base_topic}/config/set`. |
| F3-04 | Mirror relationships SHALL be persisted to LittleFS. |

#### 6.1.1 Mirror Configuration Payload

```json
{
  "mirror": {
    "source": 1,
    "targets": [2, 3],
    "mode": "mirror"
  }
}
```

#### 6.1.2 Future: Uncompressed Rendering (Not Implemented)

**Documented for future consideration.** In uncompressed mode, the target strip would display a cropped window of the effect at native pixel scale rather than rendering the full effect compressed to the target's length. This requires:
- Designating a reference-length strip.
- Staggered start/stop timing for strips shorter than the reference.
- Additional coordination logic in the render loop.

This mode is deferred due to implementation complexity and uncertain visual benefit. It may be revisited based on user feedback after Phase 3 ships.

### 6.2 Reverse Mirror Mode

| ID | Feature |
|---|---|
| F3-10 | Same as mirror mode, but the pixel order on target strips is inverted (first pixel of effect maps to last pixel of strip). |
| F3-11 | Uses the same compressed rendering approach: the full effect is rendered for the target's pixel count, then the pixel buffer is reversed. |
| F3-12 | Configurable via MQTT alongside standard mirror mode. |

#### 6.2.1 Reverse Mirror Configuration Payload

```json
{
  "mirror": {
    "source": 1,
    "targets": [2],
    "mode": "reverse"
  }
}
```

### 6.3 Strip Grouping / Pairing

| ID | Feature |
|---|---|
| F3-20 | Two or more strips SHALL be combinable into a single virtual strip with continuous pixel addressing. |
| F3-21 | The group's strip order defines the physical sequence for pixel addressing (strip 1 pixels, then strip 2 pixels, etc.). |
| F3-22 | Commands sent to `{base_topic}/group/{g}/command` apply across all member strips as one continuous strip. |
| F3-23 | Group state is published to `{base_topic}/group/{g}/state`. |
| F3-24 | Group configuration SHALL be persisted to LittleFS. |

#### 6.3.1 Group Configuration Payload

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

To dissolve a group:

```json
{
  "group": {
    "id": "A",
    "action": "dissolve"
  }
}
```

### 6.4 Mode Exclusivity

| ID | Feature |
|---|---|
| F3-30 | Each strip SHALL operate in exactly one mode: **independent** (default), **mirror source**, **mirror target**, **reverse mirror target**, or **group member**. |
| F3-31 | Assigning a strip to a new mode SHALL automatically remove it from any previous mirror or group relationship. |
| F3-32 | Mode assignments SHALL be persisted to LittleFS. |

### 6.5 OTA Updates

| ID | Feature |
|---|---|
| F3-40 | Support OTA firmware upload via PlatformIO espota protocol (direct from build environment). |
| F3-41 | Support HTTP OTA triggered via MQTT: publish firmware URL and SHA256 checksum to `{base_topic}/ota/trigger`. |
| F3-42 | The device SHALL download, verify checksum, and apply the firmware update, then reboot. |
| F3-43 | OTA progress and result SHALL be reported via MQTT on `{base_topic}/ota/status`. |
| F3-44 | Dual-partition OTA scheme: a failed update SHALL NOT brick the device; the existing firmware remains operational. |

#### 6.5.1 OTA Trigger Payload

```json
{
  "url": "http://192.168.1.10:8080/firmware.bin",
  "checksum": "sha256:abcdef1234567890..."
}
```

### 6.6 Factory Reset

| ID | Feature |
|---|---|
| F3-50 | A physical button hold (5+ seconds) SHALL wipe all configuration from LittleFS and reboot the device. |
| F3-51 | After factory reset, the device boots with default settings (Strip 1 on D1, no Wi-Fi/MQTT config) and launches the captive portal web UI. |

---

## 7. Programme Rendering Rules

These rules apply across all phases and all strip modes.

| Rule | Description |
|---|---|
| **Per-strip rendering** | Every strip receives its own call to the programme's render function with its own pixel count as input. No pixel buffer is shared or resampled between strips. |
| **Compressed mode** | The full effect is rendered within the strip's pixel count. A 30-LED strip shows the same complete effect as a 60-LED strip, just across fewer pixels. |
| **Speed and duration** | All strips in a mirror or group relationship run at the same speed and duration. Timing is synchronised; pixel count is independent. |
| **Reverse mirror** | The render function is called normally for the target's pixel count, then the resulting pixel buffer is reversed before output. |
| **Groups** | The render function is called once with the total pixel count of all member strips. The resulting buffer is then split and distributed to each strip in order. |

---

## 8. Non-Functional Requirements

Carried forward from PRD.md Section 6. These apply across all phases.

| ID | Requirement | Phase |
|---|---|---|
| NFR-01 | Boot and connect to Wi-Fi and MQTT within 10 seconds | Phase 1+ |
| NFR-02 | LED refresh rate minimum 30 FPS for smooth animations | Phase 1+ |
| NFR-03 | Continuous operation without memory leaks or watchdog resets | Phase 1+ |
| NFR-04 | MQTT message processing latency under 100 ms | Phase 2+ |
| NFR-05 | Operate in ambient temperatures 0-50 degrees Celsius | Phase 1+ |
| NFR-06 | RAM usage within ESP8266 limits (~80 KB usable heap) | Phase 1+ |
| NFR-07 | Firmware binary fits OTA-compatible partition scheme (max ~500 KB per slot) | Phase 3 |

---

## 9. Acceptance Criteria per Phase

### Phase 1 — Standalone Device (MVP)

- [ ] Single strip on D1 operates with no configuration on first boot.
- [ ] All 4 built-in programmes run correctly and loop.
- [ ] Each connected strip renders effects independently for its own pixel count.
- [ ] Web UI appears when Wi-Fi connection fails.
- [ ] Web UI allows Wi-Fi/MQTT config entry, strip config, and programme selection.
- [ ] Strip config and last-selected programme persist across reboots.
- [ ] Web UI does not run during normal operation.
- [ ] LED refresh rate is at least 30 FPS with 1-5 strips connected.

### Phase 2 — MQTT Control, Programmes & Scheduling

- [ ] Device connects to MQTT broker and publishes LWT status.
- [ ] All MQTT commands (power, colour, brightness, programme, strip length) work per-strip.
- [ ] Custom JSON programme uploaded via MQTT, stored, and selectable.
- [ ] Maximum 20 programmes stored without error.
- [ ] At least one schedule entry triggers correctly at the specified time.
- [ ] Wi-Fi and MQTT settings persist across reboots.
- [ ] MQTT settings updatable at runtime via MQTT; device reconnects.
- [ ] Strip added/removed at runtime via MQTT.

### Phase 3 — Multi-Strip Advanced, OTA & Hardening

- [ ] Mirror mode: source programme displayed on target strip(s) with per-length compressed rendering.
- [ ] Reverse mirror mode: target strip shows reversed effect.
- [ ] Strip group: 2+ strips behave as single continuous virtual strip.
- [ ] Mode exclusivity enforced — assigning new mode removes previous relationship.
- [ ] OTA update via PlatformIO espota completes successfully.
- [ ] OTA update via MQTT-triggered HTTP completes successfully.
- [ ] Failed OTA does not brick the device.
- [ ] Factory reset via button hold wipes config and launches captive portal.
- [ ] Full build and OTA deploy from LXD container using PlatformIO CLI.

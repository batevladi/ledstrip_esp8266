# ESP8266 LED Strip Controller

## Autonomy
Canonical rule: `~/.claude/CLAUDE.md` "Autonomous to integration, gated to production".
- Integration branch: `main`; merging it deploys: nothing (see the OTA note below).
- Production: flashing or OTA-updating a device, always asked.
- CI: `.forgejo/workflows/checks.yml` on every push: `pio run -e nodemcuv2`, a 500 KB size gate, `pio test -e native -v`. By hand: the same build and test commands. PR host: Forgejo, via the `forgejo` remote only (`git push -u forgejo <branch>`; `origin` also pushes to GitHub, so never `git push origin`). `fj` reads `origin` and stops because it is GitHub, so prefix every `fj` call (`create`, `checks`, `comment`, `merge`) with this override: `GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=url.git@192.168.1.31:batevladi/ledstrip_esp8266.git.insteadOf GIT_CONFIG_VALUE_0=git@github.com:batevladi/ledstrip_esp8266.git fj pr create --base main`.
- Exceptions:
  - OTA deploy (`.github/workflows/deploy-ota.yml`) is `workflow_dispatch` only on GitHub, with a device IP input; a merge never triggers it. Any OTA push or USB flash is always asked.
  - Merges into `main` stay asked until the repo is onboarded Forgejo-first (`ci-platform/release-gate/setup-repo.sh`, run by the user): until then nothing carries a Forgejo merge to GitHub `main`, where `release.yml` builds `v*` tags, and pushing `main` to GitHub by hand is not allowed.
  - `main` carries no firmware sources until `phase/1` merges (the checks skip the build and tests there), so a green check on `main` proves nothing about firmware: once the repo is onboarded, code PRs into `main` stay asked and docs-only PRs follow the rule.

## Quick Reference

| Command | Purpose |
|---|---|
| `pio run -e nodemcuv2` | Build firmware |
| `pio test -e native` | Run host-side unit tests (no hardware) |
| `pio run -t upload -e nodemcuv2` | Flash firmware via USB serial |
| `pio run -t uploadfs -e nodemcuv2` | Flash LittleFS filesystem via USB serial |
| `pio run -t upload --upload-port <IP>` | Flash firmware via OTA |
| `pio device monitor` | Open serial monitor (115200 baud) |
| `pio run -t buildfs` | Build LittleFS image only |

## Project Documents

- **PRD**: `PRD.md` — hardware spec, requirements, wiring schematic, MQTT topic structure
- **PFD**: `PFD.md` — phased feature definitions (Phase 1-3), acceptance criteria
- **Phase 1 Plan**: `docs/plans/2026-03-16-phase1-standalone-mvp.md` — 18 tasks, full code
- **CI/CD**: `docs/ci-cd.md` — GitHub Actions + LXD pipeline, OTA deployment

## Architecture

Modular C++ (Arduino framework) with separated concerns:

| Module | Files | Responsibility |
|---|---|---|
| Config | `config.h`, `config_manager.h/.cpp` | Constants, LittleFS JSON persistence |
| Strips | `strip_manager.h/.cpp` | FastLED driver, 1-5 strips, per-strip brightness |
| Programmes | `programme_engine.h/.cpp`, `programmes/*.cpp` | Abstract Programme base, 4 built-ins, registry |
| Network | `wifi_manager.h/.cpp` | Wi-Fi STA connect, AP mode |
| Web Portal | `web_portal.h/.cpp`, `web_portal_html.h` | Captive portal (runs only on connection failure) |
| Main | `main.cpp` | Boot sequence, main loop |

## Conventions

- **Language**: C++ Arduino dialect targeting ESP8266
- **Build system**: PlatformIO CLI — never Arduino IDE
- **Build environment**: LXD container (Ubuntu 24.04)
- **HTML**: Always in PROGMEM (`web_portal_html.h`), never heap-allocated String
- **Programme interface**: `void render(CRGB* leds, uint16_t numLeds, uint32_t elapsedMs)` — pure rendering, no side effects
- **Config persistence**: ArduinoJson v7 + LittleFS at `/config.json`
- **String handling**: Prefer `char[]` with `strlcpy()` over Arduino `String` class (avoids heap fragmentation)
- **Timing**: Non-blocking — use `millis()` delta, never `delay()` in main loop
- **Serial debug**: Use `F()` macro for all string literals (`Serial.println(F("text"))`)

## Hardware Constraints

| Constraint | Limit |
|---|---|
| Usable GPIO pins for LED data | 5 only: GPIO5 (D1), GPIO4 (D2), GPIO14 (D5), GPIO12 (D6), GPIO13 (D7) |
| Boot-mode pins (never use) | GPIO0 (D3), GPIO2 (D4), GPIO15 (D8) |
| Usable heap RAM | ~80 KB |
| Firmware binary (OTA slot) | max ~500 KB |
| Max LED strips | 5 per device |
| Max LEDs per strip | 300 (RAM-limited) |
| Max stored programmes | 20 (flash-limited) |
| Target refresh rate | 60 FPS (16ms frame interval) |

## Build Environment (LXD)

```bash
# Build inside LXD container
lxc exec esp8266-build -- bash -c "cd /path/to/project && pio run"

# USB passthrough for serial flash (one-time setup)
lxc config device add esp8266-build ttyUSB0 unix-char path=/dev/ttyUSB0

# OTA does NOT require USB — works over bridged network
```

## CI/CD

GitHub Actions with a self-hosted runner inside LXD. See `docs/ci-cd.md` for full setup.

Checks run on Forgejo ci001 (`.forgejo/workflows/checks.yml`): build + size gate + native tests, mirroring build.yml.

- **On push/PR**: Build firmware + run native tests
- **On tag `v*`**: Build, create GitHub Release, attach firmware binary
- **OTA deploy**: Self-hosted runner on local network pushes firmware to devices

## Implementation Phases

| Phase | Status | Scope |
|---|---|---|
| Phase 1 — Standalone MVP | Planned | 1-5 strips, 4 built-in programmes, captive portal, persistence |
| Phase 2 — MQTT + Scheduling | Planned | MQTT control, custom programmes, timer/NTP, config persistence |
| Phase 3 — Advanced + OTA | Planned | Mirror/reverse mirror, grouping, OTA, factory reset |

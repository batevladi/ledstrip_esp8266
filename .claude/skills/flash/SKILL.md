---
name: flash
description: Build and flash firmware to ESP8266 via serial or OTA
disable-model-invocation: true
---

Build and flash the firmware.

## Current State
- Build environment: !`which pio 2>/dev/null && echo "PlatformIO found" || echo "PlatformIO NOT found"`
- USB devices: !`ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "No serial devices"`

## Arguments
$ARGUMENTS should be one of:
- `serial` — Build and upload via USB (default if no argument)
- `ota <ip>` — Build and upload via OTA to the given IP
- `build` — Build only, no upload
- `fs` — Build and upload LittleFS filesystem image

## Steps
1. Run `cd /home/vmgc/Projects/prog-chain/8266 && pio run -e nodemcuv2` to build
2. If build fails, report errors and stop
3. Report firmware size — must be under 500KB for OTA compatibility. Warn at 450KB.
4. Upload using the requested method:
   - serial: `pio run -t upload -e nodemcuv2`
   - ota: `pio run -t upload -e nodemcuv2 --upload-port <ip>`
   - fs: `pio run -t uploadfs -e nodemcuv2`
   - build: skip upload
5. If upload succeeded, open serial monitor briefly to verify boot: `timeout 10 pio device monitor`

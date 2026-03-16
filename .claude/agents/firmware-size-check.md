---
name: firmware-size-check
description: Check firmware binary size after builds to ensure OTA compatibility
allowed-tools: Bash, Read, Glob
---

After a PlatformIO build completes, check the firmware binary size:

1. Find the binary: `ls -la .pio/build/nodemcuv2/firmware.bin`
2. Report the size in bytes and KB
3. Compare against the 500KB OTA partition limit
4. If over 450KB (90% threshold), warn that we're approaching the limit
5. If over 500KB, flag as CRITICAL — OTA will not work

Also check RAM usage from the build output:
- Run `pio run -e nodemcuv2 -v 2>&1 | grep -E "RAM|DATA|BSS"` to find static RAM usage
- ESP8266 has ~80KB usable heap
- Warn if static RAM usage exceeds 40KB (leaves insufficient heap for runtime)

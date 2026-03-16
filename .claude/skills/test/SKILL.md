---
name: test
description: Run project test suites (native and/or embedded)
disable-model-invocation: true
---

Run the test suite.

## Arguments
$ARGUMENTS should be one of:
- `native` — Run host-side unit tests only (default if no argument, no hardware needed)
- `device` — Run on-device tests (requires connected ESP8266)
- `all` — Run both native and device tests

## Steps
1. For native: `cd /home/vmgc/Projects/prog-chain/8266 && pio test -e native -v`
2. Report pass/fail count and any failures with details
3. For device: `cd /home/vmgc/Projects/prog-chain/8266 && pio test -e nodemcuv2 -v`
4. Report results
5. If any tests fail, show the failing test name and assertion message

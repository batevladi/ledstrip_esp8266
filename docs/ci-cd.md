# CI/CD: GitHub Actions + LXD

## Overview

The CI/CD pipeline uses GitHub as the remote repository and GitHub Actions for automation, with builds executing inside LXD containers. Two runner types are used:

| Runner | Where | Purpose |
|---|---|---|
| **GitHub-hosted** | GitHub cloud | Build firmware, run native tests, create releases |
| **Self-hosted (LXD)** | Local network | OTA firmware deployment to physical ESP8266 devices |

```
Developer → git push → GitHub → GitHub Actions
                                    │
                    ┌───────────────┴───────────────┐
                    │                               │
            GitHub-hosted runner              Self-hosted LXD runner
            (build + test)                   (on local network)
                    │                               │
            ┌───────┴───────┐                       │
            │               │                       │
        Build OK?     Native tests              OTA deploy
            │           pass?                   to ESP8266
            │               │                   devices
            └───────┬───────┘
                    │
            Upload firmware
            as Release artifact
                    │
                    └──→ Self-hosted runner
                         pulls artifact &
                         deploys via OTA
```

---

## 1. Repository Setup

### Initialise Git and push to GitHub

```bash
cd /home/vmgc/Projects/prog-chain/8266

git init
git add .
git commit -m "feat: initial project scaffolding"

# Create repo on GitHub (requires gh CLI authenticated)
gh repo create esp8266-led-controller --private --source=. --push
```

### Branch strategy

| Branch | Purpose |
|---|---|
| `main` | Stable, tagged releases. Protected — PR-only merges. |
| `phase/1` | Phase 1 development branch |
| `phase/2` | Phase 2 development branch |
| `phase/3` | Phase 3 development branch |

---

## 2. GitHub-Hosted Runner Workflows

These workflows run on GitHub's cloud infrastructure. No local setup needed.

### 2.1 Build and Test on Push/PR

Create `.github/workflows/build.yml`:

```yaml
name: Build and Test

on:
  push:
    branches: [main, 'phase/*']
  pull_request:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Cache PlatformIO
        uses: actions/cache@v4
        with:
          path: |
            ~/.cache/pip
            ~/.platformio/.cache
            ~/.platformio/platforms
            ~/.platformio/packages
          key: ${{ runner.os }}-pio-${{ hashFiles('platformio.ini') }}
          restore-keys: |
            ${{ runner.os }}-pio-

      - uses: actions/setup-python@v5
        with:
          python-version: '3.x'

      - name: Install PlatformIO
        run: pip install --upgrade platformio

      - name: Build firmware
        run: pio run -e nodemcuv2

      - name: Check firmware size
        run: |
          SIZE=$(stat -c%s .pio/build/nodemcuv2/firmware.bin)
          SIZE_KB=$((SIZE / 1024))
          echo "Firmware size: ${SIZE_KB}KB / 500KB"
          if [ $SIZE -gt 512000 ]; then
            echo "::error::Firmware exceeds 500KB OTA limit (${SIZE_KB}KB)"
            exit 1
          elif [ $SIZE -gt 460800 ]; then
            echo "::warning::Firmware approaching 500KB OTA limit (${SIZE_KB}KB)"
          fi

      - name: Run native tests
        run: pio test -e native -v

      - name: Upload firmware artifact
        uses: actions/upload-artifact@v4
        with:
          name: firmware
          path: .pio/build/nodemcuv2/firmware.bin
          retention-days: 30
```

### 2.2 Release on Tag

Create `.github/workflows/release.yml`:

```yaml
name: Release

on:
  push:
    tags: ['v*']

jobs:
  release:
    runs-on: ubuntu-latest
    permissions:
      contents: write
    steps:
      - uses: actions/checkout@v4

      - name: Cache PlatformIO
        uses: actions/cache@v4
        with:
          path: |
            ~/.cache/pip
            ~/.platformio/.cache
            ~/.platformio/platforms
            ~/.platformio/packages
          key: ${{ runner.os }}-pio-${{ hashFiles('platformio.ini') }}

      - uses: actions/setup-python@v5
        with:
          python-version: '3.x'

      - name: Install PlatformIO
        run: pip install --upgrade platformio

      - name: Build firmware
        run: pio run -e nodemcuv2

      - name: Build LittleFS image
        run: pio run -t buildfs -e nodemcuv2

      - name: Run native tests
        run: pio test -e native -v

      - name: Get firmware size
        id: size
        run: |
          SIZE=$(stat -c%s .pio/build/nodemcuv2/firmware.bin)
          echo "size_kb=$((SIZE / 1024))" >> $GITHUB_OUTPUT

      - name: Create Release
        uses: softprops/action-gh-release@v2
        with:
          files: |
            .pio/build/nodemcuv2/firmware.bin
            .pio/build/nodemcuv2/littlefs.bin
          body: |
            ## Firmware Release ${{ github.ref_name }}

            | File | Purpose |
            |---|---|
            | `firmware.bin` | Main firmware (${{ steps.size.outputs.size_kb }}KB) |
            | `littlefs.bin` | LittleFS filesystem image |

            ### Flash via serial
            ```bash
            esptool.py --port /dev/ttyUSB0 write_flash 0x0 firmware.bin
            ```

            ### Flash via OTA
            ```bash
            espota.py -i <device-ip> -f firmware.bin
            ```
          draft: false
          prerelease: ${{ contains(github.ref_name, 'rc') }}
```

---

## 3. Self-Hosted LXD Runner

A self-hosted GitHub Actions runner inside an LXD container on the local network. This runner handles OTA deployments to physical ESP8266 devices (which are not reachable from GitHub's cloud).

### 3.1 Create the Runner Container

```bash
# Create a persistent LXD container for the runner
lxc launch ubuntu:24.04 gh-runner

# Install dependencies
lxc exec gh-runner -- bash -c "
  apt update && apt install -y \
    python3 python3-pip python3-venv \
    git curl jq

  # Install PlatformIO
  pip3 install platformio

  # Pre-download ESP8266 platform (cache warm-up)
  pio platform install espressif8266
"

# Ensure bridged networking (runner must reach ESP8266 devices)
# Default LXD bridge (lxdbr0) provides this.
# Verify: lxc exec gh-runner -- ping <esp8266-ip>
```

### 3.2 Register as GitHub Actions Runner

```bash
# Get a registration token from GitHub
# (replace OWNER/REPO with your actual repo)
TOKEN=$(gh api repos/OWNER/esp8266-led-controller/actions/runners/registration-token -q .token)

lxc exec gh-runner -- bash -c "
  mkdir -p /opt/actions-runner && cd /opt/actions-runner

  # Download latest runner (check https://github.com/actions/runner/releases)
  curl -o actions-runner.tar.gz -L \
    https://github.com/actions/runner/releases/download/v2.321.0/actions-runner-linux-x64-2.321.0.tar.gz
  tar xzf actions-runner.tar.gz
  rm actions-runner.tar.gz

  # Configure (non-interactive)
  ./config.sh --url https://github.com/OWNER/esp8266-led-controller \
    --token ${TOKEN} \
    --name lxd-ota-runner \
    --labels self-hosted,lxd,ota \
    --unattended

  # Install and start as a service
  ./svc.sh install
  ./svc.sh start
"
```

### 3.3 USB Passthrough (for serial flash)

Only needed if the runner should also flash via USB serial:

```bash
# Pass through USB-serial adapter
lxc config device add gh-runner ttyUSB0 unix-char path=/dev/ttyUSB0

# Add the runner user to the dialout group inside the container
lxc exec gh-runner -- usermod -aG dialout runner
```

### 3.4 Runner Maintenance

```bash
# Check runner status
lxc exec gh-runner -- bash -c "cd /opt/actions-runner && ./svc.sh status"

# Update runner version (when GitHub enforces minimum version)
lxc exec gh-runner -- bash -c "
  cd /opt/actions-runner
  ./svc.sh stop
  curl -o actions-runner.tar.gz -L <new-release-url>
  tar xzf actions-runner.tar.gz
  rm actions-runner.tar.gz
  ./svc.sh start
"

# Update PlatformIO and platform cache
lxc exec gh-runner -- bash -c "
  pip3 install --upgrade platformio
  pio platform update espressif8266
"
```

---

## 4. OTA Deployment Workflow

This workflow runs on the self-hosted LXD runner (which is on the same local network as the ESP8266 devices).

### 4.1 Deploy on Release

Create `.github/workflows/deploy-ota.yml`:

```yaml
name: OTA Deploy

on:
  workflow_dispatch:
    inputs:
      device_ip:
        description: 'ESP8266 device IP address'
        required: true
        type: string
      release_tag:
        description: 'Release tag to deploy (e.g. v0.1.0). Leave blank for latest.'
        required: false
        type: string

jobs:
  deploy:
    runs-on: [self-hosted, lxd, ota]
    steps:
      - name: Determine release tag
        id: tag
        run: |
          if [ -n "${{ inputs.release_tag }}" ]; then
            echo "tag=${{ inputs.release_tag }}" >> $GITHUB_OUTPUT
          else
            TAG=$(gh release view --repo ${{ github.repository }} --json tagName -q .tagName)
            echo "tag=${TAG}" >> $GITHUB_OUTPUT
          fi
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}

      - name: Download firmware from release
        run: |
          gh release download ${{ steps.tag.outputs.tag }} \
            --repo ${{ github.repository }} \
            --pattern "firmware.bin" \
            --dir ./deploy
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}

      - name: Verify firmware exists
        run: |
          ls -la ./deploy/firmware.bin
          SIZE=$(stat -c%s ./deploy/firmware.bin)
          echo "Firmware size: $((SIZE / 1024))KB"

      - name: Deploy via OTA
        run: |
          echo "Deploying to ${{ inputs.device_ip }}..."
          espota.py -i ${{ inputs.device_ip }} \
            -p 8266 \
            -f ./deploy/firmware.bin \
            --progress
        timeout-minutes: 5

      - name: Verify device rebooted
        run: |
          echo "Waiting for device to reboot..."
          sleep 10
          # Ping to verify device is back online
          for i in $(seq 1 12); do
            if ping -c 1 -W 2 ${{ inputs.device_ip }} > /dev/null 2>&1; then
              echo "Device is back online at ${{ inputs.device_ip }}"
              exit 0
            fi
            echo "Waiting... (attempt $i/12)"
            sleep 5
          done
          echo "::warning::Device did not respond to ping after 60 seconds"
```

### 4.2 Fleet Deploy (Multiple Devices)

Create `.github/workflows/deploy-fleet.yml`:

```yaml
name: Fleet OTA Deploy

on:
  workflow_dispatch:
    inputs:
      release_tag:
        description: 'Release tag to deploy'
        required: true
        type: string

jobs:
  deploy:
    runs-on: [self-hosted, lxd, ota]
    strategy:
      fail-fast: false
      matrix:
        device:
          - { name: "lounge", ip: "192.168.1.50" }
          - { name: "bedroom", ip: "192.168.1.51" }
          - { name: "kitchen", ip: "192.168.1.52" }
    steps:
      - name: Download firmware
        run: |
          mkdir -p ./deploy
          gh release download ${{ inputs.release_tag }} \
            --repo ${{ github.repository }} \
            --pattern "firmware.bin" \
            --dir ./deploy
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}

      - name: Deploy to ${{ matrix.device.name }}
        run: |
          echo "Deploying to ${{ matrix.device.name }} (${{ matrix.device.ip }})..."
          espota.py -i ${{ matrix.device.ip }} \
            -p 8266 \
            -f ./deploy/firmware.bin \
            --progress
        timeout-minutes: 5

      - name: Verify ${{ matrix.device.name }}
        run: |
          sleep 10
          ping -c 3 -W 2 ${{ matrix.device.ip }} && echo "✓ ${{ matrix.device.name }} online" || echo "⚠ ${{ matrix.device.name }} not responding"
```

---

## 5. PlatformIO Caching Strategy for LXD

### For the persistent self-hosted runner

The `gh-runner` container is persistent, so PlatformIO's cache survives between runs. No special action needed — `~/.platformio/` stays populated.

To manually warm the cache:

```bash
lxc exec gh-runner -- bash -c "
  cd /path/to/project
  pio platform install espressif8266
  pio pkg install
"
```

### For ephemeral runners (future scaling)

If you later switch to ephemeral LXD runners (one container per job), use a shared LXD storage volume:

```bash
# Create a shared volume for the PlatformIO cache
lxc storage volume create default pio-cache

# Attach to runner containers at launch
lxc config device add <runner-container> pio-cache disk \
  source=pio-cache \
  pool=default \
  path=/home/runner/.platformio
```

### For GitHub-hosted runners

Already handled in the workflow files above via `actions/cache@v4` with key `pio-${{ hashFiles('platformio.ini') }}`.

---

## 6. Workflow Summary

| Trigger | Runner | Actions |
|---|---|---|
| Push to `main` or `phase/*` | GitHub-hosted | Build firmware, check size, run native tests |
| Pull request to `main` | GitHub-hosted | Build firmware, check size, run native tests |
| Push tag `v*` | GitHub-hosted | Build, test, create GitHub Release with firmware.bin + littlefs.bin |
| Manual dispatch (single device) | Self-hosted LXD | Download release firmware, OTA deploy to one device |
| Manual dispatch (fleet) | Self-hosted LXD | Download release firmware, OTA deploy to all devices in parallel |

---

## 7. Required GitHub Secrets / Variables

| Secret | Purpose | Where used |
|---|---|---|
| `GITHUB_TOKEN` | Auto-provided by GitHub Actions | Release creation, artifact download |

No additional secrets are needed for the current setup. If MQTT-triggered OTA is added (Phase 3), the MQTT broker credentials would be added as secrets.

---

## 8. Device Inventory

Maintain a list of devices for fleet deployment in the `deploy-fleet.yml` matrix. Update when devices are added or moved:

```yaml
matrix:
  device:
    - { name: "lounge", ip: "192.168.1.50" }
    - { name: "bedroom", ip: "192.168.1.51" }
    # Add new devices here
```

For larger fleets, consider moving to a JSON inventory file and loading it dynamically.

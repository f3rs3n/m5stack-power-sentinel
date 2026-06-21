# Quickstart

This quickstart is for a First Public Release User: a technically capable homelab owner who can use Linux/SSH on the Module LLM, edit config files, and flash the CoreS3 with USB/PlatformIO.

Power Sentinel v1 has two installable units:

1. **Backend Installer** for the Module LLM/Linux side.
2. **Firmware Release Path** for the CoreS3 side.

Do not treat these as one mega-installer. The backend installer does not flash the CoreS3.

## 1. Confirm hardware and prerequisites

Read [hardware.md](hardware.md) first. Public v1 assumes:

- M5Stack CoreS3 display.
- M5Stack Module LLM stack with Linux access.
- Internal StackFlow/UART transport between CoreS3 and Module LLM.
- USB access to the CoreS3 for firmware flashing.
- PlatformIO available on the machine used to build/flash firmware.

Install Module LLM packages as appropriate for your Linux distribution. For Debian/Ubuntu-style systems, the baseline dependencies are:

```bash
sudo apt-get update
sudo apt-get install -y nut nut-server nut-client curl python3 python3-zmq
```

Optional diagnostics:

```bash
sudo apt-get install -y usbutils iproute2
```

## 2. Inspect the Backend Installer dry-run

See [backend-installer.md](backend-installer.md) for the full installer scope and recorded dry-run behavior.

From the repository root on the Module LLM/Linux host:

```bash
scripts/install-power-sentinel.sh --modules nut --dry-run
```

Expected behavior:

- prints the service and binary install operations it would run;
- prints the generated `/etc/power-sentinel.json` template;
- enables NUT Monitor and keeps Proxmox disabled;
- does not mutate the machine in dry-run mode.

Optional Proxmox module dry-run:

```bash
scripts/install-power-sentinel.sh --modules nut,proxmox --dry-run
```

Expected behavior:

- prints the same backend install operations;
- enables the Proxmox section in the generated config;
- reminds you to configure read-only Proxmox credentials;
- does not attempt live Proxmox API access while `token_secret` is the placeholder.

## 3. Install or update the backend

After reviewing dry-run output:

```bash
sudo scripts/install-power-sentinel.sh --modules nut
```

For NUT plus optional Proxmox:

```bash
sudo scripts/install-power-sentinel.sh --modules nut,proxmox
```

The Backend Installer owns Power Sentinel API/StackFlow service installation and `/etc/power-sentinel.json`. It does not own your `/etc/nut/` runtime configuration and does not flash firmware.

## 4. Configure modules

Edit the Configuration Surface:

```bash
sudo editor /etc/power-sentinel.json
```

See [configuration.md](configuration.md), [NUT Monitor](modules/nut.md), and [Proxmox Module](modules/proxmox.md).

## 5. Verify backend API and StackFlow-safe payload

On the Module LLM/Linux host:

```bash
systemctl is-active power-sentinel-api.service power-sentinel-stackflow-unit.service
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
```

For a one-shot summary without running the HTTP server manually:

```bash
python3 backend/bin/power-sentinel-api.py --summary
```

The payload should use `schema: power-sentinel.summary.v1` and expose enabled module pages.

## 6. Build and flash the CoreS3 firmware

Read [firmware-flashing.md](firmware-flashing.md). From the firmware directory:

```bash
cd firmware/core-s3-display
pio run -e m5stack-cores3
pio device list
pio run -e m5stack-cores3 -t upload --upload-port /dev/ttyACM0
```

Use `pio device list` to identify the CoreS3 serial port before upload. Adjust `--upload-port` for your host; on Linux this is commonly `/dev/ttyACM0`, and on Windows it is commonly a `COM` port.

## 7. Verify CoreS3 live data

After the backend is running and the CoreS3 is flashed, capture serial output from the repository root:

```bash
python3 tools/core_s3_serial_capture.py --port /dev/ttyACM0 --duration 15
```

Look for StackFlow summary success or recovery breadcrumbs rather than only a clean boot.

## 8. Known limitations

- No OTA firmware updates in v1.
- No Module LLM-mediated flashing in v1.
- No CoreS3 setup/options page in v1.
- No mutable config CLI or web UI in v1.
- No CoreS3 Wi-Fi/HTTP polling fallback; live summaries use StackFlow/UART.
- NUT/upsmon remains the shutdown authority.
- Proxmox integration is API-only and read-only.

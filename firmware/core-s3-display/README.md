# CoreS3 display firmware

This directory contains the CoreS3 firmware for Power Sentinel.

For the public v1 Firmware Release Path, see `../../docs/firmware-flashing.md`.

## Live firmware

Build the live firmware:

```bash
pio run -e m5stack-cores3
```

Flash the live firmware over USB:

```bash
pio run -e m5stack-cores3 -t upload --upload-port /dev/ttyACM0
```

Adjust `--upload-port` for your host.

## Fixture and development environments

`m5stack-cores3` is the production/live firmware. It fetches summaries through StackFlow over the internal UART.

`m5stack-cores3-ledcards-interface` defines `POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY=1` and embeds a static visual fixture. Use it only for layout checks.

`m5stack-cores3-als-probe` is a development-only ALS calibration probe.

## Transport

Default live transport is StackFlow over the internal stacked UART:

- CoreS3 RX=G18.
- CoreS3 TX=G17.
- `Serial2`.
- 115200 baud.
- `work_id: sentinel`, `action: summary`.

CoreS3 HTTP polling is out of scope for the public v1 baseline.

## Serial capture

From the repository root, use:

```bash
python3 tools/core_s3_serial_capture.py --port /dev/ttyACM0 --duration 15
```

The helper intentionally does not reset the CoreS3 by default.

## UI

The live firmware renders enabled Module Pages for NUT Monitor and Proxmox when the backend exposes them. Enabled unavailable/unconfigured pages stay visible with unavailable treatment and `--` values instead of fake telemetry.

Touch focus is presentation-only and does not trigger controls or remediation.

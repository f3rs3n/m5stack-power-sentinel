# CoreS3 display firmware

Clean baseline: NUT Monitor only.

## Build

```bash
platformio run -e m5stack-cores3-ledcards-interface
```

If `platformio` is not on PATH:

```bash
~/.platformio/penv/bin/platformio run -e m5stack-cores3-ledcards-interface
```

## Development ALS probe

`m5stack-cores3-als-probe` is a development-only calibration probe for the
CoreS3 integrated LTR553 ambient-light sensor. It does not change the main
display firmware behavior.

```bash
platformio run -e m5stack-cores3-als-probe -t upload
platformio device monitor -b 115200
```

Serial output is CSV-like and starts with:

```text
ms,als_raw
```

The default sample interval is 250 ms and can be changed with
`POWER_SENTINEL_ALS_PROBE_INTERVAL_MS`.

## Adaptive brightness

The main firmware has a compile-time gated CoreS3 LTR553 ALS path:

- `POWER_SENTINEL_ALS_SENSOR_ENABLED=1` initializes and samples the integrated ambient-light sensor.
- `POWER_SENTINEL_ADAPTIVE_BRIGHTNESS_ENABLED=1` applies the adaptive DIM/AWAKE curves by default after the 2026-06-09 passive and active CoreS3 checks.
- When adaptive brightness is enabled, the firmware samples ALS every 100 ms, uses EMA alpha 2/3, and keeps the calibrated buckets for stability/debug only. The DIM (`32,48,64,80,96`) and AWAKE (`80,112,144,176,208`) values are interpolation anchors, so ambient-light changes produce a continuous target brightness instead of 5 discrete output steps. A continuous slew limiter (`1` brightness point every `30 ms`) applies the target; the 900 ms fade remains for display mode changes.
- OFF remains OFF; ambient light never wakes the display in the current implementation. `POWER_SENTINEL_ALS_WAKE_ON_LIGHT` is reserved for a future test.

Initial bucket thresholds come from `docs/als-calibration-2026-06-09.md` and should be revalidated with daylight/current placement before enabling adaptive brightness by default.

## Transport

Default transport is StackFlow over the internal stacked UART:

- CoreS3 RX=G18
- CoreS3 TX=G17
- `Serial2`
- 115200 baud
- `work_id: sentinel`, `action: summary`

HTTP is kept as a development fallback when WiFi is configured in ignored local config.

## UI

`src/main.cpp` only fetches/parses NUT summary fields and drives `ledcards-interface-page.*`.

Legacy HOME/PVE/HA/M5S tab rendering is intentionally absent from this baseline. Reintroduce future pages as separate modules after their backend contracts are restored.

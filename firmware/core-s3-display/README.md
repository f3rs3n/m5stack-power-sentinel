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
- The current 2026-06-11 physical tuning is confirmed as working and acceptable for the installed desk/studio conditions: response latency feels right, and the slower PWM motion is acceptable for now.
- When adaptive brightness is enabled, the firmware samples ALS every 100 ms and uses EMA alpha 2/3. The DIM (`32,48,64,80,96`) and AWAKE (`80,112,144,176,208`) values are interpolation anchors, so ALS changes first produce an instantaneous continuous brightness target. Tiny target changes inside `POWER_SENTINEL_ALS_TARGET_DEADBAND=2` are ignored when accepting a new brightness target, except curve endpoints which are always eligible. Eligible targets are debounced asymmetrically (`300` ms brightening, `1000` ms darkening); once accepted, the target is applied directly (`POWER_SENTINEL_ALS_TARGET_FILTER_SHIFT=0`) and the gamma-derived non-linear PWM slew provides the visual smoothing (`1` point every 24/34/40/48 ms from high to low brightness, roughly 3.1s from max DIM to min DIM). The 900 ms fade remains only for display mode changes.
- OFF remains OFF; ambient light never wakes the display in the current implementation. `POWER_SENTINEL_ALS_WAKE_ON_LIGHT` is reserved for a future test.

Initial raw interpolation anchors come from `docs/als-calibration-2026-06-09.md` and should be revalidated with daylight/current placement before enabling adaptive brightness by default.

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

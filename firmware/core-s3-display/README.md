# CoreS3 display firmware

Clean baseline: live NUT Monitor plus Proxmox ambient page when the backend reports the Proxmox module as implemented/observed. Both pages use the shared Ledcards visual language, fixed top status row, touch focus, and the internal StackFlow/UART transport.

## Build

Live firmware:

```bash
pio run -e m5stack-cores3
```

Static visual fixture firmware:

```bash
pio run -e m5stack-cores3-ledcards-interface
```

`m5stack-cores3` is the production/live firmware. It does not embed the Ledcards fixture and fetches summaries through StackFlow over the internal UART. `m5stack-cores3-ledcards-interface` defines `POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY=1` and calls the internal visual fixture at boot; use it only for layout checks.

## Flash live firmware

On Linux hosts where the CoreS3 appears as `/dev/ttyACM0`:

```bash
/home/martino/.platformio/penv/bin/pio run -e m5stack-cores3 -t upload --upload-port /dev/ttyACM0
```

The committed `platformio.ini` still carries the Windows/DOOMTRAIN `COM4` default for local IDE convenience, so pass `--upload-port /dev/ttyACM0` from Hermes/Linux sessions.

After flashing live firmware, verify that the Module LLM is serving real data and that StackFlow is available:

```bash
curl -fsS 'http://192.168.2.202:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
ssh root@192.168.2.202 'systemctl is-active power-sentinel-api.service power-sentinel-stackflow-unit.service'
```

## Live serial capture

For non-interactive agent live tests, prefer the repo helper over `pio device monitor` because PlatformIO's monitor requires a TTY:

```bash
/home/martino/.platformio/penv/bin/python ../../tools/core_s3_serial_capture.py --port /dev/ttyACM0 --duration 15
```

The helper intentionally does not reset the CoreS3 by default. Use it after a stable baseline when checking boot markers, display policy logs, StackFlow summary status, or crash/backtrace evidence.

## Development ALS probe

`m5stack-cores3-als-probe` is a development-only calibration probe for the
CoreS3 integrated LTR553 ambient-light sensor. It does not change the main
display firmware behavior.

```bash
pio run -e m5stack-cores3-als-probe -t upload
pio device monitor -b 115200
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
- The current 2026-06-17 physical tuning is hardware-aware: adaptive brightness chooses a physical Backlight Level first, then encodes it as a center-band nominal CoreS3 brightness value before calling M5GFX.
- When adaptive brightness is enabled, the firmware samples ALS every 100 ms and uses EMA alpha 2/3. Raw breakpoints are `0,12,45,70,95,135`. The DIM Backlight Level curve is `20,20,21,21,22,22`; the AWAKE Backlight Level curve is `21,22,23,24,25,25`. Tiny target changes inside `POWER_SENTINEL_ALS_TARGET_DEADBAND=2` are ignored when accepting a new brightness target, except curve endpoints which are always eligible. Eligible targets are debounced asymmetrically (`300` ms brightening, `1000` ms darkening); once accepted, the target is applied directly (`POWER_SENTINEL_ALS_TARGET_FILTER_SHIFT=0`) and the hardware-aware slew moves one Backlight Level every 96/136/160/192 ms from high to low brightness. The 900 ms fade remains only for display mode changes.
- OFF remains OFF; ambient light never wakes the display in the current implementation. `POWER_SENTINEL_ALS_WAKE_ON_LIGHT` is reserved for a future test.

Initial raw interpolation anchors come from `docs/als-calibration-2026-06-09.md` and should be revalidated with daylight/current placement before enabling adaptive brightness by default.

## Transport

Default transport is StackFlow over the internal stacked UART:

- CoreS3 RX=G18
- CoreS3 TX=G17
- `Serial2`
- 115200 baud
- `work_id: sentinel`, `action: summary`

CoreS3 HTTP polling is out of scope for the current baseline; live data comes through StackFlow/UART only.

Boot logs print the active transport configuration. Runtime diagnostics classify StackFlow serial timeout, JSON parse, StackFlow error, and stale-response failures, preserve the last-good payload timing, log first OK/failure/recovery events, and avoid steady-state success spam.

## UI

`src/main.cpp` fetches/parses the live summary and drives `ledcards-interface-page.*` for the NUT page plus any enabled backend page in `pages`, currently NUT and Proxmox. Enabled unavailable/unconfigured module pages should remain visible with unavailable/dim treatment rather than disappearing.

`ledcards-graphics.h` is the small shared helper seam for module-neutral Ledcards rendering details: Ambient Card render text ownership, visual-class color mapping, state fill/text colors, and physical ring slot geometry. NUT and Proxmox page models remain responsible for module-specific condition semantics, card vocabulary, hero policy, and touch-focus policy.

Ambient Console motion is live: page transitions animate between enabled Pages, and NUT/Proxmox card promotions move through the physical bidirectional ring using non-clickable animation ghosts. Touch focus promotes a card for presentation only; it does not create controls or remediation actions.

Legacy HOME/PVE/HA/M5S tab rendering is intentionally absent from this baseline. Reintroduce future pages as separate modules after their backend contracts are restored.

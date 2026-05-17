# Power Sentinel current state

Last updated: 2026-05-17

This file exists to survive chat/context compaction. It records the hardware facts, firmware decisions, and verified paths that must not be rediscovered unless hardware/firmware changes.

## Hardware stack

Current assembled order, bottom to top:

```text
DIN BASE -> LLM MATE -> LLM MODULE -> CORE S3
```

No DIP switch changes were needed for the confirmed UART path.

## Power behavior

Observed behavior:

| Firmware / config | CoreS3 powered from stack | CoreS3 USB-C powers stack |
| --- | --- | --- |
| M5Stack stock/demo firmware | yes | yes |
| Power Sentinel firmware with `cfg.output_power = true` | no | yes |
| Power Sentinel firmware with `cfg.output_power = false` | yes | no |

Interpretation:

- M5Stack stock/demo firmware appears to manage the shared power rail bidirectionally/dynamically.
- M5Unified 0.1.x exposes this as an explicit external output enable bit via `cfg.output_power` -> `Power.setExtOutput()`.
- For Power Sentinel appliance mode, use the safe external-input mode:

```cpp
#define POWER_SENTINEL_STACK_POWER_OUT 0
```

This lets the CoreS3 boot when powered from LLM Mate/LLM Module/base, but the CoreS3 USB-C will not feed 5V back into the stack.

## CoreS3 firmware config file location

The local, ignored config file must be here:

```text
firmware/core-s3-display/include/power_sentinel_config.h
```

The template is:

```text
firmware/core-s3-display/include/power_sentinel_config.example.h
```

A file at this path is wrong and is not used by the firmware include path:

```text
firmware/core-s3-display/power_sentinel_config.h
```

## Confirmed internal UART path

The internal CoreS3 <-> LLM Module UART path is confirmed working:

```text
CoreS3 UART1 RX = G18
CoreS3 UART1 TX = G17
Baud            = 115200 8N1
LLM Module dev  = /dev/ttyS1
```

Probe result:

```text
/dev/ttyS1 RX PS1 PING <millis>
/dev/ttyS1 TX PS1 PONG <same-token>
```

`/dev/ttyS2` through `/dev/ttyS5` returned I/O errors during scan. `/dev/ttyS0` is the Linux console and must not be used.

## Current backend

The LLM Module backend is reachable at:

```text
http://192.168.2.202:8088/api/v1/summary
```

The shared schema is:

```text
power-sentinel.summary.v1
```

See:

```text
docs/architecture/api-contract-v1.md
docs/operations/power-sentinel-api.md
```

## Current firmware state

CoreS3 frontend:

```text
firmware/core-s3-display/
```

Current dependency strategy in `platformio.ini`:

```text
M5Unified latest via PlatformIO registry (verified: 0.2.15)
M5GFX     transitive dependency of M5Unified (verified: 0.2.21)
LVGL      ^9.0.0 via PlatformIO registry (verified: 9.5.0)
ArduinoJson ^7.0.4
```

Important LVGL/M5GFX integration detail:

- `src/main.cpp` must include the real external `<lvgl.h>` and must not include `<M5Unified.h>` in the same translation unit.
- M5GFX includes its own internal LVGL-compatible font shim at `lgfx/v1/lvgl.h`; if the same translation unit includes both M5Unified/M5GFX and external LVGL, symbols such as `lv_area_t`, `lv_color_format_t`, and `lv_font_glyph_format_t` collide.
- The firmware avoids that by isolating M5Unified/M5GFX calls in `src/m5_hal.cpp` and exposing a tiny wrapper declared by `src/m5_hal.h`.
- Use `-DLV_CONF_INCLUDE_SIMPLE` plus `-I include` for `include/lv_conf.h`; the earlier `LV_CONF_PATH` setup and local `include/lvgl/lvgl.h` shim are no longer needed.

The firmware currently has:

- LVGL tab UI for UPS, HA, PVE, M5, Offline.
- WiFi HTTP fetching as development/fallback transport.
- Optional UART probe mode:

```cpp
#define POWER_SENTINEL_UART_PROBE 1
#define POWER_SENTINEL_UART_RX_PIN 18
#define POWER_SENTINEL_UART_TX_PIN 17
#define POWER_SENTINEL_UART_BAUD 115200
```

For normal builds, keep probe disabled:

```cpp
#define POWER_SENTINEL_UART_PROBE 0
```

## Next implementation step

Replace UART probe with the real serial bridge.

Target protocol:

Request from CoreS3:

```text
PS1 GET summary
```

Response from LLM Module:

```text
PS1 OK <json-byte-length>
{...power-sentinel.summary.v1...}
```

Implementation split:

1. LLM Module service:
   - open `/dev/ttyS1` at 115200;
   - listen for `PS1 GET summary`;
   - fetch `http://127.0.0.1:8088/api/v1/summary`;
   - return length-prefixed JSON;
   - run under systemd.

2. CoreS3 firmware:
   - periodically request summary over UART;
   - parse `PS1 OK <len>` and JSON payload;
   - update existing LVGL state;
   - keep WiFi HTTP only as development/fallback.

## Related docs

- `docs/operations/core-s3-llm-uart-discovery.md` — detailed UART discovery and confirmed scan output.
- `docs/architecture/frontend-core-s3-decisions.md` — frontend framework and workflow decisions.
- `docs/architecture/api-contract-v1.md` — JSON contract.
- `docs/operations/power-sentinel-api.md` — backend API operations.
- `docs/operations/backend-ops.md` — backend service operations.
- `docs/plans/homelab-power-sentinel-plan-2026-05-15.md` — original project plan.

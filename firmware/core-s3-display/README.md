# CoreS3 Display Firmware

PlatformIO firmware for the M5Stack CoreS3 display used by Power Sentinel.

## Current UI

The default `platformio.ini` environment (`m5stack-cores3`) builds the normal five-tab live firmware. A separate `m5stack-cores3-ledcards-interface` environment defines `POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY=1`; despite the historical environment name, this now builds the fullscreen Ledcards Interface NUT/UPS page against the same live StackFlow/HTTP summary path. It keeps the sidebar/dashboard disabled so the CoreS3 can be checked as a dedicated Ledcards Interface appliance surface, but it no longer uses compile-time fake NUT values.

The Ledcards Interface live page maps `SummaryState` into `LedcardsInterfaceNutView` in `main.cpp` and renders through `ledcards-interface-page.cpp`:

- hero candidate priority: stale/unavailable -> NUT, low battery -> Battery, on battery -> TTE, high load -> Load, marginal input -> Input, otherwise Battery;
- hero swaps are rate-limited by a short cooldown and maintain a five-metric order stack (`Battery`, `TTE`, `Load`, `Input`, `NUT`) so the hero is not duplicated in mini-cards;
- missing or stale telemetry renders as `--`/gray, never as nominal demo data;
- charging is inferred from NUT status/status label and overrides battery wording with `CHARGING`.

The normal firmware UI renders a five-tab LVGL dashboard:

1. `HOME` - overall power and homelab status.
2. `NUT` - UPS telemetry, NUT service state, connected-client count, and optional connected-host list.
3. `PVE` - Proxmox node health, capacity, storage, ZFS/SMART, and active interfaces.
4. `HA` - Home Assistant, MQTT, Zigbee2MQTT, update count, and coordinator/device status.
5. `M5S` - M5Stack appliance health, StackFlow/API freshness, local resources, and LLM smoke state.

Navigation in normal mode uses the icon-only left sidebar. Top-level tabs switch vertically through the sidebar stack; horizontal gestures inside the content area belong to the current tab/card carousel.

## Data source

The firmware consumes the normalized `power-sentinel.summary.v1` payload through the local StackFlow path. Missing data must render as unknown/unavailable/stale and must never fall back to demo values.

## UI validation

Visible firmware UI changes must be mirrored in `assets/lvgl-spike/power-sentinel-dashboard-fixture.c` and rendered through the LVGL/MCP workflow documented in `assets/lvgl-spike/README.md`.

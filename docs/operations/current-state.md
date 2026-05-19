# Power Sentinel current state

Last updated: 2026-05-19

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
- M5Unified exposes this as an explicit external output enable bit via `cfg.output_power` -> `Power.setExtOutput()`.
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
CoreS3 RX = G18
CoreS3 TX = G17
Baud      = 115200 8N1
LLM side  = llm_sys owning /dev/ttyS1
```

Important: `/dev/ttyS1` is owned by the vendor `llm_sys` service. Power Sentinel no longer runs a parallel Linux serial bridge on that device. The CoreS3 still uses the physical stacked UART, but speaks official StackFlow JSON and lets `llm_sys` route `work_id: "sentinel"` to the custom unit.

`/dev/ttyS2` through `/dev/ttyS5` returned I/O errors during scan. `/dev/ttyS0` is the Linux console and must not be used.

## Current UPS / NUT state

The UPS is physically connected over USB OTG to the LLM Module and is detected as:

```text
Vendor/Product: 051d:0002
Vendor: American Power Conversion
Model: Back-UPS ES 850G2
Serial: 5B2350TD6030
NUT driver: usbhid-ups
NUT UPS name: homelab_ups@localhost
```

Current verified NUT service state:

```text
nut-server: enabled/active
nut-driver: static/active
nut-monitor: disabled/inactive
```

`nut-monitor` is intentionally disabled for now: the appliance exposes/serves UPS data but no real shutdown is armed. The accepted real shutdown strategy is Standard NUT (`upsmon` primary on the USB-attached LLM Module, `upsmon` secondary on Proxmox). Power Sentinel is only a dashboard/dry-run observer and must not orchestrate Proxmox shutdown through the Proxmox API.

Verified live UPS values after connection:

```text
ups.status: OL
battery.charge: 100
battery.runtime: 384 seconds
input.voltage: 223.0 V
ups.load: 38 %
```

The local API and StackFlow path now report `ups.available=true`, `status=OL`, and overall `severity=ok` from live `upsc` data rather than placeholder values.

Sanitized example configs are in:

```text
backend/config/nut.conf.example
backend/config/ups.conf.example
backend/config/upsd.conf.example
backend/config/upsd.users.standard-nut.example
backend/config/upsmon.primary.example
backend/config/upsmon.secondary.example
backend/config/nut-clients.example.json
```

## Current backend

The LLM Module backend is reachable at:

```text
http://192.168.2.202:8088/api/v1/summary
```

The shared schema is:

```text
power-sentinel.summary.v1
```

Current backend services:

```text
power-sentinel-api.service
power-sentinel-stackflow-unit.service
llm-sys.service
```

The StackFlow custom unit listens on:

```text
ipc:///tmp/rpc.sentinel
```

The custom unit calls the API using the non-recursive safe endpoint:

```text
http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1
```

See:

```text
docs/architecture/api-contract-v1.md
docs/operations/power-sentinel-api.md
docs/operations/backend-ops.md
```

Verified V1 backend state after Proxmox/network integration:

```text
proxmox.available: true
proxmox.severity: ok
proxmox.node: pve
proxmox.node_status: online
proxmox.zfs.status: ONLINE
proxmox.smart.status: OK
proxmox.vm.running_names: ["haos"]
proxmox.lxc.running_names: ["docker", "hermes"]
proxmox.shutdown_state: disarmed (compatibility/display only; real shutdown path is NUT secondary upsmon)
network.available: true
network.default_route: true
network.probe: tcp
network.target: 1.1.1.1:53
mqtt.available: true
homeassistant.status: unknown    # homeassistant/status is not retained on the current HA install
zigbee2mqtt.available: true
zigbee2mqtt.state: online
zigbee2mqtt.version: 2.10.1
zigbee2mqtt.coordinator.type: EmberZNet
zigbee2mqtt.coordinator.firmware: 7.4.5 [GA]
zigbee2mqtt.devices: total=29 interviewed=29 disabled=0
shutdown.strategy: standard-nut
shutdown.mode: dry-run
shutdown.real_shutdown_owner: upsmon
shutdown.primary_ready: true
shutdown.primary_monitor_active: false
shutdown.secondary_ready: false
shutdown.nut_clients[0].state: reachable_via_upsc
shutdown.nut_clients[0].package_installed: true
shutdown.nut_clients[0].reachable_via_upsc: true
shutdown.nut_clients[0].connected_as_upsmon: false
shutdown.nut_clients[0].armed: false
shutdown.would_shutdown: false
shutdown.reason: UPS online
problems: []
```

The Proxmox API token lives only in root-owned runtime config on the LLM Module:

```text
/etc/power-sentinel.json
mode: 600
owner: root:root
```

Do not print or commit the token secret. The token is read-only and the service has no shutdown action. Real shutdown is delegated to Standard NUT, not custom shutdown orchestration.

## Current firmware state

CoreS3 frontend:

```text
firmware/core-s3-display/
```

Current dependency strategy in `platformio.ini`:

```text
M5Unified latest via PlatformIO registry
M5GFX     transitive dependency of M5Unified
LVGL      ^9.0.0 via PlatformIO registry
ArduinoJson ^7.0.4
```

Important LVGL/M5GFX integration detail:

- `src/main.cpp` must include the real external `<lvgl.h>` and must not include `<M5Unified.h>` in the same translation unit.
- M5GFX includes its own internal LVGL-compatible font shim at `lgfx/v1/lvgl.h`; if the same translation unit includes both M5Unified/M5GFX and external LVGL, symbols such as `lv_area_t`, `lv_color_format_t`, and `lv_font_glyph_format_t` collide.
- The firmware avoids that by isolating M5Unified/M5GFX calls in `src/m5_hal.cpp` and exposing a tiny wrapper declared by `src/m5_hal.h`.
- Use `-DLV_CONF_INCLUDE_SIMPLE` plus `-I include` for `include/lv_conf.h`.
- RGB565 color order is handled explicitly in the LVGL flush callback with `lv_draw_sw_rgb565_swap(pxMap, w * h)` before writing to M5GFX with its swap flag disabled.

The firmware currently has:

- Five LVGL tabs: `HOME`, `NUT`, `PVE`, `HA`, `M5S`.
- V1b modern polish: dark theme, HOME hero card, card radius/shadow, structured metric rows, status pills, and percent bars.
- HOME severity badge text is uppercase (`OK`, `WARN`, `CRITICAL`).
- HOME `NET` comes from the backend `network` object, which checks the LLM Module Linux default route plus a short TCP probe to `1.1.1.1:53`; it is not inferred from Proxmox.
- HA tab now shows HA core reachability, MQTT, Zigbee2MQTT state/version, coordinator type/firmware, and Zigbee device totals from the MQTT-first Z2M backend summary. If `homeassistant/status` is unavailable because the birth topic is not retained, the UI says `HA birth topic not retained` instead of presenting this as a failure.
- NUT tab now shows Standard NUT shutdown dry-run state: strategy, owner `upsmon`, primary readiness, generic NUT client readiness state (`not_configured`, `reachable_via_upsc`, `connected_as_upsmon`, `armed`), and NUT low-battery thresholds.
- PVE tab consumes read-only Proxmox API data: node latency/status, CPU/RAM/storage, ZFS, SMART, VM/LXC running names and counts. CPU/RAM/storage bars are explicitly labelled after the first hardware flash showed that a single unlabeled bar was ambiguous; missing Proxmox CPU temperature is rendered as `Temp n/a`.
- M5S tab treats missing/not-run chat smoke as `n/a`, not `FAIL`; StackFlow/OpenAI health remain the primary live checks.
- No boot/demo/sample payload. Initial display state is explicit `boot`/`offline`/`waiting` until the first live StackFlow summary arrives.
- Internal UART StackFlow transport enabled by default:

```cpp
#define POWER_SENTINEL_TRANSPORT_SERIAL 1
#define POWER_SENTINEL_SERIAL_TIMEOUT_MS 3500UL
#define POWER_SENTINEL_SERIAL_MAX_JSON_BYTES 8192
#define POWER_SENTINEL_SERIAL_RETRIES 3
```

- Optional WiFi HTTP fallback/development transport:

```cpp
#define POWER_SENTINEL_HTTP_FALLBACK 1
#define POWER_SENTINEL_SUMMARY_URL "http://192.168.2.202:8088/api/v1/summary"
```

## Verified StackFlow path

Verified by the user from VSCode on Windows over COM4 after flashing the StackFlow firmware:

```text
Serial TX StackFlow sentinel.summary id=ps-3 attempt 1/3
Serial RX StackFlow: {"request_id":"ps-3","work_id":"sentinel","created":...,"object":"power-sentinel.summary.v1",...}
StackFlow summary OK: 843 bytes
```

The user reported 38 consecutive OK polls with no misses. This validates the full path:

```text
CoreS3
-> internal UART / M-Bus
-> llm_sys
-> ipc:///tmp/rpc.sentinel
-> power-sentinel-stackflow-unit
-> http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1
-> CoreS3 display
```

## Deprecated implementation removed from repo and deployed module

The old direct serial bridge (`power-sentinel-serial-bridge.py`, its systemd unit, tests, and PS1 probe helper) was removed from the repository after StackFlow was verified. The deployed LLM Module was also cleaned after a stale active `power-sentinel-serial-bridge.service` was found still running and contending for `/dev/ttyS1`.

The stale deployed bridge symptom on the CoreS3 was:

```text
Serial RX StackFlow: PS1 ERR unknown_command 0
StackFlow JSON parse error: InvalidInput
```

The fix was:

```bash
systemctl disable --now power-sentinel-serial-bridge.service
rm -f /etc/systemd/system/power-sentinel-serial-bridge.service \
      /etc/systemd/system/multi-user.target.wants/power-sentinel-serial-bridge.service \
      /usr/local/bin/power-sentinel-serial-bridge
systemctl daemon-reload
systemctl restart llm-sys
systemctl restart power-sentinel-stackflow-unit
```

After cleanup, only `llm_sys` owns `/dev/ttyS1`, and CoreS3 polling returned online with increasing poll OK counts. The choice is deliberate: preserving `llm_sys` avoids UART contention and keeps the vendor StackFlow/assistant path available.

## Related docs

- `docs/operations/core-s3-llm-uart-discovery.md` — detailed UART discovery and confirmed scan output.
- `docs/architecture/frontend-core-s3-decisions.md` — frontend framework and workflow decisions.
- `docs/architecture/api-contract-v1.md` — JSON contract.
- `docs/operations/power-sentinel-api.md` — backend API operations.
- `docs/operations/backend-ops.md` — backend service operations.
- `docs/plans/homelab-power-sentinel-plan-2026-05-15.md` — original project plan.

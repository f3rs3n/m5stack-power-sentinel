# Power Sentinel current state

Last updated: 2026-05-23

This file exists to survive chat/context compaction. It records the hardware facts, firmware decisions, verified paths, and remaining gaps that must not be rediscovered unless hardware/firmware changes.

Current product goal:

```text
Turn the stack into an autonomous multi-function Linux server with:
- NUT server for the attached UPS;
- modern practical CoreS3 dashboards for UPS/NUT, Proxmox, Home Assistant/Zigbee2MQTT, network, and the M5Stack itself;
- an extensible mini-dashboard model;
- future local LLM inference for dashboard enrichment or a companion tab;
- MQTT exposure of stack sensors/health to Home Assistant.
```

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

Current verified NUT service state after the controlled primary/secondary upsmon test and safety disable:

```text
nut-server: enabled/active
nut-driver: static/active
nut-monitor: disabled/inactive on the M5Stack primary
Proxmox nut-monitor: disabled/inactive on the first secondary host
```

`nut-monitor` was temporarily enabled on the M5Stack primary and then on the Proxmox secondary to prove the Standard NUT path. Both monitors have since been explicitly stopped and disabled for the current staged/safe state. Shutdown is handled only by Standard NUT (`upsmon` primary on the USB-attached LLM Module, `upsmon` secondary on clients such as Proxmox) when the monitors are deliberately re-enabled. Power Sentinel is only a dashboard/readiness observer and must not orchestrate Proxmox shutdown through the Proxmox API.

Real NUT monitor users and `upsmon.conf` files have been prepared on the M5Stack and first secondary host. The controlled test verified that the M5Stack can run as `upsmon` primary and that Proxmox can connect as `upsmon` secondary; after the test, both `nut-monitor` services were disabled/inactive. The M5Stack currently runs NUT 2.7.4, so the deployed config uses legacy `master`/`slave` keywords for the Standard NUT primary/secondary roles.

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
proxmox.vm.running_names: ["haos"] plus optional running_items mini-metrics
proxmox.lxc.running_names: ["docker", "hermes"] plus optional running_items mini-metrics
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
shutdown.real_shutdown_owner: upsmon
shutdown.primary_ready: true
shutdown.primary_monitor_active: false
shutdown.armed: false
shutdown.secondary_ready: false
shutdown.nut_clients[0].state: reachable_via_upsc
shutdown.nut_clients[0].package_installed: true
shutdown.nut_clients[0].reachable_via_upsc: true
shutdown.nut_clients[0].configured: true
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

Current Proxmox token/role details that matter for future sessions:

```text
Proxmox role: PowerSentinelReadOnly
Privileges: Sys.Audit VM.Audit Datastore.Audit VM.GuestAgent.Audit
Token: power-sentinel@pve!readonly, privilege-separated
ACL scope: / with propagate=1 for both the owning user and token
```

`VM.GuestAgent.Audit` is intentionally included so the backend can read `qemu/{vmid}/agent/get-fsinfo` for VM filesystem usage. For HAOS, Proxmox list data reports `disk=0` even though the VM has a 64G disk, and `/status/current` is needed for accurate RAM. The backend therefore enriches running VM data with `/status/current`, then reads guest-agent fsinfo when available. HAOS exposes `/` as a tiny read-only `erofs` filesystem at 100%; this is ignored so displayed HDD usage comes from `/mnt/data` instead. Verified live HAOS mini-metrics after this fix:

```text
ram_percent: 90
ram_total_bytes: 4294967296
disk_percent: 15
disk_total_bytes: 64121331712
```

Global SSH aliases available in Martino's Hermes environment:

```text
ssh pve        # Proxmox node, root@192.168.2.99
ssh m5stack    # M5Stack LLM Module, root@192.168.2.202
ssh doomtrain  # Windows workstation, marti@192.168.2.199
```

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
- V1b/V1c/V1d modern polish: dark theme, compact left sidebar navigation with small icon/spia-style labels (`HM`, `NT`, `PV`, `HA`, `M5`) and vertical swipe between tabs, fixed-height horizontal card carousel per tab, HOME hero/local-control cards, card radius/shadow, structured metric rows, status pills, percent bars, and `SLEEP DISPLAY`.
- The LVGL MCP Windows spike is validated and kept under `assets/lvgl-spike/`: `run-lvgl-mcp-render.mjs` drives `lvgl-mcp-server` over stdio on the Windows host, and curated 320x240 visual baselines live in `assets/lvgl-spike/results/` as PNG plus widget-tree JSON.
- The MCP render caught a real HOME overflow/clipping issue; the current HOME layout keeps the first card dense and moves local controls/problems/sleep to a second horizontal card so vertical scrolling no longer becomes the primary way to traverse a tab.
- HOME severity badge text is uppercase (`OK`, `WARN`, `CRITICAL`).
- HOME `NET` comes from the backend `network` object, which checks the LLM Module Linux default route plus a short TCP probe to `1.1.1.1:53`; it is not inferred from Proxmox.
- HA tab now shows HA core reachability, MQTT, update count, Zigbee2MQTT state, coordinator type/firmware, and `Z2M devices: interviewed/total` from the MQTT-first Z2M backend summary. It deliberately does not show the HA birth-topic retained/debug state or the installed Z2M version.
- NUT tab now shows NUT shutdown readiness: owner `upsmon`, primary readiness, generic NUT client readiness state (`not_configured`, `reachable_via_upsc`, `connected_as_upsmon`, `armed`), and NUT low-battery thresholds.
- PVE tab consumes read-only Proxmox API data: CPU/RAM/storage, ZFS, SMART, VM/LXC running names/counts and optional per-running-workload mini-metrics. The main PVE card no longer shows the old `node / api` latency row; API timing moved to the M5S transport/debug card. CPU/RAM/storage bars are explicitly labelled, main RAM and workload RAM/HDD totals are right-aligned, workload mini-cards show CPU/RAM/HDD bars with totals where meaningful, and LXC workloads are labelled as `LXC` rather than `CT`. When no per-workload mini-metrics are available, the old full fallback card is replaced by a half-height info mini-card. The old combined temp/storage row was removed because Proxmox CPU temperature is unavailable here and storage is already shown as its own labelled bar.
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

## Deprecated implementation status

The previous parallel serial-bridge implementation is no longer part of the active architecture. The deployed LLM Module currently has no parallel bridge service or executable; `llm_sys` is the sole owner of `/dev/ttyS1`, and Power Sentinel integrates through the StackFlow custom unit.

This choice is deliberate: preserving `llm_sys` avoids UART contention and keeps the vendor StackFlow/assistant path available.

## Remaining gaps / next capabilities

Not yet implemented:

- Standard NUT shutdown is staged, not armed. `nut-monitor` remains disabled/inactive on the M5Stack primary and first Proxmox secondary until a deliberate arming step.
- Only the first Proxmox secondary has been prepared/verified as a NUT client; broader client inventory is future work.
- The CoreS3 has current mini-dashboards for HOME, NUT, PVE, HA, and M5S. Additional LAN mini-dashboards are future extensions.
- Local LLM inference is not used yet to enrich dashboard summaries or provide a companion tab. Current LLM use is baseline health/OpenAI availability and optional chat smoke.
- OTA/agent-driven flashing is not implemented; Windows + VSCode + PlatformIO remains the normal flash workflow.

## Related docs

- `docs/operations/core-s3-llm-uart-discovery.md` — detailed UART discovery and confirmed scan output.
- `docs/architecture/frontend-core-s3-decisions.md` — frontend framework and workflow decisions.
- `docs/architecture/api-contract-v1.md` — JSON contract.
- `docs/operations/power-sentinel-api.md` — backend API operations.
- `docs/operations/backend-ops.md` — backend service operations.
- `docs/plans/homelab-power-sentinel-plan-2026-05-15.md` — original project plan.

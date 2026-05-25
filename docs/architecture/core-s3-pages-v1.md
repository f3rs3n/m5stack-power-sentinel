# CoreS3 Pages V1 Specification

Date: 2026-05-18
Status: V1 implemented; this file is now the UI contract and regression reference, not just a future design sketch.
Device: M5Stack CoreS3 + LLM Kit Power Sentinel

## Goal

Define and preserve the V1 CoreS3 information architecture for the broader Power Sentinel goal: an autonomous multi-function Linux server with a modern, captivating, practical display for LAN state.

The CoreS3 is a desk-visible physical homelab sentinel. It should answer, quickly and honestly:

1. Is power OK?
2. How much runtime remains?
3. Is the NUT server/client readiness healthy?
4. Are Proxmox, Home Assistant, MQTT/Zigbee2MQTT, network, and the M5Stack appliance healthy?
5. If something is wrong, which subsystem is the cause?
6. If the display itself looks stale, is StackFlow transport still working?

No page may show plausible demo/sample values after live transport fails. Missing data must render as `unknown`, `unavailable`, or an explicit stale/offline state.

Future scope: the same tab/card model should support dedicated mini-dashboards and, later, local LLM inference that enriches dashboard summaries or appears as a companion tab. That inference layer is not implemented in V1.

## Page set

V1 has five top-level tabs:

1. `HOME`
2. `NUT`
3. `PVE`
4. `HA`
5. `M5S`

Navigation:

- `HOME` is the default boot page.
- Top-level navigation uses a compact left sidebar to preserve the scarce 240 px vertical space.
- Sidebar navigation uses icon-only 18 px glyphs in a fixed 44 px left rail. Text suffixes such as `HM`, `NT`, `PV`, `HA`, and `M5` are no longer part of the normal live UI; the selected tab is identified by the blue pill/background while inactive icons remain bright/readable.
- Top-level tab switching is via the sidebar or vertical swipe through the left-sidebar tab stack; horizontal gestures inside the content area belong to the current tab's card carousel, not to changing tabs.
- No automatic tab switch in V1.
- No automatic return-to-HOME timer in V1.
- No audible/beep alert in V1; visual alerts only.
- For V1d, each tab lays out its cards horizontally as a swipeable carousel. This avoids a long vertical page that traps the user before they can move to another top-level tab.
- Individual cards may retain their own short vertical scroll only when content overflows the fixed 320x240 card viewport.
- A later V2 may promote dense sections into tap-to-open subpages with a back affordance.

Visual direction:

- Modern, clean, desk-readable dashboard.
- Dark background, high contrast text, restrained accent colors.
- Favor compact cards, clear status pills, and simple bars over decorative assets.

## Severity and colors

Global severity values:

- `ok`
- `warn`
- `critical`
- `offline` / `stale` display state when the CoreS3 has no live summary

Color mapping:

- OK: green accent
- WARN: amber/orange accent
- CRITICAL: red accent
- OFFLINE/STALE/UNAVAILABLE: grey or amber depending on whether stale data exists
- System/debug/informational accents: cyan/blue

V1 severity policy:

Critical:

- NUT reports `LB` / low battery.
- Estimated runtime <= 180 seconds.
- Battery <= 15%, unless NUT/runtime clearly indicates a better state.
- PVE API/node down.
- ZFS degraded/faulted/error state.
- SMART failing or disk health not OK according to PVE API data.
- HA API down.
- MQTT broker down.

Warn:

- NUT reports on-battery (`OB`) but not low-battery.
- Runtime <= 10 minutes and > 3 minutes.
- Battery <= 50% and > 15%.
- Zigbee2MQTT add-on/bridge down.
- Zigbee coordinator down.
- CPU temperature warning thresholds crossed.
- OpenAI API / chat smoke failure on the M5Stack.
- Short stale data / transient transport failures.

Temperature thresholds:

- PVE CPU temperature WARN: >= 70 C
- PVE CPU temperature CRITICAL: >= 80 C

Notes:

- `OpenAI FAIL` is WARN, not CRITICAL. It does not break the primary power sentinel function.
- VM/LXC counts are informational in V1 and do not affect global severity until an expected-state model is explicitly introduced.
- Z2M/coordinator are WARN in V1, not CRITICAL.

## HOME tab

Purpose: dense desk overview.

HOME answers: “Is the homelab currently OK, and if not, where should I look?”

Required visible elements:

- Header: `POWER SENTINEL` or compact title.
- Global severity pill: `OK`, `WARN`, `CRITICAL`, `OFFLINE`, or `STALE`.
- Main power state:
  - `GRID ONLINE`
  - `ON BATTERY`
  - `LOW BATTERY`
  - `UPS UNAVAILABLE`
  - `STALE DATA`
- UPS essentials:
  - battery percent
  - runtime, min/sec
  - load percent
  - input voltage
- Subsystem status row:
  - `NUT ONLINE/OFFLINE/WARN`
  - `PVE ONLINE/OFFLINE`
  - `HA ONLINE/OFFLINE/WARN`
  - `NET OK/DOWN/UNK`
  - `M5S ONLINE/OFFLINE/WARN`
- Problems summary:
  - show max 2-3 human-readable problems.
  - if none: `No active problems`.

HOME must not show:

- VM/LXC counts.
- Long IPs, tokens, model IDs, schemas, serial numbers, or driver versions.
- Transport poll counters; those belong on M5S.
- Architecture commentary or explanatory slogans; HOME should stay operational.

Example HOME layout:

```text
POWER SENTINEL       OK
GRID ONLINE

Batt 100%     Run 6m24s
Load 38%      In 226V

NUT ONLINE   PVE ONLINE   HA ONLINE
NET OK       M5S ONLINE

No active problems
```

Network status in HOME:

- V1 may show `NET OK/DOWN/UNK` if a simple gateway/router reachability probe is available.
- If no reliable probe exists yet, render `NET UNK` rather than inventing state.
- Router/Ubiquiti is not otherwise modeled as a first-class page in V1.

## NUT tab

Purpose: UPS + NUT server operational view.

The NUT tab is primarily an observer/readiness UI. It may show whether the LLM Module primary `upsmon` and configured downstream NUT clients appear ready/armed, but V1 must not expose a normal arming/disarming button. Downstream clients such as Proxmox own their own `upsmon` service state; Power Sentinel only reports it. If a future maintenance control is added, it must follow `docs/operations/standard-nut-arming-runbook.md`: local LLM Module `upsmon` hold/release only, feature-flagged/admin-only, finite TTL, audited, explicit confirmation, no downstream client control, no FSD, and no custom shutdown orchestration.

The `NUT` tab combines horizontal cards for V1d:

1. UPS essentials
2. NUT server / clients

If this becomes too dense, V2 may split into separate `UPS` and `NUT` pages or use tap-to-open subpages.

### NUT: UPS essentials section

Always visible near the top:

- UPS state label: `Online`, `On battery`, `Low battery`, `Unavailable`, `Stale`.
- Raw NUT status: e.g. `OL`, `OB`, `LB`, `CHRG`, `DISCHRG`, `RB` if present.
- Battery percent with large bar.
- Runtime in min/sec as a prominent number.
- Load percent with smaller bar.
- Estimated load watts, if `ups.realpower.nominal` is known:
  - `load_w = realpower_nominal_w * load_percent / 100`
- Input voltage.
- Data age/stale indicator.

Secondary UPS details, on a separate horizontally reachable card:

- output voltage, if NUT exposes it.
- battery voltage.
- nominal real power W.
- transfer reason.
- beeper status.
- UPS model.
- NUT driver name.

Do not show on the normal top section:

- UPS serial number.
- UPS firmware version.
- NUT driver version.
- battery manufacture date.

These may appear only in a future debug/details section if useful.

Current known UPS data from APC Back-UPS ES 850G2:

- model: `Back-UPS ES 850G2`
- driver: `usbhid-ups`
- status: `OL`
- battery percent: available
- runtime seconds: available
- load percent: available
- input voltage: available
- battery voltage: available
- nominal real power: available (`ups.realpower.nominal`)
- beeper status: available
- transfer reason: available
- output voltage: currently may be unavailable

Example NUT top section:

```text
NUT                 ONLINE

Battery
[##########] 100%

Runtime 6m24s
Load [####------] 38%  ~198W

Input 226V
Status OL   Age 0s
```

### NUT: server / clients section

V1 should use NUT-observable client data only. Do not add custom log parsing, custom upsmon config introspection, or bespoke client health agents for V1.

Display fields:

- `nut-server`: OK/DOWN
- `nut-driver`: OK/DOWN
- `upsd` listener: localhost/LAN status if available
- client count, if obtainable from NUT/upsd tools in a low-complexity way
- connected client names/hosts, if obtainable from NUT/upsd tools in a low-complexity way

Known physical UPS loads:

- Proxmox mini-PC
- main PC
- Ubiquiti router

Do not hard-code these as `OK` clients unless NUT actually exposes them as connected clients. Static physical load labels may be used only as low-priority informational text if dynamic client discovery is not available, but the preferred V1 behavior is NUT client-derived data.

Shutdown policy:

- Real shutdown path is Standard NUT, not custom shutdown orchestration.
- `nut-monitor` remains disabled until Standard NUT primary/secondary config is deliberately armed.
- Power Sentinel is dashboard/readiness observer only.
- NUT page should show NUT shutdown readiness, owner `upsmon`, primary/secondary readiness, client inventory state, and low-battery thresholds.

## PVE tab

Purpose: single-node Proxmox host and workload health.

Scope:

- One Proxmox node only.
- No cluster model in V1.
- Read-only API integration.
- No shutdown actions in V1.
- Shutdown policy display points to Standard NUT / `upsmon`; do not imply Proxmox API shutdown control.

Security/config direction:

- Use a dedicated Proxmox API token with read-only permissions.
- Store token/config in a root-only file on the LLM Module, e.g. `/etc/power-sentinel.json`.
- Commit only sanitized config templates.
- The M5Stack backend talks directly to the Proxmox API over HTTPS.

Top PVE section:

- Header title is `PROXMOX` with a right-aligned status pill using `ONLINE` / `OFFLINE`, not `PVE OK` / `PVE DOWN`.
- node/API diagnostics are intentionally not in the compact PVE card; API latency moved to the M5S transport/debug card
- CPU usage % plus percent bar, labelled simply `CPU N%`.
- RAM usage % plus percent bar, labelled simply `RAM N%`, with total RAM right-aligned when available.
- CPU temperature is intentionally omitted from the compact V1 PVE card until the backend can expose a meaningful host/sensor-specific value; do not show a permanent `Temp n/a` placeholder.
- storage usage % plus percent bar, labelled simply `Storage N%`, with Total Node Capacity right-aligned when available. Total Node Capacity is `proxmox.storage_total_bytes`, aggregated from `/nodes/{node}/storage` enabled/active storages, not the node rootfs total except as fallback.
- ZFS status as a compact status pill, rendered as `ZFS online` / `ZFS warn`.
- SMART/disk health status as a compact status pill, rendered as `SMART ok` / `SMART warn`.
- active non-loopback Proxmox interfaces from `proxmox.active_network_interfaces[]` render as compact pills such as `eth25g` or `vmbr0`.
- PVE NUT readiness renders as `NUT armed` / `NUT disarmed` and must refer only to the configured Proxmox NUT client (`shutdown.proxmox_nut_client.armed`), not aggregate readiness of any secondary client.

Do not show Proxmox API shutdown triggers in V1. Avoid implying an armed shutdown policy before Standard NUT readiness is understood.

VM/LXC section:

- Use horizontally reachable mini-card pages in the PVE carousel.
- When Proxmox list-endpoint metrics are available, render one mini-card per running VM/LXC.
- Mini-cards are half-height so two fit cleanly in the 320x240 content viewport.
- Each mini-card shows three compact bars: CPU %, RAM % with total RAM on the right, and HDD/disk % with total disk on the right.
- For VMs, RAM should come from `/qemu/{vmid}/status/current` when available; the Proxmox list endpoint can over-report HAOS memory.
- For VMs, disk usage should come from QEMU guest-agent `get-fsinfo` when available. Do not display a Proxmox VM `disk=0` as `HDD 0%`; render unknown unless fsinfo provides real filesystem usage. HAOS exposes `/` as a small read-only `erofs` filesystem at 100%; ignore read-only/pseudo filesystems and use the data filesystem such as `/mnt/data`.
- If no per-workload mini-metrics are available, do not render the old full `Running workloads` fallback card. Render a single half-height info mini-card instead: `No running VM/LXC` when the node has no active workloads, or `Workload metrics unavailable` when counts exist but detailed metrics are absent.
- Do not list stopped workloads in V1.
- Do not distinguish critical vs non-critical workloads in V1.
- VM/LXC stopped/running counts are informational only and do not affect severity until an explicit expected-state config exists.

ZFS section:

- pool name(s)
- pool health: `ONLINE`, `DEGRADED`, `FAULTED`, etc.
- capacity %
- scrub state / last scrub age if available via API
- error count if available

SMART/disk section:

- Use PVE API first.
- V1 should not require SSH or custom `smartctl` scripts on the Proxmox host.
- Show health summary and failing/warning count.
- Disk names are not essential on the CoreS3 unless there is a problem.

Example PVE layout:

```text
PROXMOX            ONLINE

CPU 18%
[percent bar]
RAM 46%        32GB
[percent bar]
Storage 8%     7.2TB
[percent bar]

[ZFS online] [SMART ok]
[eth25g] [vmbr0]
[NUT disarmed]

-- horizontal mini-card page --
VM haos
CPU 4%          [bar]
RAM 42%     4GB [bar]
HDD 15%    60GB [bar]

LXC docker
CPU 9%          [bar]
RAM 61%     8GB [bar]
HDD 55%    64GB [bar]
```

## HA tab

Purpose: Home Assistant, MQTT, Zigbee2MQTT, and coordinator health.

HA OK in V1 requires:

- Home Assistant API reachable.
- MQTT broker reachable.

Zigbee2MQTT and coordinator are shown separately and currently map to WARN, not CRITICAL.

Data source priority:

1. Home Assistant API for HA reachability and entities.
2. MQTT broker reachability.
3. Zigbee2MQTT MQTT bridge topics:
   - `zigbee2mqtt/bridge/state`
   - `zigbee2mqtt/bridge/info`
4. Supervisor API only if MQTT/entity checks are insufficient.

Known HA/Zigbee context:

- Home Assistant is supervised.
- Zigbee2MQTT runs as a Home Assistant add-on.
- Zigbee2MQTT topic base: `zigbee2mqtt`.
- Coordinator: Sonoff Dongle-M via Ethernet.

Visible fields:

- `HA API OK/DOWN`
- `MQTT OK/DOWN`
- `Z2M OK/DOWN/UNK`
- `Coordinator OK/DOWN/UNK`
- `Updates N`, where unavailable update data renders as `Updates 0` rather than `n/a`
- `Z2M devices: interviewed/total`
- HA API latency if available
- MQTT last publish age if available
- last error/problem summary

Do not duplicate the full Home Assistant UI.
Keep text operational: status, latency, age, and problem summaries only.

Example HA layout:

```text
HA                  OK

API OK       MQTT OK
Updates 0

Z2M OK       COORD OK
Z2M devices: 29/29

Problem: none
```

Severity mapping:

- HA API down: CRITICAL.
- MQTT broker down: CRITICAL.
- Z2M down: WARN in V1.
- Coordinator down: WARN in V1.

## M5S tab

Purpose: local M5Stack appliance health + transport/debug.

This tab replaces separate `M5` and `Transport` debug tabs for the final appliance UI.

Visible fields:

M5Stack Linux:

- module temperature
- RAM available or RAM usage
- disk free
- uptime if available
- core services:
  - `llm-sys`
  - `power-sentinel-api`
  - `power-sentinel-stackflow-unit`
  - `nut-server`
- StackFlow API OK/FAIL
- OpenAI API OK/FAIL
- chat smoke OK/FAIL/last run if available

Transport:

- source:
  - `stackflow`
  - `http`
  - `boot`
  - `stale`
- poll OK count
- poll fail count
- last fetch duration ms
- last good age
- last request ID if useful
- firmware build
- backend schema
- UART pins/baud:
  - CoreS3 RX G18 / TX G17 @ 115200

Example M5S layout:

```text
M5S                 OK

Temp 42C     RAM 695MB
Disk 20.0GB  Uptime 1d

llm-sys OK   API OK
StackFlow OK NUT OK
OpenAI WARN  Chat FAIL

Transport stackflow
Poll 153 ok / 2 fail
Last 84ms  Age 1s
FW stackflow-2026-05-23-pve-polish
Schema summary.v1
UART 18/17 115200
```

OpenAI/chat smoke failures should surface here and may contribute WARN, but not CRITICAL.

## Alert/offline behavior

V1 visual-only alert behavior:

- No beep/audio.
- No auto-switch to HOME/NUT/PVE.
- No forced modal alerts.
- Current page may show a global sidebar/badge/accent reflecting global severity.

Offline/stale behavior:

- If CoreS3 has no valid live summary, show explicit stale/offline state.
- Keep the last good values only if clearly marked stale and aged.
- Never synthesize plausible replacement values.
- M5S must show poll fail count and last good age.

## API contract extension direction

Keep `power-sentinel.summary.v1` and add optional fields. The firmware must tolerate missing fields and render `unknown`/`unavailable`.

Implementation status as of 2026-05-23:

- NUT/UPS fields are implemented from live `upsc` data.
- NUT server/driver/shutdown-readiness fields are implemented; real shutdown remains Standard NUT only.
- Network status is implemented from the LLM Module Linux default route plus TCP probe, not from Proxmox reachability.
- Proxmox read-only fields are implemented for one node via API token stored only in root-owned runtime config. VM RAM is enriched from `/status/current`; VM disk usage is enriched from QEMU guest-agent fsinfo with read-only/pseudo filesystems filtered out.
- HA/MQTT/Zigbee2MQTT read-only fields are implemented with HA TCP/API reachability, optional HA update counts, and MQTT bridge topics.
- M5Stack local health fields are implemented from healthcheck/service state.
- Local LLM inference for enriched text or a companion tab is not implemented yet.

Remaining contract direction: future additions should continue as optional fields or new optional objects so old firmware renders missing data honestly.

### Proposed optional `ups` fields

```json
{
  "model": "Back-UPS ES 850G2",
  "status": "OL",
  "status_label": "Online",
  "battery_percent": 100,
  "runtime_seconds": 384,
  "load_percent": 38,
  "input_voltage": 226.0,
  "output_voltage": null,
  "battery_voltage": 13.6,
  "realpower_nominal_w": 520,
  "load_w": 198,
  "beeper_status": "disabled",
  "transfer_reason": "input voltage out of range",
  "driver": "usbhid-ups",
  "nut_server_state": "active",
  "age_seconds": 0,
  "stale": false
}
```

### Proposed optional `nut` object

```json
{
  "server_active": true,
  "driver_active": true,
  "monitor_active": false,
  "mode": "netserver",
  "client_count": null,
  "clients": [],
  "shutdown_state": "disarmed"
}
```

`clients` should be populated only from low-complexity NUT/upsd-visible data. Do not add custom client agents for V1.

### Proposed optional `network` object

```json
{
  "available": true,
  "gateway_ok": true,
  "router_ok": true,
  "latency_ms": 2.1,
  "severity": "ok"
}
```

Render as `NET OK/DOWN/UNK` on HOME. If not implemented, use `UNK`.

### Proposed optional `proxmox` fields

```json
{
  "available": true,
  "severity": "ok",
  "node_name": "pve-mini",
  "api_latency_ms": 25,
  "node_online": true,
  "cpu_usage_percent": 18,
  "ram_usage_percent": 46,
  "cpu_temperature_c": 58,
  "storage_usage_percent": 62,
  "vms_running": 8,
  "lxcs_running": 4,
  "running_vms": ["ha", "docker"],
  "running_lxcs": ["hermes", "services"],
  "zfs": {
    "status": "ONLINE",
    "pools": [
      {"name": "rpool", "status": "ONLINE", "capacity_percent": 62, "errors": 0}
    ]
  },
  "smart": {
    "status": "OK",
    "failing_count": 0,
    "warning_count": 0,
    "max_temperature_c": null
  },
  "shutdown_state": "disarmed",
  "shutdown_policy": "not_active",
  "last_error": null
}
```

### Proposed optional `homeassistant` fields

```json
{
  "available": true,
  "mqtt": true,
  "severity": "ok",
  "api_latency_ms": 20,
  "mqtt_last_publish_age_seconds": 12,
  "power_entities_ok": true,
  "z2m": {
    "available": true,
    "state": "online",
    "topic_base": "zigbee2mqtt"
  },
  "coordinator": {
    "available": true,
    "type": "Sonoff Dongle-M Ethernet",
    "state": "online"
  },
  "last_error": null
}
```

### Proposed optional `m5stack` fields

```json
{
  "available": true,
  "severity": "ok",
  "temperature_c": 42.4,
  "ram_available_mb": 695,
  "ram_usage_percent": null,
  "disk_free_gb": 20.0,
  "uptime_seconds": 86400,
  "services": {
    "llm-sys": "active",
    "power-sentinel-api": "active",
    "power-sentinel-stackflow-unit": "active",
    "nut-server": "active"
  },
  "stackflow_ok": true,
  "openai_ok": true,
  "chat_smoke_ok": false,
  "last_chat_smoke_age_seconds": null
}
```

### Backend metadata

```json
{
  "backend_version": "unknown",
  "generated_at": "2026-05-18T17:00:00Z",
  "data_sources": {
    "nut": "live",
    "ha": "live",
    "proxmox": "unavailable",
    "m5stack": "live"
  }
}
```

Transport counters remain firmware-local because the backend cannot know whether the CoreS3 received the response.

## Implemented / remaining work

Already implemented:

1. Backend parsing for live NUT fields already present in `upsc`:
   - model
   - battery voltage
   - nominal W
   - estimated load W
   - beeper status
   - transfer reason
   - driver
   - NUT service state
2. CoreS3 tabs restructured to `HOME`, `NUT`, `PVE`, `HA`, `M5S`.
3. V1a functional UI implemented:
   - sections/cards
   - bars
   - HOME dense overview
   - M5S transport/debug
4. V1b modern UI polish implemented:
   - improved spacing
   - consistent pills
   - better bar styling
   - visual severity badges/top hierarchy
5. PVE read-only API integration implemented.
6. HA/MQTT/Z2M/coordinator read-only checks implemented.
7. HOME display sleep control implemented.
8. LVGL MCP visual baseline workflow implemented under `assets/lvgl-spike/`.

Still remaining:

1. Deliberately enable/test Standard NUT monitors in stages only when the system should become armed.
2. Add more NUT clients beyond the first Proxmox secondary.
3. Add future mini-dashboards if another LAN subsystem deserves a first-class page.
4. Add local LLM inference only after there is a concrete use: enriched incident summaries, explanation text, or a companion tab.

No real shutdown automation belongs in this V1 display spec.

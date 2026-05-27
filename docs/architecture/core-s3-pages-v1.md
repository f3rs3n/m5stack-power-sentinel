# CoreS3 Pages V1 Specification

Status: V1 implemented; this file is the UI contract and regression reference.
Device: M5Stack CoreS3 + LLM Kit Power Sentinel

## Goal

Define and preserve the V1 CoreS3 information architecture for the broader Power Sentinel goal: an autonomous multi-function Linux server with a modern, practical display for LAN state.

The CoreS3 is a desk-visible physical homelab monitor. It should answer quickly and honestly:

1. Is power OK?
2. How much runtime remains if the UPS is on battery?
3. Is the NUT service live and how many clients are connected?
4. Are Proxmox, Home Assistant, MQTT/Zigbee2MQTT, network, and the M5Stack appliance healthy?
5. If something is wrong, which subsystem is the cause?
6. If the display itself looks stale, is StackFlow transport still working?

No page may show plausible demo/sample values after live transport fails. Missing data must render as `unknown`, `unavailable`, or an explicit stale/offline state.

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
- Sidebar navigation uses icon-only 18 px glyphs in a fixed 44 px left rail.
- Top-level tab switching is via the sidebar or vertical swipe through the left-sidebar tab stack.
- Horizontal gestures inside the content area belong to the current tab's card carousel.
- No automatic tab switch in V1.
- No automatic return-to-HOME timer in V1.
- No audible/beep alert in V1; visual alerts only.

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

## HOME tab

Purpose: dense desk overview.

HOME answers: “Is the homelab currently OK, and if not, where should I look?”

Required visible elements:

- Header: `POWER SENTINEL` or compact title.
- Global severity pill: `OK`, `WARN`, `CRITICAL`, `OFFLINE`, or `STALE`.
- Main power state: `GRID ONLINE`, `ON BATTERY`, `LOW BATTERY`, `UPS UNAVAILABLE`, or `STALE DATA`.
- UPS essentials: battery percent, runtime, load percent, input voltage.
- Subsystem status row: `NUT`, `PVE`, `HA`, `NET`, `M5S`.
- Problems summary: max 2-3 human-readable problems, or `No active problems`.

HOME must not show long IPs, tokens, model IDs, schemas, serial numbers, driver versions, or transport counters.

## NUT tab

Purpose: UPS telemetry, NUT service state, and connected-client visibility.

The `NUT` tab uses a compact horizontal carousel. V1 cards:

1. `UPS`: synthetic UPS state (`ONLINE`, `ON BATT`, `LOW BATT`, `UNAVAILABLE`, `STALE`), UPS model, battery/runtime, load/power W estimate, and input voltage.
2. `BATTERY`: battery-centric terms such as `Battery Charge`, `Runtime Remaining`, and `Battery Voltage`; the header pill carries compact battery state (`CHARGING`, `DISCHARGING`, `LOW`, `UNKNOWN`) instead of a body row.
3. `POWER`: electrical/load terms such as `Power Usage`, `System Load`, `Input Voltage`, `Output Voltage` when available, and nominal power.
4. `CLIENTS`: NUT service state, connected client count, and optional connected host list.

NUT UI rules:

- Do not duplicate the `ONLINE` pill with an `Online Charging` body line.
- Do not expose raw NUT status tokens such as `OL`/`OB`/`LB` on the normal card.
- Client names and counts must come from the API payload, not hardcoded UI assumptions.
- Future tiny sparklines may be tested separately, but only with backend-side history/downsampling and compact display payloads.

## PVE tab

Purpose: Proxmox node operational view.

Required visible elements:

- PVE API/node availability.
- CPU/RAM/storage percent and totals where available.
- ZFS and SMART health.
- Active non-loopback network interfaces.
- Running VM/LXC names/counts if space allows.

Do not show long VM/LXC tables on the 320x240 overview. Keep workload detail secondary.

## HA tab

Purpose: Home Assistant, MQTT, and Zigbee2MQTT health summary.

Required visible elements:

- HA API/reachability.
- MQTT broker state.
- Zigbee2MQTT bridge/coordinator state.
- Update count if available.
- Device interview count if available.

Do not turn this into a generic Home Assistant entity dashboard.

## M5S tab

Purpose: M5Stack appliance and transport health.

Required visible elements:

- Stack/API availability.
- StackFlow transport state and data age.
- Temperature, RAM, disk.
- LLM/OpenAI/chat smoke state if available.

`OpenAI FAIL` is warning, not critical, because it does not break the primary power-monitor function.

## API fields consumed by CoreS3

CoreS3 consumes these high-level groups from `power-sentinel.summary.v1`:

- `schema`
- `timestamp`
- `severity`
- `ups`
- `nut`
- `homeassistant`
- `mqtt`
- `zigbee2mqtt`
- `proxmox`
- `network`
- `m5stack`
- `problems`

NUT fields relevant to V1 display:

```json
{
  "nut": {
    "server_available": true,
    "driver_available": true,
    "connected_client_count": 1,
    "connected_clients": ["pve"]
  }
}
```

## Implementation / regression notes

- Firmware and LVGL fixture must remain visually aligned.
- Every visible layout change needs a render of affected cards/tabs.
- For carousel/card changes, render every changed card/state, not just the first visible card.
- Stale, unavailable, and unknown states must have explicit rendering.
- The normal display must not depend on demo/sample payloads.

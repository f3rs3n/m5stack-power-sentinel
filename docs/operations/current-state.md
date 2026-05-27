# Current State

This document records the current verified Power Sentinel runtime state. Keep it sanitized for public repo use.

## Hardware / topology

- M5Stack LLM Module is the autonomous Linux appliance.
- CoreS3 is the display surface.
- UPS is connected to the LLM Module over USB OTG and exposed through NUT as `homelab_ups`.
- CoreS3 display data is delivered through StackFlow/`llm_sys` using `work_id: "sentinel"` and `ipc:///tmp/rpc.sentinel`.

## Verified services

Verified on the physical module:

- SSH key login works; password/telnet/rpcbind hardening was applied.
- LLM/OpenAI-compatible API baseline is stable.
- Local LLM healthcheck is installed.
- MQTT/Home Assistant discovery for LLM module health is installed and verified.
- `nut-server` and UPS driver are active.
- `power-sentinel-api` serves `power-sentinel.summary.v1`.
- `power-sentinel-stackflow-unit` sends live summaries to CoreS3.

## UPS / NUT

Verified live UPS values after connection:

```text
ups.available: true
ups.status: OL
ups.status_label: Online
battery.charge: available
battery.runtime: available
ups.load: available
input.voltage: available
battery.voltage: available
ups.realpower.nominal: available
```

The local API and StackFlow path report live UPS/NUT data rather than placeholder values.

Sanitized example configs are in:

```text
backend/config/ups.conf.example
backend/config/upsd.conf.example
backend/config/nut-clients.example.json
```

## API summary fields currently expected

```text
schema: power-sentinel.summary.v1
severity: ok
ups.available: true
ups.status_label: Online
nut.server_available: true
nut.driver_available: true
nut.connected_client_count: integer
nut.connected_clients: optional host list
proxmox.available: true
proxmox.node: pve
proxmox.cpu_percent / ram_percent / storage_percent: available
proxmox.active_network_interfaces: available
homeassistant.available: true/false/unknown depending on current probe
mqtt.available: true/false
zigbee2mqtt.available: true/false
network.available: true
m5stack.available: true
problems: []
```

## CoreS3 UI

Current CoreS3 LVGL UI:

- Five tabs: `HOME`, `NUT`, `PVE`, `HA`, `M5S`.
- 44 px icon-only left sidebar.
- Horizontal card carousel inside each tab.
- HOME shows power state, UPS essentials, subsystem row, and problem summary.
- NUT shows `UPS`, `BATTERY`, `POWER`, and `CLIENTS` cards.
- PVE shows Proxmox node health/capacity/interfaces/workload summary.
- HA shows HA/MQTT/Zigbee2MQTT state and update/device counts.
- M5S shows appliance health and transport freshness.
- No boot/demo/sample payload: initial display state is explicit `boot`/`offline`/`waiting` until the first live StackFlow summary arrives.

## Not yet implemented

- NUT connected-host list polish beyond the current connected-count display.
- NUT trend/history graphs. Future spike: backend-side UPS history, downsampled endpoint, compact sparkline on CoreS3.
- Idempotent LLM Module installer script.
- Local LLM dashboard enrichment or companion UI.
- Additional LAN mini-dashboards.
- Optional OTA or more automated flash workflow.

## Reference docs

- `PRODUCT.md`
- `DESIGN.md`
- `docs/architecture/api-contract-v1.md`
- `docs/architecture/core-s3-pages-v1.md`
- `docs/operations/backend-ops.md`
- `docs/operations/llm-module-dependencies.md`
- `assets/lvgl-spike/README.md`

# M5Stack Power Sentinel

Homelab appliance project for M5Stack CoreS3 + LLM Kit / LLM Module + LLM Mate.

Goal: turn the stack into an autonomous multi-function Linux server offering:

- a NUT server for the directly attached UPS;
- information dashboards on the CoreS3 display, with a modern, captivating, but practical interface for LAN elements, starting with the UPS/NUT stack, the Proxmox node, the Home Assistant instance, and the M5Stack stack itself;
- an extensible dashboard model for future dedicated mini-dashboards;
- a later local-inference layer using the LLM Module to enrich the dashboards or add a companion tab;
- MQTT exposure of the stack sensors and health state to Home Assistant.

Current architecture target:

```text
UPS -> USB OTG -> M5Stack LLM Module -> NUT server
                              |-> local JSON/status API -> StackFlow custom unit -> CoreS3 display
                              |-> MQTT -> Home Assistant
                              |-> local LLM inference -> dashboard enrichment / companion tab (future)
```

Important principles:

- The LLM Module is the autonomous Linux appliance; Home Assistant and Proxmox are monitored/consumer systems, not prerequisites for the CoreS3 dashboard.
- Do not commit secrets or local credentials.
- The CoreS3 uses the internal stacked UART through vendor StackFlow/`llm_sys`, not a parallel `/dev/ttyS1` reader. This preserves vendor services and avoids UART contention.
- Dashboard data comes from the local `power-sentinel.summary.v1` contract. Missing data must render as unknown/unavailable/stale, never as plausible demo values.

## Repository layout

```text
backend/
  bin/        Python/service scripts for the LLM Module
  config/     sanitized config templates only
  systemd/    systemd unit/timer files
  tests/      backend tests
firmware/
  core-s3-display/  CoreS3 PlatformIO firmware project using M5Unified/M5GFX/LVGL
docs/
  plans/      historical plans and phase checklists
  architecture/ current API/UI architecture and design decisions
  operations/ verified runtime state and operational commands
scripts/      helper scripts for local development/deploy
tools/        tooling and validation helpers
assets/       UI mockups/icons/palettes/LVGL visual baselines
```

## VSCode / PlatformIO flash workflow

Martino normally pulls this repository from GitHub in VSCode on Windows and flashes the CoreS3 from there. Therefore the GitHub version must be directly buildable/flashable without private files.

Current firmware behavior:

- `firmware/core-s3-display/platformio.ini` pins `upload_port = COM4` and `monitor_port = COM4` for the current Windows/CoreS3 setup.
- The firmware includes `include/power_sentinel_config.example.h` automatically when the ignored local `include/power_sentinel_config.h` is absent.
- The default flash path uses the internal stacked UART/StackFlow transport, so no WiFi, MQTT, Home Assistant, Proxmox, or NUT secret is required at CoreS3 flash time.
- WiFi credentials are only needed for the optional HTTP development fallback. If needed, copy `include/power_sentinel_config.example.h` to `include/power_sentinel_config.h` locally and fill `WIFI_SSID` / `WIFI_PASSWORD`; do not commit that file.
- Runtime/backend secrets live on the LLM Module in root-only config files such as `/etc/power-sentinel.json`, `/etc/m5stack-ha-publish.json`, and NUT config. They are not firmware flash inputs.

## Current status

Implemented and verified on the physical module:

- light hardening: SSH key login, root password changed, SSH password login disabled, telnet/inetd disabled, rpcbind disabled;
- LLM/OpenAI-compatible API baseline stable;
- local LLM healthcheck installed;
- MQTT/Home Assistant discovery for LLM module health installed and verified;
- UPS connected over USB OTG and served by NUT as `homelab_ups`;
- NUT telemetry active: `nut-server` and `nut-driver` are active;
- backend API serving `power-sentinel.summary.v1` with live UPS/NUT, Proxmox, HA/MQTT/Zigbee2MQTT, network, and M5Stack sections;
- Proxmox data includes aggregate Total Node Capacity from `/nodes/{node}/storage`, active non-loopback interfaces, and selected workload mini-metrics;
- StackFlow transport verified: CoreS3 sends `work_id: "sentinel"` summaries over the internal UART, `llm_sys` routes to `ipc:///tmp/rpc.sentinel`, and the display renders live data;
- CoreS3 LVGL UI implemented with five tabs: `HOME`, `NUT`, `PVE`, `HA`, `M5S`, an icon-only 18 px left sidebar, standardized uppercase card headers, and horizontal card carousels;
- Proxmox read-only token includes `VM.GuestAgent.Audit` so VM disk usage can be derived from QEMU guest-agent `get-fsinfo`;
- global SSH aliases in Martino's Hermes environment are `ssh pve`, `ssh m5stack`, and `ssh doomtrain` for Proxmox, the LLM Module, and the Windows workstation respectively;
- LLM Module dependency inventory exists in `docs/operations/llm-module-dependencies.md` so the backend install can be reproduced on a fresh stack;
- LVGL MCP visual baseline workflow exists under `assets/lvgl-spike/` for 320x240 layout review.

Still missing / future work:

- connected NUT client host list polish beyond the current connected-count display;
- idempotent LLM Module installer script generated from `docs/operations/llm-module-dependencies.md`;
- real use of local LLM inference for dashboard enrichment or a companion tab;
- additional mini-dashboards beyond the current UPS/NUT, PVE, HA/Z2M, and M5Stack views;
- optional OTA or more automated flash workflow after the Windows/VSCode path remains stable.

## Documentation map

Product contract:

- `PRODUCT.md`

CoreS3 HMI/design contract:

- `DESIGN.md`
- `docs/architecture/core-s3-pages-v1.md`

API/backend contract:

- `docs/architecture/api-contract-v1.md`

Render/fixture workflow:

- `assets/lvgl-spike/README.md`

Operations/runbooks:

- `docs/operations/current-state.md`
- `docs/operations/backend-ops.md`
- `docs/operations/llm-module-dependencies.md`

Precedence rule: if documents disagree, `PRODUCT.md` wins for product boundaries, `DESIGN.md` wins for HMI/design rules, `docs/architecture/api-contract-v1.md` wins for backend schema, and `assets/lvgl-spike/README.md` wins for render workflow.

# M5Stack Power Sentinel

Homelab power-sentinel project for M5Stack CoreS3 + LLM Kit / LLM Module + LLM Mate.

Goal: turn the stack into a low-power autonomous UPS/NUT appliance, CoreS3 physical display, Home Assistant MQTT integration, and future Hermes/voice satellite.

Current architecture target:

```text
UPS -> USB OTG -> M5Stack LLM Module -> NUT server
                              |-> local JSON/status API -> StackFlow custom unit -> CoreS3 display
                              |-> MQTT -> Home Assistant
                              |-> NUT clients -> Proxmox / HA / other hosts
```

Important principles:

- The M5Stack is the primary UPS state source.
- Home Assistant is a consumer/automation layer, not required for the CoreS3 UPS page.
- Proxmox shutdown will stay read-only/dry-run until explicitly approved and tested.
- Do not commit secrets or local credentials.
- The CoreS3 uses the internal stacked UART through vendor StackFlow/`llm_sys`, not a parallel `/dev/ttyS1` reader. This preserves vendor services and avoids UART contention.

## Repository layout

```text
backend/
  bin/        Python/service scripts for the LLM Module
  config/     sanitized config templates only
  systemd/    systemd unit/timer files
  tests/      backend tests
firmware/
  core-s3-display/  CoreS3 firmware project, planned PlatformIO + M5Unified/M5GFX
docs/
  plans/      project plans and phase checklists
  architecture/
  operations/
scripts/      helper scripts for local development/deploy
tools/        tooling and validation helpers
assets/       UI mockups/icons/palettes
```

## Current status

Already completed on the physical module:

- light hardening: SSH key login, root password changed, SSH password login disabled, telnet/inetd disabled, rpcbind disabled;
- LLM/OpenAI-compatible API baseline stable;
- local LLM healthcheck installed;
- MQTT/Home Assistant discovery for LLM module health installed and verified;
- NUT packages installed, but NUT services intentionally disabled until UPS USB OTG cable arrives;
- read-only UPS detection script prepared;
- StackFlow transport verified: CoreS3 sends `work_id: "sentinel"` summaries over the internal UART, `llm_sys` routes to `ipc:///tmp/rpc.sentinel`, and the display renders live data.

See:

- `docs/plans/homelab-power-sentinel-plan-2026-05-15.md`

## Near-term phases

1. Deploy Power Sentinel API V0 and use `/api/v1/summary` as the CoreS3/frontend contract.
2. Connect UPS via USB OTG and run discovery.
3. Configure NUT server as `homelab_ups`.
4. Replace current UPS-unavailable state with real NUT data.
5. Build UPS MQTT/Home Assistant integration.
6. Build CoreS3 display firmware.
7. Add Home Assistant automations.
8. Add Proxmox NUT client in read-only mode, then dry-run shutdown, then real shutdown only after explicit approval.

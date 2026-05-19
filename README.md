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
- Proxmox shutdown stays disarmed until Standard NUT client monitoring is explicitly enabled and tested.
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
  core-s3-display/  CoreS3 PlatformIO firmware project using M5Unified/M5GFX/LVGL
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
- UPS connected over USB OTG and served by NUT as `homelab_ups`;
- NUT telemetry active: `nut-server` and `nut-driver` are active, while `nut-monitor` is currently disabled/inactive on both the M5Stack primary and Proxmox secondary;
- Proxmox NUT secondary readiness proven, then deliberately disarmed;
- StackFlow transport verified: CoreS3 sends `work_id: "sentinel"` summaries over the internal UART, `llm_sys` routes to `ipc:///tmp/rpc.sentinel`, and the display renders live data.

See:

- `docs/operations/current-state.md`
- `docs/operations/backend-ops.md`

## Current next steps

1. Keep the Standard NUT shutdown path staged/disarmed until the final deliberate arming step.
2. Continue polish/verification of the CoreS3 display and Home Assistant/Zigbee2MQTT read-only status.
3. When ready, deliberately re-enable `nut-monitor` on the M5Stack primary first, then on Proxmox secondary, with live verification after each step.

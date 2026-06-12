# Power Sentinel API contract v1

Current profile: `nut-monitor-clean-baseline`.

## Endpoint

`GET /api/v1/summary`

`GET /api/v1/summary?stackflow_safe=1` for CoreS3/StackFlow polling.

## Top-level fields

- `schema`: `power-sentinel.summary.v1`
- `timestamp`: UTC ISO timestamp
- `product`: `Power Sentinel`
- `profile`: `nut-monitor-clean-baseline`
- `severity`: aggregate `ok` / `warn` / `critical` / `unknown`
- `available_modules`: stable registry, currently `["nut", "proxmox", "ha"]`
- `enabled_modules`: modules selected by `/etc/power-sentinel.json` or `POWER_SENTINEL_MODULES`
- `pages`: UI pages corresponding to enabled modules
- `module`: local Module LLM/backend status used by the CoreS3 top bar
- `modules`: per-module payloads
- `ups`, `nut`: compatibility aliases for the NUT Monitor firmware

`module` exposes:

- `lan_connected`: boolean Module LLM Ethernet/global IPv4 status
- `lan_ip`: selected Module LLM IPv4 address as a string, or `null`
- `time_hhmm`: Module LLM local time in 24-hour `HH:MM` format, or `null` if unavailable. The deployed Module LLM host is expected to use the local site timezone (`Europe/Rome` for the current installation).

These fields are informational UI status only. They do not affect aggregate `severity` or NUT hero selection.

## Module policy

Only `modules.nut` is implemented in this baseline.

`modules.proxmox` and `modules.ha` are placeholders. They may be enabled in config to reserve pages, but must not fake telemetry. Reintroduce them as separate modules with tests before rendering live data.

## NUT module

`modules.nut.ups` and top-level `ups` expose:

- `available`, `stale`
- `status`, `status_label`
- `on_battery`, `low_battery`, `charging`
- `would_shutdown`
- `battery_percent`, `battery_voltage`, `runtime_seconds`
- `load_percent`, `realpower_nominal_w`, `load_w`
- `input_voltage`, `output_voltage`
- `beeper_status`, `transfer_reason`, `driver`

Shutdown rule: `would_shutdown` is true only when `ups.status` contains both `OB` and `LB`. `OL ... LB` is warning/no-shutdown.

`modules.nut.nut` and top-level `nut` expose:

- `mode`
- `driver_active`, `server_active`, `monitor_active`
- `client_count`, `clients`
- `shutdown_owner: upsmon`
- `would_shutdown`
- `shutdown_rule`

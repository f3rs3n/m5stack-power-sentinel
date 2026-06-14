# Power Sentinel API contract v1

Current profile: `nut-monitor-clean-baseline`.

## Endpoints

`GET /api/v1/summary`

`GET /api/v1/summary?stackflow_safe=1` for CoreS3/StackFlow polling.

`GET /api/v1/health`

## Top-level fields

- `schema`: `power-sentinel.summary.v1`
- `timestamp`: UTC ISO timestamp
- `product`: `Power Sentinel`
- `profile`: `nut-monitor-clean-baseline`
- `condition`: aggregate module condition. This is the primary interpreted state.
- `severity`: legacy compatibility alias: `ok` / `warn` / `critical` / `unknown`
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

These fields are informational UI status only. They do not affect aggregate `condition` or NUT hero selection.

## Condition and legacy severity

Canonical condition values are:

- `healthy`
- `stale`
- `warning`
- `critical`
- `unavailable`

`condition` is the source of truth for new module logic. `severity` remains for the current CoreS3 NUT monitor firmware and older clients.

Legacy severity aliases:

- `healthy` -> `ok`
- `warning` -> `warn`
- `critical` -> `critical`
- `stale` -> `warn`
- `unavailable` -> `warn`

Top-level aggregation uses worst condition wins: `critical`, then `warning`, then `stale`, then `unavailable`, then `healthy`. Disabled modules do not affect the aggregate condition.

## Module policy

`modules.nut` is implemented in this baseline. `modules.proxmox` has an initial read-only API adapter slice: it validates configuration, can make lightweight Proxmox API reads, and still avoids SSH, remote commands, guest control, or fake telemetry.

`modules.ha` is still a placeholder. It may be enabled in config to reserve a page, but must not fake telemetry. Reintroduce it as a separate module with tests before rendering live data.

When enabled, a placeholder or unobservable module reports `condition: unavailable` because it cannot produce useful data. Disabled modules do not report a condition and do not affect the aggregate condition.

## NUT module

`modules.nut.condition` exposes the NUT Monitor condition. `modules.nut.severity` is the legacy alias.

The CoreS3 Ambient Console consumes the NUT module through the stable adapter contract in `docs/architecture/nut-ambient-console-contract.md`. That contract is the first page-model shape for future Ambient Console module pages; it keeps Proxmox page rendering, Overview, Telemetry History, and controls explicitly out of scope.

Current mapping:

- line power normal (`OL`) -> `condition: healthy`, `severity: ok`
- on battery (`OB`) -> `condition: warning`, `severity: warn`
- shutdown condition (`OB LB`) -> `condition: critical`, `severity: critical`
- NUT/UPS unavailable -> `condition: unavailable`, `severity: warn`

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

## Proxmox module availability slice

`modules.proxmox` is implemented as a minimal read-only Proxmox API adapter. It uses no SSH, no remote commands, and no Proxmox controls.

Minimum config:

- `proxmox.api_url`
- `proxmox.token_id`
- `proxmox.token_secret`

Optional Network card config:

- `proxmox.network_uplink`: physical uplink interface when API auto-selection is ambiguous
- `proxmox.network_uplink_speed_mbps`: uplink speed when API data does not expose reliable speed

If enabled without minimum config, `modules.proxmox` reports:

- `implemented: true`
- `condition: unavailable`
- `severity: warn`
- `status: unconfigured`
- `signals[0].kind: unconfigured`

If minimum config exists but the token is still the example placeholder `CHANGE_ME`, this slice does not attempt a live API call and reports:

- `condition: unavailable`
- `status: not_observed`
- `signals[0].kind: api_unavailable`
- `nodes: []`
- `guests: []`

With real credentials, this slice reads:

- `/api2/json/cluster/status`
- `/api2/json/nodes`
- `/api2/json/nodes/{node}/storage` for online nodes
- `/api2/json/nodes/{node}/qemu` for online nodes
- `/api2/json/nodes/{node}/lxc` for online nodes
- `/api2/json/nodes/{node}/network` for physical uplink selection and speed when available
- `/api2/json/nodes/{node}/netstat` for latest uplink traffic rates

First healthy observation reports:

- `condition: healthy`
- `severity: ok`
- `status: observed`
- `signals: []`
- `environment.name`
- `environment.node_count`
- `environment.online_node_count`
- `environment.guest_total_count`
- `environment.guest_running_count`
- `environment.network_uplink` when selected and observable
- `environment.storage_count`
- `nodes[]` with `name`, `condition`, `status`, and best-effort `cpu_percent` / `memory_percent`
- `storage[]` with active/enabled storage entries only
- `guests[]` with visible QEMU VMs and LXC containers

Node condition and signals:

- node `status: online` -> node `condition: healthy`
- node `status: offline` -> node `condition: critical`, signal `node_down`
- any other non-online node status -> node `condition: critical`, signal `node_degraded`
- any node signal makes `modules.proxmox.condition: critical`

Guest Inventory Summary condition and signals:

- no visible guests (`0/0`) -> guest card `condition: healthy`
- all visible guests running -> guest card `condition: healthy`
- some visible guests stopped -> guest card `condition: warning`, signal `guest_down`
- zero running out of a non-zero total -> guest card `condition: critical`, signal `guest_down`
- guest signals participate in worst-condition aggregation for `modules.proxmox.condition`

Storage condition and signals:

- inactive or disabled storage entries are ignored
- `used/total < 85%` -> storage `condition: healthy`
- `used/total >= 85%` -> storage `condition: warning`, signal `storage_warning`
- `used/total >= 95%` -> storage `condition: critical`, signal `storage_critical`
- storage signals participate in worst-condition aggregation for `modules.proxmox.condition`

Network condition and signals:

- one active physical uplink can be auto-selected; multiple active physical uplinks require `proxmox.network_uplink`
- API-provided `speed_mbps`/`speed` is preferred; otherwise `proxmox.network_uplink_speed_mbps` is required
- latest saturation is the greater of inbound/outbound bps as a percentage of uplink speed
- one sample at or above 80%/95% does not change condition
- sustained saturation at or above 80% -> network card `condition: warning`, signal `network_pressure`
- sustained saturation at or above 95% -> network card `condition: critical`, signal `network_pressure`
- unavailable Network card data while the selected node is observable -> `network_unavailable` warning-level attention, not global Proxmox unavailable

API/auth failure reports:

- `condition: unavailable`
- `severity: warn`
- `status: api_unavailable`
- `signals[0].kind: api_unavailable`

Token secrets must not be copied into payloads or signal summaries.

## Health endpoint

`GET /api/v1/health` returns:

- `schema`: `power-sentinel.health.v1`
- `ok`: false only when `condition` is `critical`
- `condition`: copied from the summary aggregate condition
- `severity`: legacy alias copied from the summary aggregate severity
- `timestamp`: UTC ISO timestamp

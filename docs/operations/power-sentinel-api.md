# Power Sentinel API Operations

Scope: normalized local state for the autonomous multi-function Linux appliance. The API feeds the CoreS3 dashboards and exposes UPS/NUT, Proxmox, Home Assistant/MQTT/Zigbee2MQTT, network, and M5Stack health.

## Endpoint

```text
GET /api/v1/summary
```

The CoreS3 normally consumes the StackFlow-safe form where required by transport limits.

## Top-level groups

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

## UPS/NUT fields

UPS telemetry is normalized under `ups`:

- `ups.available`
- `ups.status`, `ups.status_label`
- `ups.on_battery`, `ups.low_battery`
- `ups.battery_percent`
- `ups.runtime_seconds`
- `ups.load_percent`
- `ups.input_voltage`, `ups.output_voltage`
- `ups.stale`, `ups.age_seconds`

NUT service/client visibility is normalized under `nut`:

- `nut.server_available`
- `nut.driver_available`
- `nut.connected_client_count`
- `nut.connected_clients[]`

Only the connected count is required for the normal CoreS3 UI. A compact host list may be shown later if it remains readable.

## Proxmox fields

- `proxmox.available`
- `proxmox.severity`
- `proxmox.node`, `proxmox.node_status`
- `proxmox.cpu_percent`
- `proxmox.ram_percent`, `proxmox.ram_total_bytes`
- `proxmox.storage_percent`, `proxmox.storage_total_bytes`
- `proxmox.active_network_interfaces[]`
- `proxmox.zfs.status`
- `proxmox.smart.status`
- optional running VM/LXC summaries and mini-metrics

## Home Assistant / MQTT / Zigbee2MQTT fields

- HA API/reachability state
- MQTT broker state
- Zigbee2MQTT bridge/coordinator state
- update count when available
- device interview count when available

## M5Stack fields

- `m5stack.available`
- `m5stack.severity`
- `m5stack.temperature_c`
- `m5stack.ram_available_mb`
- `m5stack.disk_free_gb`
- `m5stack.stackflow_ok`
- `m5stack.openai_ok`
- `m5stack.chat_smoke_ok`

## Operational notes

- Do not print or commit token secrets.
- Proxmox uses read-only Proxmox API data when a token is configured.
- Missing data should render as unknown/unavailable/stale, never as plausible demo values.
- StackFlow-safe summaries should remain compact enough for the CoreS3 transport.

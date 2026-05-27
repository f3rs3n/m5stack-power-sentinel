# Architecture Notes

## Power Sentinel role

The M5Stack LLM Module is treated as an autonomous multi-function Linux appliance for the homelab.

Primary responsibilities:

- run NUT server for the directly attached UPS;
- produce normalized local JSON state for dashboards and integrations;
- expose a local API for the CoreS3 display;
- publish MQTT retained state and Home Assistant Discovery for stack sensors/health;
- provide read-only information dashboards for UPS/NUT, Proxmox, Home Assistant/Zigbee2MQTT, network, and the M5Stack appliance itself;
- remain extensible for future mini-dashboards;
- later use local LLM Module inference to enrich dashboard content or add a companion tab.

## Data contract direction

The CoreS3 consumes a compact summary endpoint, not raw NUT/Home Assistant/Proxmox APIs.

Current endpoint:

```text
GET /api/v1/summary
```

Current schema:

```json
{
  "schema": "power-sentinel.summary.v1",
  "timestamp": "2026-05-17T12:00:00Z",
  "severity": "ok",
  "ups": {
    "available": true,
    "status": "OL",
    "status_label": "Online",
    "on_battery": false,
    "low_battery": false,
    "battery_percent": 100,
    "runtime_seconds": 6120,
    "load_percent": 18,
    "input_voltage": 231.2,
    "output_voltage": 230.0,
    "stale": false,
    "age_seconds": 4
  },
  "nut": {
    "server_available": true,
    "driver_available": true,
    "connected_client_count": 1,
    "connected_clients": ["pve"]
  },
  "homeassistant": {
    "available": true,
    "severity": "ok"
  },
  "proxmox": {
    "available": true,
    "severity": "ok",
    "node": "pve",
    "node_status": "online",
    "cpu_percent": 18,
    "ram_percent": 46,
    "ram_total_bytes": 34359738368,
    "storage_percent": 8,
    "storage_total_bytes": 7897066643456,
    "active_network_interfaces": ["eth25g"],
    "zfs": {"status": "ONLINE"},
    "smart": {"status": "OK"}
  },
  "m5stack": {
    "available": true,
    "severity": "ok",
    "temperature_c": 45.4,
    "ram_available_mb": 500,
    "disk_free_gb": 20.5,
    "stackflow_ok": true,
    "openai_ok": true,
    "chat_smoke_ok": true
  },
  "problems": []
}
```

Implementation status:

- `ups` and `nut`: implemented from live NUT/upsc data for the APC Back-UPS ES 850G2.
- `proxmox`: implemented as read-only API integration for one node (`pve`) with CPU/RAM/storage/ZFS/SMART, running VM/LXC summaries, active non-loopback network interfaces, and aggregate Total Node Capacity from `/nodes/{node}/storage`.
- `homeassistant`, `mqtt`, `zigbee2mqtt`: implemented as HA TCP reachability plus MQTT/Zigbee2MQTT bridge-topic health; HA update count is read from HA `/api/states` when a read-only token is configured.
- `network`: implemented on the LLM Module using Linux default-route state plus a TCP probe, currently `1.1.1.1:53`.
- `m5stack`: implemented from local healthcheck/service state.
- NUT history/trend endpoints are not implemented. If tested later, history should be backend-owned and downsampled, not a raw display concern.
- Local LLM inference for dashboard enrichment / companion UI is not implemented yet; current LLM usage is baseline health and optional chat smoke.

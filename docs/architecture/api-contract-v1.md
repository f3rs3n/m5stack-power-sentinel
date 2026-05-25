# Architecture Notes

## Power Sentinel role

The M5Stack LLM Module is treated as an autonomous multi-function Linux appliance for the homelab.

Primary responsibilities:

- run NUT server for the directly attached UPS;
- produce normalized local JSON state for dashboards and integrations;
- expose a local API for the CoreS3 display;
- publish MQTT retained state and Home Assistant Discovery for stack sensors/health;
- serve as NUT source for Proxmox/Home Assistant/other clients;
- provide read-only information dashboards for UPS/NUT, Proxmox, Home Assistant/Zigbee2MQTT, network, and the M5Stack appliance itself;
- remain extensible for future mini-dashboards;
- later use local LLM Module inference to enrich dashboard content or add a companion tab;
- remain an observer/readiness dashboard for shutdown policy; real shutdown is Standard NUT via `upsmon`, not custom shutdown orchestration.

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
    "smart": {"status": "OK"},
    "shutdown_state": "disarmed"
  },
  "shutdown": {
    "real_shutdown_owner": "upsmon",
    "primary_ready": true,
    "primary_monitor_active": false,
    "nut_upsmon": {"armed": false, "state": "disarmed", "label": "DISARMED"},
    "secondary_ready": true,
    "nut_client_summary": {"total": 4, "secondary_total": 3, "connected": 1, "armed": 1, "unknown": 1, "unavailable": 0},
    "nut_clients": [
      {
        "name": "m5stack",
        "host": "localhost",
        "role": "primary",
        "available": true,
        "state": "disarmed",
        "configured": true,
        "connected_as_upsmon": false,
        "armed": false,
        "last_seen_seconds": 0
      },
      {
        "name": "nas",
        "host": "192.168.2.50",
        "role": "secondary",
        "available": null,
        "state": "armed",
        "configured": true,
        "connected_as_upsmon": true,
        "armed": true
      },
      {
        "name": "pve",
        "host": "192.168.2.99",
        "role": "secondary",
        "available": null,
        "state": "reachable_via_upsc",
        "connected_as_upsmon": false,
        "armed": false
      },
      {
        "name": "backup-host",
        "host": "192.168.2.60",
        "role": "secondary",
        "available": null,
        "state": "unknown",
        "configured": true,
        "connected_as_upsmon": false,
        "armed": false,
        "discovery_error": null
      }
    ],
    "proxmox_nut_client": {
      "name": "pve",
      "host": "192.168.2.99",
      "role": "secondary",
      "state": "reachable_via_upsc",
      "connected_as_upsmon": false,
      "armed": false
    },
    "would_shutdown": false,
    "reason": "UPS online"
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

Implementation status, verified 2026-05-24:

- `ups` and `nut`: implemented from live NUT/upsc data for the APC Back-UPS ES 850G2.
- `proxmox`: implemented as read-only API integration for one node (`pve`) with CPU/RAM/storage/ZFS/SMART, running VM/LXC summaries, active non-loopback network interfaces, aggregate Total Node Capacity from `/nodes/{node}/storage`, and optional per-running-workload CPU/RAM/disk mini-metrics. Running VM RAM is enriched from `/qemu/{vmid}/status/current`; VM disk usage is enriched through QEMU guest-agent `get-fsinfo` when `VM.GuestAgent.Audit` is available. Misleading VM `disk=0` values are rendered as unknown unless fsinfo supplies real filesystem usage; HAOS read-only `erofs` root is ignored in favor of its data filesystem.
- `homeassistant`, `mqtt`, `zigbee2mqtt`: implemented as HA TCP reachability plus MQTT/Zigbee2MQTT bridge-topic health; HA update count is read from HA `/api/states` when a read-only token is configured, otherwise it defaults to `0` for display clarity.
- `network`: implemented on the LLM Module using Linux default-route state plus a TCP probe, currently `1.1.1.1:53`.
- `m5stack`: implemented from local healthcheck/service state.
- `shutdown`: implemented as Standard NUT readiness state only; no custom shutdown action exists. `shutdown.nut_upsmon` exposes the primary local upsmon state as `{armed,state,label}` for explicit `NUT upsmon: ARMED/DISARMED` UI wording. `shutdown.nut_clients[]` is a normalized multi-client inventory/readiness list whose first item is always the local primary (`role=primary`, `name=m5stack`) followed by configured secondary clients. Client state enum is `armed`, `disarmed`, `connected_as_upsmon`, `reachable_via_upsc`, `configured_not_connected`, `not_configured`, `unknown`, and `unavailable`. `shutdown.nut_client_summary` counts `total`, `secondary_total`, `connected`, `armed`, `unknown`, and `unavailable`; unknown/unavailable clients never contribute to connected/armed counts or to `secondary_ready`. `shutdown.proxmox_nut_client` is selected from the configured Proxmox node/host and is the only source for the PVE dashboard `NUT armed` / `NUT disarmed` pill; aggregate `nut_client_summary` is global NUT context only. Any future maintenance-control endpoint must be designed separately from this V1 read-only summary contract and must be limited to LLM Module local `upsmon` hold/release as described in `docs/operations/standard-nut-arming-runbook.md`; downstream client control, Proxmox orchestration, and FSD are explicitly out of scope.
- Local LLM inference for dashboard enrichment / companion UI is not implemented yet; current LLM usage is baseline health and optional chat smoke.

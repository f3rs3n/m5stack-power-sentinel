# Architecture Notes

## Power Sentinel role

The M5Stack LLM Module is treated as an autonomous low-power Linux appliance.

Primary responsibilities:

- run NUT server for the directly attached UPS;
- produce normalized local JSON state;
- expose a local API for the CoreS3 display;
- publish MQTT retained state and Home Assistant Discovery;
- serve as NUT source for Proxmox/Home Assistant/other clients;
- remain an observer/dry-run dashboard for shutdown policy; real shutdown is Standard NUT via `upsmon`, not custom shutdown orchestration.

## Data contract direction

The CoreS3 should consume a compact summary endpoint, not raw NUT/Home Assistant/Proxmox APIs.

Planned endpoint:

```text
GET /api/v1/summary
```

Planned schema:

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
    "shutdown_state": "disarmed"
  },
  "shutdown": {
    "strategy": "standard-nut",
    "mode": "dry-run",
    "real_shutdown_owner": "upsmon",
    "primary_ready": true,
    "primary_monitor_active": false,
    "secondary_ready": false,
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

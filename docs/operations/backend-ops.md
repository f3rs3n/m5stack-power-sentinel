# Backend Operations

Operational notes for the Power Sentinel backend on the M5Stack LLM Module.

## Services

Main services:

- `power-sentinel-api`: local summary API.
- `power-sentinel-stackflow-unit`: StackFlow bridge/unit for CoreS3 display data.
- `nut-server`: NUT service for the directly attached UPS.
- `nut-driver`: UPS driver service.
- MQTT/Home Assistant publisher timers/services as configured.

## Useful checks

```bash
systemctl status power-sentinel-api power-sentinel-stackflow-unit nut-server nut-driver --no-pager
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
upsc homelab_ups@localhost ups.status
upsc homelab_ups@localhost battery.charge
upsc homelab_ups@localhost ups.load
```

## NUT telemetry

`nut-server` is enabled so UPS telemetry survives reboot. The dashboard consumes live NUT/upsc data for UPS status, battery charge, runtime, load, input voltage, service state, and connected-client count.

Sanitized config templates:

```text
backend/config/ups.conf.example
backend/config/upsd.conf.example
backend/config/nut-clients.example.json
```

The optional NUT client inventory is for display only. The UI currently needs the connected count; a compact connected-host list may be added later.

## Summary API smoke check

Expected high-level fields:

```text
schema: power-sentinel.summary.v1
severity: ok|warn|critical|stale|offline
ups.available: true/false
ups.status_label: Online|On battery|Low battery|Unavailable|Stale
nut.server_available: true/false
nut.driver_available: true/false
nut.connected_client_count: integer
nut.connected_clients: optional list of host names
proxmox.available: true/false
homeassistant.available: true/false
mqtt.available: true/false
zigbee2mqtt.available: true/false
network.available: true/false
m5stack.available: true/false
problems: [] or short human-readable problem objects
```

## MQTT / HA / Zigbee2MQTT

MQTT and HA publisher configuration lives in root-only config files on the LLM Module. Do not commit token secrets.

Useful checks:

```bash
systemctl status m5stack-ha-publish.timer m5stack-ha-publish-chat.timer --no-pager
mosquitto_sub -h <broker> -t 'homeassistant/status' -C 1
curl -fsS http://<ha-host>:8123/api/ -H "Authorization: Bearer <token>"
```

## Proxmox integration

Proxmox uses a read-only API token. The backend may enrich VM disk usage through QEMU guest-agent filesystem info when permission is available.

Do not print or commit token secrets.

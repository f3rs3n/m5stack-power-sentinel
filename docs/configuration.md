# Configuration Surface

The public v1 Configuration Surface is `/etc/power-sentinel.json` plus read-only validation and troubleshooting commands.

Power Sentinel v1 does not provide a mutable config CLI, web UI, wizard, or CoreS3 setup/options page.

## Module-local configuration

Each module owns its own configuration section and uses a shared `enabled` flag.

NUT-only default:

```json
{
  "nut": {
    "enabled": true,
    "ups": "homelab_ups@localhost",
    "clients_file": "/etc/power-sentinel-nut-clients.json"
  },
  "proxmox": {
    "enabled": false,
    "api_url": "https://pve.example:8006",
    "token_id": "power-sentinel@pve!monitor",
    "token_secret": "CHANGE_ME"
  }
}
```

NUT plus optional Proxmox:

```json
{
  "nut": {
    "enabled": true,
    "ups": "homelab_ups@localhost",
    "clients_file": "/etc/power-sentinel-nut-clients.json"
  },
  "proxmox": {
    "enabled": true,
    "api_url": "https://pve.example:8006",
    "token_id": "power-sentinel@pve!monitor",
    "token_secret": "REPLACE_WITH_READ_ONLY_TOKEN_SECRET",
    "network_uplink": "eno1",
    "network_uplink_speed_mbps": 1000
  }
}
```

`network_uplink` and `network_uplink_speed_mbps` are only needed when the Proxmox API cannot make physical uplink observation unambiguous.

## Validate configuration and runtime state

Check that the JSON is valid:

```bash
python3 -m json.tool /etc/power-sentinel.json
```

Check the one-shot summary from a checkout:

```bash
python3 backend/bin/power-sentinel-api.py --summary
```

Check an installed service:

```bash
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
systemctl is-active power-sentinel-api.service power-sentinel-stackflow-unit.service
```

## State vocabulary

- `disabled`: the module exists but is not enabled. It may appear in diagnostics, but it does not expose an active page or affect the aggregate condition.
- `unconfigured`: the module is enabled but lacks minimum useful configuration. It normally reports `condition: unavailable` with configuration context.
- `unavailable`: the module cannot currently produce useful data, usually because it is unconfigured, unreachable from startup, or failed API/auth checks.
- `stale`: the module previously had useful data, but the data is no longer fresh enough to trust.
- `warning`: attention is needed but the situation is not immediately critical.
- `critical`: urgent attention or safety-relevant handoff is needed.

## Secrets

Do not commit deployed `/etc/power-sentinel.json` files. Token secrets must not appear in API payloads, logs, screenshots, serial captures, issues, or troubleshooting output.

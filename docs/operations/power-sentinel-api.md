# Power Sentinel API

V0 backend for the M5Stack Power Sentinel project.

## Script

```text
backend/bin/power-sentinel-api.py
```

Target deployed path on the LLM Module:

```text
/usr/local/bin/power-sentinel-api
```

## Endpoints

```text
GET /api/v1/summary
GET /api/v1/health
GET /api/v1/ups
```

The important frontend contract is:

```text
GET /api/v1/summary
```

Schema:

```text
power-sentinel.summary.v1
```

## Current behavior

If the UPS/NUT path is unavailable:

- UPS section is deliberately `available=false`, `status=UNAVAILABLE`, `stale=true`;
- no plausible placeholder UPS values are emitted.

With the current APC Back-UPS ES 850G2 connected via USB OTG/NUT, `/api/v1/summary` exposes live UPS essentials plus V1 display fields:

- `ups.model`
- `ups.battery_voltage`
- `ups.realpower_nominal_w`
- `ups.load_w`
- `ups.beeper_status`
- `ups.transfer_reason`
- `ups.driver`

The summary also exposes the low-complexity NUT service state needed by the CoreS3 `NUT` page:

- `nut.server_active`
- `nut.driver_active`
- `nut.monitor_active`
- `nut.mode`
- `nut.client_count`
- `nut.clients`
- `nut.shutdown_state`

Other sections:

- M5Stack section tries to read `/usr/local/bin/m5stack-healthcheck --json`;
- Home Assistant reachability uses TCP `192.168.2.200:8123`;
- MQTT reachability uses TCP `192.168.2.200:1883`;
- Proxmox still keeps a cheap TCP reachability check for availability, but the `proxmox` summary object can now use read-only Proxmox API data when a token is configured.

## Proxmox read-only integration

Configure a dedicated read-only Proxmox API token in root-only runtime config:

```text
/etc/power-sentinel.json
```

Sanitized shape:

```json
{
  "proxmox": {
    "host": "192.168.2.99",
    "port": 8006,
    "node": "pve",
    "verify_ssl": false,
    "token_id": "power-sentinel@pve!readonly",
    "token_secret": "CHANGE_ME"
  }
}
```

The API reads these PVE endpoints only:

- `/api2/json/nodes/{node}/status`
- `/api2/json/nodes/{node}/qemu`
- `/api2/json/nodes/{node}/lxc`
- `/api2/json/nodes/{node}/disks/zfs` best-effort
- `/api2/json/nodes/{node}/disks/list` best-effort

No SSH, no shutdown action, no `smartctl` custom script, and no expected-state model are used in this V1 read-only step.

The `proxmox` summary object includes:

- `available`, `severity`, `node`, `node_status`, `api_latency_ms`
- `cpu_percent`, `ram_percent`, `cpu_temp_c`, `storage_percent`
- `zfs.status`, `zfs.pools[]`
- `smart.status`, `smart.failing_count`, `smart.warning_count`
- `vm.running_count`, `vm.running_names[]`
- `lxc.running_count`, `lxc.running_names[]`
- `shutdown_state=disarmed`
- `problems[]`

Environment overrides:

```text
POWER_SENTINEL_BIND
POWER_SENTINEL_PORT
POWER_SENTINEL_HEALTHCHECK
POWER_SENTINEL_UPSC
POWER_SENTINEL_UPS
POWER_SENTINEL_HA_HOST
POWER_SENTINEL_MQTT_HOST
POWER_SENTINEL_PROXMOX_HOST
POWER_SENTINEL_PROXMOX_PORT
POWER_SENTINEL_PROXMOX_NODE
POWER_SENTINEL_PROXMOX_TOKEN_ID
POWER_SENTINEL_PROXMOX_TOKEN_SECRET
POWER_SENTINEL_PROXMOX_VERIFY_SSL
POWER_SENTINEL_CONFIG
```

## Local development

Run one-shot output:

```bash
python3 backend/bin/power-sentinel-api.py --once summary
python3 backend/bin/power-sentinel-api.py --once health
python3 backend/bin/power-sentinel-api.py --once ups
```

Run HTTP server locally:

```bash
python3 backend/bin/power-sentinel-api.py --host 127.0.0.1 --port 8088
curl -s http://127.0.0.1:8088/api/v1/summary | python3 -m json.tool
```

## Deploy target

```bash
install -m 755 backend/bin/power-sentinel-api.py /usr/local/bin/power-sentinel-api
install -m 644 backend/systemd/power-sentinel-api.service /etc/systemd/system/power-sentinel-api.service
systemctl daemon-reload
systemctl enable --now power-sentinel-api.service
```

## Notes

This API intentionally does not depend on Home Assistant or MQTT. The CoreS3 display should read this API directly from the LLM Module.

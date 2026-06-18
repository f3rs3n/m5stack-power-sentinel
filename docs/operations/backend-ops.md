# Backend operations

## Local one-shot summary

```bash
python3 backend/bin/power-sentinel-api.py --summary
```

## Run API manually

```bash
python3 backend/bin/power-sentinel-api.py --host 127.0.0.1 --port 8088
curl -fsS http://127.0.0.1:8088/api/v1/summary | python3 -m json.tool
curl -fsS http://127.0.0.1:8088/api/v1/health | python3 -m json.tool
```

`condition` is the primary interpreted state for new module logic. `severity`
is a legacy alias kept for the current CoreS3 NUT monitor firmware.

## Module selection

Default is `nut` only.

Environment override:

```bash
POWER_SENTINEL_MODULES=nut python3 backend/bin/power-sentinel-api.py --summary
POWER_SENTINEL_MODULES=nut,proxmox python3 backend/bin/power-sentinel-api.py --summary
```

Config file: `/etc/power-sentinel.json` from `backend/config/power-sentinel.example.json`.

Enabled unobservable or unconfigured modules report `condition: unavailable` and
affect the aggregate condition. Disabled modules produce diagnostic summaries,
but do not expose active pages or affect the aggregate condition.

The Proxmox module is API-only/read-only observability for the CoreS3 Proxmox
Ambient Console page. Minimum config is:

```json
{
  "proxmox": {
    "enabled": true,
    "api_url": "https://pve.example:8006",
    "token_id": "power-sentinel@pve!monitor",
    "token_secret": "CHANGE_ME"
  }
}
```

With the example `CHANGE_ME` secret, the module does not attempt a live API call
and reports `condition: unavailable` with `status: not_observed`. With real
read-only credentials, it reads `/cluster/status` and `/nodes`; API/auth failure
reports `status: api_unavailable`. Token secrets must not appear in payloads.
Node status is interpreted conservatively: `offline` emits `node_down`, any
other non-`online` status emits `node_degraded`, and either makes the Proxmox
module critical.
For online nodes the backend also reads `/nodes/{node}/storage`,
`/nodes/{node}/qemu`, `/nodes/{node}/lxc`, `/nodes/{node}/network`, and
`/nodes/{node}/netstat`. Active/enabled storage entries at or above 85% emit
`storage_warning`; at or above 95% emit `storage_critical`. Guest inventory
summarizes all visible QEMU VMs and LXC containers as running over total. A
partial guest inventory outage emits `guest_down` warning; zero running out of a
non-zero total is critical; `0/0` is healthy.

The Network card auto-selects the physical uplink only when exactly one active
physical interface is visible. If there are multiple candidates, set
`proxmox.network_uplink`. If API data does not expose uplink speed, set
`proxmox.network_uplink_speed_mbps`; speed is not guessed. Network saturation is
latest max inbound/outbound throughput as a percent of uplink speed and only
raises `network_pressure` after sustained threshold crossings.

## Install/update

```bash
sudo scripts/install-power-sentinel.sh --modules nut
```

Dry-run before changing a device:

```bash
sudo scripts/install-power-sentinel.sh --modules nut --dry-run
```

## Services

- `power-sentinel-api.service`
- `power-sentinel-stackflow-unit.service`

No HA publisher services are part of the clean baseline.

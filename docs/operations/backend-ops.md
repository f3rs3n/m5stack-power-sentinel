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
POWER_SENTINEL_MODULES=nut,proxmox,ha python3 backend/bin/power-sentinel-api.py --summary
```

Config file: `/etc/power-sentinel.json` from `backend/config/power-sentinel.example.json`.

Enabled placeholder or unobservable modules report `condition: unavailable` and
affect the aggregate condition. Disabled modules do not affect the aggregate
condition.

The Proxmox module currently has an initial read-only API adapter slice. Minimum
config is:

```json
{
  "proxmox": {
    "api_url": "https://pve.example:8006",
    "token_id": "power-sentinel@pve!monitor",
    "token_secret": "CHANGE_ME",
    "watched_guests": [101]
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
For online nodes the backend also reads `/nodes/{node}/storage`. Active/enabled
storage entries at or above 85% emit `storage_warning`; at or above 95% emit
`storage_critical`.
When `proxmox.watched_guests` is configured, the backend reads
`/nodes/{node}/qemu` for online nodes only. Watched guests must be `running`;
stopped or missing VMIDs emit `watched_guest_down` and make the Proxmox module
critical. Unwatched guests are ignored for condition.

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

# Backend operations

## Local one-shot summary

```bash
python3 backend/bin/power-sentinel-api.py --summary
```

## Run API manually

```bash
python3 backend/bin/power-sentinel-api.py --host 127.0.0.1 --port 8088
curl -fsS http://127.0.0.1:8088/api/v1/summary | python3 -m json.tool
```

## Module selection

Default is `nut` only.

Environment override:

```bash
POWER_SENTINEL_MODULES=nut python3 backend/bin/power-sentinel-api.py --summary
POWER_SENTINEL_MODULES=nut,proxmox,ha python3 backend/bin/power-sentinel-api.py --summary
```

Config file: `/etc/power-sentinel.json` from `backend/config/power-sentinel.example.json`.

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

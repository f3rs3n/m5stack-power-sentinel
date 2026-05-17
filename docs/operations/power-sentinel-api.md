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

## V0 behavior

Before the UPS cable is attached/configured:

- UPS section is deliberately `available=false`, `status=MOCK`, `stale=true`;
- M5Stack section tries to read `/usr/local/bin/m5stack-healthcheck --json`;
- Home Assistant reachability uses TCP `192.168.2.200:8123`;
- MQTT reachability uses TCP `192.168.2.200:1883`;
- Proxmox reachability defaults to TCP `192.168.2.99:8006` (`pve.warpzone.info`) unless overridden.

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

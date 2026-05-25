# Power Sentinel API

Current backend/API reference for the M5Stack Power Sentinel project.

Scope: normalized local state for the autonomous multi-function Linux appliance. The API feeds the CoreS3 dashboards and exposes UPS/NUT, Proxmox, Home Assistant/MQTT/Zigbee2MQTT, network, M5Stack health, and Standard NUT readiness. It is also the future extension point for mini-dashboards and local LLM enrichment/companion features.

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

Shutdown readiness is reported for Standard NUT. Power Sentinel exposes only NUT `upsmon` primary/secondary readiness.

- `shutdown.real_shutdown_owner=upsmon`
- `shutdown.primary_ready`, `shutdown.primary_monitor_active`, `shutdown.secondary_ready`
- `shutdown.nut_upsmon`: explicit local primary upsmon summary, `{armed,state,label}`; the CoreS3 UI should render this as `NUT upsmon: ARMED` or `NUT upsmon: DISARMED` rather than conflating it with telemetry health.
- `shutdown.nut_clients[]`: normalized NUT client inventory/readiness records. The first record is always the local primary (`name=m5stack`, `role=primary`); configured secondary clients follow in inventory order. Each record includes `name`, `host`, `role`, `available`, `package_installed`, `reachable_via_upsc`, `configured`, `connected_as_upsmon`, `armed`, `state`, and optional `discovery_error`/`last_seen_seconds`.
- `shutdown.nut_clients[].state`: one of `armed`, `disarmed`, `connected_as_upsmon`, `reachable_via_upsc`, `configured_not_connected`, `not_configured`, `unknown`, or `unavailable`. `unknown`/`unavailable` records are inventory records only: they do not increment connected/armed counts and do not make `secondary_ready=true`.
- `shutdown.nut_client_summary`: aggregate counts across all records: `total`, `secondary_total`, `connected`, `armed`, `unknown`, and `unavailable`.
- `shutdown.proxmox_nut_client`: the configured Proxmox client selected by Proxmox node name/host; the PVE dashboard NUT pill uses this object only, not aggregate secondary-client counts
- `shutdown.proxmox_nut_client.state`: same enum as `shutdown.nut_clients[].state`
- `shutdown.proxmox_nut_client.package_installed`: whether Proxmox `nut-client`/`upsc` was observed; `null` when not yet discovered
- `shutdown.proxmox_nut_client.reachable_via_upsc`: whether Proxmox can run `upsc homelab_ups@192.168.2.202`; `null` when not yet discovered
- `shutdown.proxmox_nut_client.connected_as_upsmon`: whether the M5Stack NUT server sees Proxmox as a connected NUT upsmon client
- `shutdown.proxmox_nut_client.armed`: true only when Proxmox `nut-monitor` is active and the NUT server sees the Proxmox upsmon client
- `shutdown.would_shutdown`, `shutdown.reason`
- `shutdown.thresholds.battery_charge_low_percent`, `shutdown.thresholds.battery_runtime_low_seconds`

Power Sentinel does not perform custom shutdown orchestration. Real shutdown belongs to NUT `upsmon` primary/secondary roles; enabling `nut-monitor` is the meaningful state transition.

Other sections:

- M5Stack section tries to read `/usr/local/bin/m5stack-healthcheck --json`;
- Home Assistant reachability uses TCP `192.168.2.200:8123`; optional HA update counts use read-only `/api/states` with a long-lived token from root-only config when present;
- MQTT reachability uses TCP `192.168.2.200:1883`;
- Proxmox uses read-only Proxmox API data when a token is configured; no shutdown action is exposed;
- Internet/network status uses the LLM Module Linux default-route table plus a short TCP probe to `1.1.1.1:53` by default, exposed as the `network` summary object for the HOME `NET` indicator;
- MQTT/Zigbee2MQTT status uses `mosquitto_sub` with credentials loaded from root-only config files, not password arguments. The backend reads retained Z2M bridge topics for the `zigbee2mqtt` summary object;
- local LLM inference enrichment is not implemented yet. Current LLM-facing state is health/openai/chat-smoke metadata under `m5stack`.

## MQTT / Zigbee2MQTT read-only integration

The backend can reuse the existing M5Stack HA publisher credentials from:

```text
/etc/m5stack-ha-publish.json
```

or an explicit Power Sentinel MQTT config. `mosquitto-clients` is required on the LLM Module. Per the Mosquitto client man pages, username/password should be placed in client option files rather than passed as `-u`/`-P` command-line arguments; the backend creates temporary `0600` option files under a private `XDG_CONFIG_HOME` for each `mosquitto_sub` call.

Read topics:

```text
homeassistant/status              # best-effort; not retained on this HA install, so often unknown
zigbee2mqtt/bridge/state
zigbee2mqtt/bridge/info
zigbee2mqtt/bridge/devices
```

The `zigbee2mqtt` summary object includes:

- `available`, `severity`, `state`, `base_topic`, `version`
- `coordinator.available`, `coordinator.type`, `coordinator.ieee_address`, `coordinator.firmware`
- `network.channel`, `network.pan_id`
- `devices.total`, `devices.interviewed`, `devices.disabled`
- `problems[]`

No Home Assistant long-lived token is required for the MQTT-first Z2M health view. HA itself is still TCP-checked on `8123`; the MQTT `homeassistant/status` value is exposed in the API when retained/available, otherwise it is `unknown`, but the CoreS3 UI does not show that low-value birth-topic detail. If a Home Assistant long-lived token is available in root-only `/etc/power-sentinel.json`, the backend reads `/api/states` and counts `update.*` entities with state `on`; if the token/read is unavailable, `homeassistant.updates.available_count` falls back to `0` so the display can show `Updates 0` rather than an ambiguous `n/a`.

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
- `/api2/json/nodes/{node}/qemu/{vmid}/status/current` for running VMs, best-effort enrichment
- `/api2/json/nodes/{node}/qemu/{vmid}/agent/get-fsinfo` for running VMs with guest agent, best-effort disk-usage enrichment
- `/api2/json/nodes/{node}/lxc`
- `/api2/json/nodes/{node}/disks/zfs` best-effort
- `/api2/json/nodes/{node}/disks/list` best-effort
- `/api2/json/nodes/{node}/storage` best-effort for Total Node Capacity and aggregate storage usage across enabled/active node storages
- `/api2/json/nodes/{node}/network` best-effort for active interface names

No SSH, no shutdown action, no `smartctl` custom script, and no expected-state model are used in this V1 read-only step.

The Proxmox role used by `power-sentinel@pve!readonly` is still read-only, but includes `VM.GuestAgent.Audit` in addition to `Sys.Audit`, `VM.Audit`, and `Datastore.Audit`. This allows `get-fsinfo` without granting guest-agent file read/write or unrestricted command execution. Assign this role to both the owning user and the privilege-separated token; Proxmox token permissions are the intersection of both ACL sets.

VM metric caveats implemented in the backend:

- running VM RAM is taken from `/status/current` when available, because the list endpoint can report `mem > maxmem` for HAOS;
- Proxmox VM list/status can report `disk=0` even for an active disk, so a VM `disk=0` is not treated as reliable usage unless guest-agent fsinfo supplies real filesystem numbers;
- guest-agent fsinfo filters pseudo/read-only filesystems such as `tmpfs`, `squashfs`, `overlay`, and HAOS `erofs`; for HAOS the useful HDD metric currently comes from `/mnt/data`, not `/`.

The `proxmox` summary object includes:

- `available`, `severity`, `node`, `node_status`, `api_latency_ms`
- `cpu_percent`, `ram_percent`, `cpu_temp_c`, `storage_percent`, `storage_total_bytes` (`/nodes/{node}/storage` aggregate Total Node Capacity)
- `active_network_interfaces[]` from active non-loopback PVE network devices
- `zfs.status`, `zfs.pools[]`
- `smart.status`, `smart.failing_count`, `smart.warning_count`
- `vm.running_count`, `vm.running_names[]`, `vm.running_items[]` with running-only CPU/RAM/disk mini-metrics. VM RAM is enriched from `/status/current`; VM disk usage is enriched from QEMU guest-agent fsinfo when available and otherwise remains `null` rather than showing a misleading `0%`.
- `lxc.running_count`, `lxc.running_names[]`, `lxc.running_items[]` with running-only CPU/RAM/disk mini-metrics when Proxmox exposes them
- `shutdown_state=disarmed` as a compatibility/read-only display field only. Real host shutdown is Standard NUT secondary `upsmon`.
- `problems[]`

Environment overrides:

```text
POWER_SENTINEL_BIND
POWER_SENTINEL_PORT
POWER_SENTINEL_HEALTHCHECK
POWER_SENTINEL_UPSC
POWER_SENTINEL_UPS
POWER_SENTINEL_HA_HOST
POWER_SENTINEL_HA_PORT
POWER_SENTINEL_HA_SCHEME
POWER_SENTINEL_HA_TOKEN
POWER_SENTINEL_MQTT_HOST
POWER_SENTINEL_PROXMOX_HOST
POWER_SENTINEL_PROXMOX_PORT
POWER_SENTINEL_PROXMOX_NODE
POWER_SENTINEL_PROXMOX_TOKEN_ID
POWER_SENTINEL_PROXMOX_TOKEN_SECRET
POWER_SENTINEL_PROXMOX_VERIFY_SSL
POWER_SENTINEL_NETWORK_PROBE_HOST
POWER_SENTINEL_NETWORK_PROBE_PORT
POWER_SENTINEL_NUT_CLIENTS_FILE
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

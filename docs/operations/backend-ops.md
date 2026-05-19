# Backend operations

## Existing deployed paths on the M5Stack LLM Module

```text
/usr/local/bin/m5stack-healthcheck
/usr/local/bin/m5stack-ha-publish
/usr/local/bin/m5stack-ups-detect
/usr/local/bin/power-sentinel-api
/usr/local/bin/power-sentinel-stackflow-unit
/etc/m5stack-ha-publish.json          # root-only, contains MQTT credentials, do not commit
/etc/systemd/system/m5stack-ha-publish.service
/etc/systemd/system/m5stack-ha-publish.timer
/etc/systemd/system/m5stack-ha-publish-chat.service
/etc/systemd/system/m5stack-ha-publish-chat.timer
/etc/systemd/system/power-sentinel-api.service
/etc/systemd/system/power-sentinel-stackflow-unit.service
```

## Useful commands

```bash
ssh root@192.168.2.202 '/usr/local/bin/m5stack-healthcheck'
ssh root@192.168.2.202 '/usr/local/bin/m5stack-healthcheck --json'
ssh root@192.168.2.202 '/usr/local/bin/m5stack-healthcheck --chat'
ssh root@192.168.2.202 '/usr/local/bin/m5stack-ups-detect'
ssh root@192.168.2.202 'systemctl list-timers "m5stack-ha-publish*" --no-pager'
ssh root@192.168.2.202 'curl -s http://127.0.0.1:8088/api/v1/summary'
ssh root@192.168.2.202 'systemctl status power-sentinel-api power-sentinel-stackflow-unit llm-sys --no-pager'
ssh root@192.168.2.202 'journalctl -u power-sentinel-stackflow-unit -f'
```

## MQTT / HA / Zigbee2MQTT

`mosquitto-clients` is installed on the LLM Module. The existing M5Stack HA publisher config remains the credential source:

```text
/etc/m5stack-ha-publish.json          # 0600 root:root, contains MQTT credentials
```

`m5stack-ha-publish` now shells out to `mosquitto_pub` instead of using a hand-rolled MQTT packet client. To avoid leaking passwords through argv, it creates a temporary `XDG_CONFIG_HOME/mosquitto_pub` option file with `0600` permissions and sends payloads via stdin (`mosquitto_pub -s`). The Power Sentinel API uses the same option-file pattern with `mosquitto_sub`.

Useful read-only probes from the LLM Module:

```bash
# Use the script/API wrappers for real use; do not put MQTT passwords in shell history.
/usr/local/bin/m5stack-ha-publish --no-discovery
curl -s 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
```

Live observations:

```text
homeassistant/status: timed out / not retained -> summary homeassistant.status=unknown
zigbee2mqtt/bridge/state: online
zigbee2mqtt.version: 2.10.1
coordinator.type: EmberZNet
coordinator.firmware: 7.4.5 [GA]
zigbee devices: total=29 interviewed=29 disabled=0
```

## CoreS3 StackFlow transport

The CoreS3 uses the internal stacked UART, but it does not own `/dev/ttyS1` directly. The vendor `llm_sys` service remains the UART owner and routes Power Sentinel requests to the custom StackFlow unit over ZeroMQ.

```text
CoreS3 -> llm_sys UART: {"request_id":"ps-N","work_id":"sentinel","action":"summary","object":"None","data":"None"}
llm_sys -> custom unit: ipc:///tmp/rpc.sentinel
custom unit -> API:     http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1
custom unit -> llm_sys: StackFlow JSON response, object=power-sentinel.summary.v1
llm_sys -> CoreS3 UART: same StackFlow JSON response
```

Why StackFlow instead of the old direct serial bridge:

- `llm_sys` already owns `/dev/ttyS1` on the LLM Module.
- A parallel reader/writer on `/dev/ttyS1` risks corrupting frames and breaking future vendor voice/assistant features.
- StackFlow lets Power Sentinel coexist with the vendor runtime using the documented routing model for `work_id` units.
- The CoreS3 keeps the same physical internal M-Bus/UART path, but speaks official StackFlow JSON at 115200 8N1.

Install/update on the LLM Module:

```bash
install -m 755 backend/bin/power-sentinel-api.py /usr/local/bin/power-sentinel-api
install -m 755 backend/bin/power-sentinel-stackflow-unit.py /usr/local/bin/power-sentinel-stackflow-unit
install -m 644 backend/systemd/power-sentinel-api.service /etc/systemd/system/power-sentinel-api.service
install -m 644 backend/systemd/power-sentinel-stackflow-unit.service /etc/systemd/system/power-sentinel-stackflow-unit.service
systemctl daemon-reload
systemctl enable --now power-sentinel-api.service power-sentinel-stackflow-unit.service
```

Quick StackFlow dependency check from the LLM Module:

```bash
python3 - <<'PY'
import json, socket, time
msg = {"request_id":"ops-check","work_id":"sentinel","action":"summary","object":"None","data":"None"}
s = socket.socket(); s.settimeout(5)
s.connect(("127.0.0.1", 10001))
s.sendall(json.dumps(msg, separators=(",", ":")).encode() + b"\n")
time.sleep(0.2)
print(s.recv(4096).decode(errors="replace"))
s.close()
PY
```

## NUT current state

The USB OTG cable is connected and the UPS is detected by the LLM Module:

```text
Bus 001 Device 003: ID 051d:0002 American Power Conversion Uninterruptible Power Supply
Product: Back-UPS ES 850G2 FW:938.a2 .I USB FW:a2
Serial: 5B2350TD6030
NUT driver: usbhid-ups
UPS name: homelab_ups
```

Current service policy:

```text
nut-server: enabled/active
nut-driver: static/active
nut-monitor: disabled/inactive
/etc/nut/nut.conf: MODE=netserver
```

`nut-server` is enabled so UPS telemetry survives reboot. `nut-monitor` remains disabled for now. The approved real shutdown strategy is Standard NUT: `upsmon` primary on the M5Stack LLM Module and `upsmon` secondary on Proxmox. Power Sentinel remains a dashboard/dry-run observer; it does not orchestrate Proxmox shutdown via API.

Useful NUT commands:

```bash
ssh root@192.168.2.202 'upsc homelab_ups@localhost'
ssh root@192.168.2.202 'systemctl status nut-server nut-driver nut-monitor --no-pager'
ssh root@192.168.2.202 'ss -ltnp | grep :3493 || true'
ssh root@192.168.2.202 '/usr/local/bin/m5stack-ups-detect'
```

Current sanitized config templates in the repo:

```text
backend/config/nut.conf.example
backend/config/ups.conf.example
backend/config/upsd.conf.example
backend/config/upsd.users.standard-nut.example
backend/config/upsmon.primary.example
backend/config/upsmon.secondary-proxmox.example
backend/config/proxmox-nut-readiness.example.json
```

The live API summary should now show:

```text
ups.available: true
ups.status: OL
ups.status_label: Online
severity: ok
shutdown.strategy: standard-nut
shutdown.mode: dry-run
shutdown.real_shutdown_owner: upsmon
shutdown.proxmox_api_orchestration: false
shutdown.proxmox_secondary.state: not_configured | reachable_via_upsc | connected_as_upsmon | armed
```

## Proxmox NUT secondary readiness

Point 8 is intentionally non-destructive. It discovers whether Proxmox can become a Standard NUT secondary, but it does not enable `nut-monitor`, does not edit Proxmox config, and does not trigger FSD/shutdown.

Discovery attempted from Hermes:

```text
ssh root@192.168.2.99: blocked by SSH auth / host-key state
ssh martino@192.168.2.99: blocked by SSH auth
ssh root@pve.warpzone.info: blocked by SSH auth
ssh martino@pve.warpzone.info: blocked by SSH auth
```

Discovery confirmed from the M5Stack LLM Module:

```text
upsc homelab_ups@localhost ups.status: OL
upsd listens on 127.0.0.1:3493 and 192.168.2.202:3493
no active TCP client on :3493 at discovery time
/etc/nut/upsd.users still contains only examples/comments, no active upsmon users yet
```

Because Hermes currently cannot SSH into Proxmox, package/config checks on Proxmox remain unknown until Martino either grants SSH access or runs the commands manually on Proxmox. The backend exposes a conservative readiness object:

```json
"shutdown": {
  "proxmox_secondary": {
    "target_host": "192.168.2.99",
    "package_installed": null,
    "reachable_via_upsc": null,
    "configured": false,
    "connected_as_upsmon": false,
    "armed": false,
    "state": "not_configured"
  }
}
```

State meanings:

- `not_configured`: no readiness file and no observed Proxmox upsmon client.
- `reachable_via_upsc`: Proxmox has `upsc`/`nut-client` and can read `homelab_ups@192.168.2.202`, but upsmon is not connected.
- `connected_as_upsmon`: the M5Stack NUT server sees a Proxmox/PVE client connection on port 3493; not necessarily armed.
- `armed`: Proxmox readiness says `monitor_active=true` and the M5Stack NUT server sees the Proxmox upsmon client.

Optional manual readiness file on the LLM Module:

```text
/etc/power-sentinel-proxmox-nut-readiness.json
```

Sanitized example:

```json
{
  "target_host": "192.168.2.99",
  "package_installed": true,
  "upsc_reachable": true,
  "config_present": false,
  "monitor_active": false
}
```

Manual Proxmox commands for Martino, still read-only/non-destructive:

```bash
command -v upsc || true
dpkg-query -W nut-client nut-server nut 2>/dev/null || true
timeout 3 bash -lc '</dev/tcp/192.168.2.202/3493' && echo tcp_3493_ok || echo tcp_3493_fail
upsc homelab_ups@192.168.2.202 ups.status
systemctl is-enabled nut-monitor 2>/dev/null || true
systemctl is-active nut-monitor 2>/dev/null || true
```

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

`nut-server` is enabled so UPS telemetry survives reboot. `nut-monitor` remains disabled until a deliberate shutdown policy is designed and approved.

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
```

The live API summary should now show:

```text
ups.available: true
ups.status: OL
ups.status_label: Online
severity: ok
```

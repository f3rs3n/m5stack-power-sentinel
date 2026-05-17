# Backend operations

## Existing deployed paths on the M5Stack LLM Module

```text
/usr/local/bin/m5stack-healthcheck
/usr/local/bin/m5stack-ha-publish
/usr/local/bin/m5stack-ups-detect
/usr/local/bin/power-sentinel-api
/usr/local/bin/power-sentinel-serial-bridge
/etc/m5stack-ha-publish.json          # root-only, contains MQTT credentials, do not commit
/etc/systemd/system/m5stack-ha-publish.service
/etc/systemd/system/m5stack-ha-publish.timer
/etc/systemd/system/m5stack-ha-publish-chat.service
/etc/systemd/system/m5stack-ha-publish-chat.timer
/etc/systemd/system/power-sentinel-api.service
/etc/systemd/system/power-sentinel-serial-bridge.service
```

## Useful commands

```bash
ssh root@192.168.2.202 '/usr/local/bin/m5stack-healthcheck'
ssh root@192.168.2.202 '/usr/local/bin/m5stack-healthcheck --json'
ssh root@192.168.2.202 '/usr/local/bin/m5stack-healthcheck --chat'
ssh root@192.168.2.202 '/usr/local/bin/m5stack-ups-detect'
ssh root@192.168.2.202 'systemctl list-timers "m5stack-ha-publish*" --no-pager'
ssh root@192.168.2.202 'curl -s http://127.0.0.1:8088/api/v1/summary'
ssh root@192.168.2.202 'systemctl status power-sentinel-api power-sentinel-serial-bridge --no-pager'
journalctl -u power-sentinel-serial-bridge -f
```

## CoreS3 serial bridge

The serial bridge is the Linux-side endpoint for the final internal CoreS3 transport.

```text
CoreS3 -> LLM Module: PS1 PING <token>
LLM Module -> CoreS3: PS1 PONG <token>

CoreS3 -> LLM Module: PS1 GET summary
LLM Module -> CoreS3: PS1 OK <json-byte-length>
LLM Module -> CoreS3: {...power-sentinel.summary.v1...}
```

Default runtime settings:

```text
serial port: /dev/ttyS1
baud:        115200
summary URL: http://127.0.0.1:8088/api/v1/summary
```

Install/update on the LLM Module:

```bash
install -m 755 backend/bin/power-sentinel-serial-bridge.py /usr/local/bin/power-sentinel-serial-bridge
install -m 644 backend/systemd/power-sentinel-serial-bridge.service /etc/systemd/system/power-sentinel-serial-bridge.service
systemctl daemon-reload
systemctl enable --now power-sentinel-serial-bridge.service
```

Quick API dependency check before starting the bridge:

```bash
/usr/local/bin/power-sentinel-serial-bridge --test-fetch
```

## NUT pre-cable state

NUT packages are installed, but services are intentionally disabled until the UPS is attached and configured:

```text
nut-server: disabled/inactive
nut-monitor: disabled/inactive
nut-driver: static/inactive
/etc/nut/nut.conf: MODE=none
```

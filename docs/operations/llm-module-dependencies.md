# LLM Module dependencies

Clean NUT Monitor baseline dependencies for a fresh M5Stack LLM Module.

## Debian/Ubuntu packages

Required:

```bash
apt-get update
apt-get install -y nut nut-server nut-client curl python3 python3-zmq
```

Optional diagnostics:

```bash
apt-get install -y usbutils iproute2
```

## Installed project files

Installed by `scripts/install-power-sentinel.sh --modules nut`:

```text
/usr/local/bin/power-sentinel-api
/usr/local/bin/power-sentinel-stackflow-unit
/usr/local/bin/m5stack-ups-detect
/etc/systemd/system/power-sentinel-api.service
/etc/systemd/system/power-sentinel-stackflow-unit.service
/etc/power-sentinel.json
```

Runtime NUT config remains under `/etc/nut/` and should be managed deliberately, not overwritten blindly by the project installer.

## Smoke checks

```bash
/usr/local/bin/m5stack-ups-detect
/usr/bin/upsc homelab_ups@localhost
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1'
python3 -m json.tool /etc/power-sentinel.json
systemctl is-active power-sentinel-api.service
systemctl is-active power-sentinel-stackflow-unit.service
```

## Future modules

`proxmox` and `ha` are installer-recognized placeholders only. Add their package/config dependencies here only when the module backend is reintroduced with tests.

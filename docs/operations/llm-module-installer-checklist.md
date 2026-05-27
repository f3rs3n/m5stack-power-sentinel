# LLM Module Installer Checklist

Purpose: implementation checklist for future `backend/install/install-llm-module.sh`, derived from the dependency inventory and cross-checked against current files under `backend/`.

## Goals

- [ ] Install required apt packages and Python dependencies.
- [ ] Install backend scripts and systemd units.
- [ ] Install sanitized config templates only.
- [ ] Preserve/back up existing secret-bearing files.
- [ ] Validate the local API and StackFlow path.
- [ ] Print clear remaining manual steps.

## Config and secret handling

- [ ] Use sanitized placeholders for site-specific hosts, tokens, MQTT users/passwords, HA tokens, Proxmox tokens, NUT passwords, UPS serials, and LAN addresses.
- [ ] Do not overwrite `/etc/power-sentinel.json`, `/etc/m5stack-ha-publish.json`, or `/etc/nut/*` unless an explicit force/update flag is supplied.
- [ ] Create timestamped backups before replacing any existing root-owned config file.

## Package install

- [ ] `python3`, `python3-venv`, `python3-pip`
- [ ] `nut`, `nut-server`, `nut-client`, `nut-cgi`
- [ ] `mosquitto-clients`
- [ ] `curl`, `jq`, `iproute2`

## Files and units

- [ ] Backend scripts under `/opt/power-sentinel/` or the chosen project path.
- [ ] `power-sentinel-api.service`.
- [ ] `power-sentinel-stackflow-unit.service`.
- [ ] Optional MQTT/HA publisher service/timer files.
- [ ] Sanitized NUT templates for UPS server config and client inventory display.

## Validation

- [ ] `systemctl status llm-sys nut-server power-sentinel-api power-sentinel-stackflow-unit --no-pager`
- [ ] `curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool >/dev/null`
- [ ] `upsc homelab_ups@localhost ups.status`
- [ ] Confirm the API includes `ups`, `nut`, `proxmox`, `homeassistant`, `mqtt`, `zigbee2mqtt`, `network`, `m5stack`, and `problems`.
- [ ] Confirm `nut.connected_client_count` is present, even if zero.

## Output

After validation, print:

- service status summary;
- paths to installed config files;
- which config files were created vs preserved;
- missing secrets/config values still required;
- CoreS3 flash/build pointer for the firmware side.

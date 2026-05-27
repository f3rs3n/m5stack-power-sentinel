# LLM Module Dependency Inventory

This file records the packages, config files, and smoke checks needed to reproduce the Power Sentinel backend on a fresh M5Stack LLM Module.

## Base assumptions

- Ubuntu 22.04 style userspace on the LLM Module.
- Python 3.10+.
- Vendor StackFlow / `llm_sys` runtime remains installed.
- Root-only runtime config files are used for secrets.

## Apt packages

Expected package groups:

```text
python3 python3-venv python3-pip
nut nut-server nut-client nut-cgi
mosquitto-clients
curl jq iproute2
```

## Python packages

Backend scripts currently use stdlib plus project-pinned packages where present. Verify with the project test runner and the live API smoke check.

## Runtime config files

```text
/etc/m5stack-ha-publish.json          # MQTT/HA publisher credentials/config
/etc/power-sentinel.json              # summary API config: HA, MQTT, Proxmox token, network targets, etc.
/etc/power-sentinel-nut-clients.json  # optional NUT connected-host display inventory
/etc/nut/ups.conf                     # UPS driver config
/etc/nut/upsd.conf                    # NUT server listener config
/etc/nut/upsd.users                   # NUT server user config
```

## Smoke checks

```bash
python3 --version
command -v upsc upsdrvctl upsd nut-scanner mosquitto_pub mosquitto_sub curl ss
systemctl status llm-sys nut-server power-sentinel-api power-sentinel-stackflow-unit --no-pager
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool >/dev/null
upsc homelab_ups@localhost ups.status
```

## Installer safety notes

- Never overwrite secret-bearing `/etc/*.json` or `/etc/nut/*` without a timestamped backup and explicit flag.
- Never install a parallel serial bridge on `/dev/ttyS1`; StackFlow via `llm_sys` remains the primary transport.
- Keep public-release docs sanitized if this repo is prepared for publication.

# LLM Module dependency inventory

Status: living operational inventory for reproducing the Power Sentinel backend on a fresh M5Stack LLM Module. This is not yet an installer script; the roadmap now includes turning this inventory into an idempotent installer.

Target currently verified on the deployed module:

```text
OS: Ubuntu 22.04.5 LTS
Python runtime: Python 3.10.x
Architecture: arm64 on M5Stack LLM Module
Vendor runtime required: llm_sys / StackFlow services already installed and working
```

## Required apt packages

Current verified package set on the live LLM Module:

```text
ca-certificates        20240203~22.04.1
curl                   7.81.0-1ubuntu1.24
iproute2               5.15.0-1ubuntu2.1
mosquitto-clients      2.0.11-1ubuntu1.2
nut                    2.7.4-14ubuntu2
nut-client             2.7.4-14ubuntu2
nut-server             2.7.4-14ubuntu2
openssh-server         1:8.9p1-3ubuntu0.15
python3                3.10.x / Ubuntu 22.04 package
python3-zmq            22.3.0-1build1
systemd                249.11-0ubuntu3.20
```

Minimum install command for a fresh Ubuntu-based LLM Module should start from:

```bash
apt-get update
apt-get install -y \
  ca-certificates \
  curl \
  iproute2 \
  mosquitto-clients \
  nut \
  nut-client \
  nut-server \
  openssh-server \
  python3 \
  python3-zmq
```

Notes:

- `python3-zmq` is required by `power-sentinel-stackflow-unit` to bind `ipc:///tmp/rpc.sentinel` and send callback responses to `llm_sys`.
- `mosquitto-clients` is required for MQTT/HA/Zigbee2MQTT probes and publishing. Credentials must be read from root-only config files and passed via temporary Mosquitto option files, not argv.
- `nut`, `nut-server`, and `nut-client` are all intentionally installed on the LLM Module. The server owns the USB UPS; client tools such as `upsc` are also used locally by scripts/API.
- `iproute2` provides `ss` and route inspection used by operations/probes.
- `curl` is useful for local smoke checks even though the Python API uses stdlib HTTP clients.

## Required vendor/runtime assumptions

Power Sentinel does not replace the vendor StackFlow runtime. A new stack must have:

```text
llm_sys service active
StackFlow TCP API on 127.0.0.1:10001 reachable
/dev/ttyS1 owned by llm_sys for CoreS3 UART routing
```

Power Sentinel integrates through a custom StackFlow unit:

```text
CoreS3 UART -> llm_sys -> ipc:///tmp/rpc.sentinel -> power-sentinel-stackflow-unit -> local summary API
```

Do not install a parallel `/dev/ttyS1` bridge on a fresh stack.

## Installed Power Sentinel files

A fresh install must place these executable scripts:

```text
/usr/local/bin/m5stack-healthcheck
/usr/local/bin/m5stack-ha-publish
/usr/local/bin/m5stack-ups-detect
/usr/local/bin/power-sentinel-api
/usr/local/bin/power-sentinel-stackflow-unit
```

And these systemd units/timers:

```text
/etc/systemd/system/m5stack-ha-publish.service
/etc/systemd/system/m5stack-ha-publish.timer
/etc/systemd/system/m5stack-ha-publish-chat.service
/etc/systemd/system/m5stack-ha-publish-chat.timer
/etc/systemd/system/power-sentinel-api.service
/etc/systemd/system/power-sentinel-stackflow-unit.service
```

NUT config/templates live under `backend/config/` in sanitized form. Runtime files under `/etc/nut/` and `/etc/power-sentinel*.json` may contain secrets and must not be committed.

## Runtime config files to create manually until the installer exists

Root-only, secret-bearing files:

```text
/etc/m5stack-ha-publish.json          # MQTT/HA publisher credentials/config
/etc/power-sentinel.json              # summary API config: HA, MQTT, Proxmox token, network targets, etc.
/etc/power-sentinel-nut-clients.json  # optional NUT client readiness inventory
```

Expected permissions:

```bash
chown root:root /etc/m5stack-ha-publish.json /etc/power-sentinel.json /etc/power-sentinel-nut-clients.json
chmod 0600 /etc/m5stack-ha-publish.json /etc/power-sentinel.json /etc/power-sentinel-nut-clients.json
```

## Proxmox side dependency/ACL

For VM mini-card disk usage, the Proxmox API token needs read-only guest-agent audit permission:

```text
Role: PowerSentinelReadOnly
Privileges: Sys.Audit VM.Audit Datastore.Audit VM.GuestAgent.Audit
Token: power-sentinel@pve!readonly
ACL: assign the role to both the owning user and the privilege-separated token, with propagation
```

This enables `qemu/{vmid}/agent/get-fsinfo` without granting unrestricted guest-agent command execution.

## Post-install verification checklist

Run on the LLM Module:

```bash
python3 --version
python3 - <<'PY'
import zmq
print('pyzmq', zmq.__version__)
PY
command -v upsc upsdrvctl upsd upsmon nut-scanner mosquitto_pub mosquitto_sub curl ss
systemctl status llm-sys nut-server power-sentinel-api power-sentinel-stackflow-unit --no-pager
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool >/dev/null
```

StackFlow smoke check:

```bash
python3 - <<'PY'
import json, socket, time
msg = {"request_id":"install-check","work_id":"sentinel","action":"summary","object":"None","data":"None"}
s = socket.socket(); s.settimeout(5)
s.connect(("127.0.0.1", 10001))
s.sendall(json.dumps(msg, separators=(",", ":")).encode() + b"\n")
time.sleep(0.2)
print(s.recv(4096).decode(errors="replace")[:1000])
s.close()
PY
```

## Installer roadmap

Create an idempotent installer script after this inventory stabilizes. Proposed path:

```text
backend/install/install-llm-module.sh
```

The installer should:

1. verify target OS/architecture and refuse unsupported systems unless `--force` is given;
2. install apt dependencies;
3. install Power Sentinel scripts and systemd units from the repo;
4. create sanitized config stubs only when runtime config files do not exist;
5. set root-only permissions on secret config files;
6. enable/start `nut-server`, `power-sentinel-api`, and `power-sentinel-stackflow-unit`;
7. leave `nut-monitor` disabled unless an explicit `--arm-nut-monitor` flag is provided;
8. run the post-install verification checklist;
9. print remaining manual steps: UPS cable, real MQTT/HA/Proxmox tokens, Proxmox ACL, CoreS3 firmware flash.

Safety constraints for the future installer:

- Never overwrite secret-bearing `/etc/*.json` or `/etc/nut/*` without a timestamped backup and explicit flag.
- Never enable `nut-monitor` or trigger FSD/shutdown by default.
- Never install a parallel serial bridge on `/dev/ttyS1`; StackFlow via `llm_sys` remains the primary transport.
- Keep public-release docs sanitized if this repo is ever prepared for publication.

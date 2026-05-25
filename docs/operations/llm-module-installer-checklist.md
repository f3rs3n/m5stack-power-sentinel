# Fresh M5Stack LLM Module installer checklist

Source inventory: `docs/operations/llm-module-dependencies.md`.

Purpose: implementation checklist for future `backend/install/install-llm-module.sh`, derived from the dependency inventory and cross-checked against current files under `backend/`. Keep the installer idempotent, sanitized, and safe for a module that observes Standard NUT readiness rather than orchestrating shutdown.

## Scope and safety invariants

- [ ] Do not commit, print, or generate real secrets in repo files, logs, docs, argv, or systemd unit files.
- [ ] Use sanitized placeholders for site-specific hosts, tokens, MQTT users/passwords, HA tokens, Proxmox tokens, NUT passwords, UPS serials, and LAN addresses.
- [ ] Preserve the architecture principle: Power Sentinel observes Standard NUT state/readiness and local summary state; Standard NUT `upsmon` remains the only shutdown owner when deliberately armed.
- [ ] Do not enable `nut-monitor`, trigger FSD, call shutdown, or attempt killpower by default.
- [ ] Do not call HA Supervisor APIs in V1; use local config plus MQTT/HA endpoints supplied by the operator.
- [ ] Do not replace vendor StackFlow or install a competing serial bridge on `/dev/ttyS1`.

## Target and preflight

- [ ] Verify target OS is Ubuntu 22.04.x, currently verified as Ubuntu 22.04.5 LTS.
- [ ] Verify architecture is `arm64` on an M5Stack LLM Module.
- [ ] Verify Python runtime is Python 3.10.x from Ubuntu 22.04 packages.
- [ ] Refuse unsupported OS/architecture unless an explicit `--force` flag is provided.
- [ ] Verify vendor StackFlow runtime is already installed and working before changing the system:
  - [ ] Canonical systemd unit name for current repo units is `llm-sys.service` with hyphen. Current `backend/systemd/*.service` files use `After=... llm-sys.service` / `Wants=... llm-sys.service`.
  - [ ] Treat `llm_sys` with underscore as the vendor process/component name used in docs/comments and for `/dev/ttyS1` ownership descriptions, not as the canonical systemd unit name unless detected on a specific image.
  - [ ] Installer preflight should check `llm-sys.service` first, and may fall back to probing `llm_sys.service` only for diagnostics/backward compatibility. Do not install or rename vendor units.
  - [ ] StackFlow TCP API is reachable on `127.0.0.1:10001`.
  - [ ] `/dev/ttyS1` remains owned/managed by the vendor StackFlow path for CoreS3 UART routing.
- [ ] Expected transport path remains: `CoreS3 UART -> llm_sys/StackFlow -> ipc:///tmp/rpc.sentinel -> power-sentinel-stackflow-unit -> local summary API`.

## OS packages

Run `apt-get update`, then install these packages:

- [ ] `ca-certificates` verified version on live module: `20240203~22.04.1`.
- [ ] `curl` verified version: `7.81.0-1ubuntu1.24`; used for smoke checks.
- [ ] `iproute2` verified version: `5.15.0-1ubuntu2.1`; provides `ss` and route inspection.
- [ ] `mosquitto-clients` verified version: `2.0.11-1ubuntu1.2`; used for MQTT/HA/Zigbee2MQTT probes and publishing.
- [ ] `nut` verified version: `2.7.4-14ubuntu2`.
- [ ] `nut-client` verified version: `2.7.4-14ubuntu2`; local scripts/API use client tools such as `upsc`.
- [ ] `nut-server` verified version: `2.7.4-14ubuntu2`; the LLM Module owns the USB UPS.
- [ ] `openssh-server` verified version: `1:8.9p1-3ubuntu0.15`.
- [ ] `python3` Ubuntu 22.04 Python 3.10.x package.
- [ ] `python3-zmq` verified version: `22.3.0-1build1`; required by `power-sentinel-stackflow-unit` for `ipc:///tmp/rpc.sentinel` and `llm_sys` callbacks.
- [ ] `systemd` verified version on live module: `249.11-0ubuntu3.20`; normally already present, but systemd units/timers require it.

Minimum package command from source:

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

## Python dependencies

- [ ] Use system Python 3.10.x.
- [ ] Install/verify `pyzmq` via the Ubuntu `python3-zmq` package, not an embedded vendored dependency.
- [ ] Validate with:

```bash
python3 --version
python3 - <<'PY'
import zmq
print('pyzmq', zmq.__version__)
PY
```

## Power Sentinel executables to install

Install executable scripts from the repo to `/usr/local/bin/`. Current repo sources are in `backend/bin/*.py`; installed commands should omit the `.py` suffix.

- [ ] `/usr/local/bin/m5stack-ha-publish` from `backend/bin/m5stack-ha-publish.py`.
- [ ] `/usr/local/bin/m5stack-ups-detect` from `backend/bin/m5stack-ups-detect.py`.
- [ ] `/usr/local/bin/power-sentinel-api` from `backend/bin/power-sentinel-api.py`.
- [ ] `/usr/local/bin/power-sentinel-stackflow-unit` from `backend/bin/power-sentinel-stackflow-unit.py`.
- [ ] `/usr/local/bin/m5stack-healthcheck` is required at runtime by both `m5stack-ha-publish.py` and `power-sentinel-api.py`, but no matching `backend/bin/m5stack-healthcheck.py` exists in the current repo inventory. Treat this as a blocking implementation gap for the installer: provide the source, vendor it explicitly, or fail preflight with a clear message.
- [ ] Set executable mode on installed commands.
- [ ] Installer validation must run `command -v` for all installed commands, including `m5stack-healthcheck`, because services depend on it.

## systemd units and timers

Install these units from `backend/systemd/` to `/etc/systemd/system/`:

- [ ] `m5stack-ha-publish.service`.
- [ ] `m5stack-ha-publish.timer`.
- [ ] `m5stack-ha-publish-chat.service`.
- [ ] `m5stack-ha-publish-chat.timer`.
- [ ] `power-sentinel-api.service`.
- [ ] `power-sentinel-stackflow-unit.service`.
- [ ] Before enabling services, verify installed unit files do not embed site-specific private IPs/tokens. Site values should come from root-only config files or sanitized environment overrides.
- [ ] Current repo unit dependency spelling is `llm-sys.service`; keep that spelling in validation unless future backend units are changed.
- [ ] Run `systemctl daemon-reload` after installing or changing unit files.
- [ ] Enable/start core services only by default: `nut-server`, `power-sentinel-api`, and `power-sentinel-stackflow-unit`.
- [ ] Leave `nut-monitor` disabled unless the installer receives an explicit `--arm-nut-monitor` flag and the operator has intentionally installed/tested primary `upsmon.conf`.
- [ ] Install HA publisher service/timer files, but do not enable `m5stack-ha-publish.timer` or `m5stack-ha-publish-chat.timer` by default unless an explicit option such as `--enable-ha-publisher` is supplied and `/etc/m5stack-ha-publish.json` has non-placeholder MQTT config.
- [ ] If publisher timers are enabled, start timers rather than one-shot services directly, and validate with `systemctl is-enabled --quiet m5stack-ha-publish.timer` / `systemctl is-active --quiet m5stack-ha-publish.timer`.

## Config files and templates

Use sanitized repo templates only. Never commit or generate real secrets into the repo.

Repo-local sanitized templates under `backend/config/`:

- [ ] `m5stack-ha-publish.example.json` -> template for `/etc/m5stack-ha-publish.json`.
- [ ] `power-sentinel.example.json` -> template for `/etc/power-sentinel.json`.
- [ ] `nut-clients.example.json` -> template for `/etc/power-sentinel-nut-clients.json`.

NUT template mapping:

- [ ] `nut.conf.example` -> `/etc/nut/nut.conf`; default mode for the USB-attached LLM Module is `MODE=netserver`.
- [ ] `ups.conf.example` -> `/etc/nut/ups.conf`; UPS driver config, may need operator-supplied model/serial after detection.
- [ ] `upsd.conf.example` -> `/etc/nut/upsd.conf`; listener config. Keep placeholders sanitized, e.g. localhost plus `<LLM_MODULE_LAN_IP>` when exposing NUT to trusted clients.
- [ ] `upsd.users.standard-nut.example` -> `/etc/nut/upsd.users`; Standard NUT users. NUT 2.7.4 uses legacy `master`/`slave` keywords, documented in project terms as primary/secondary roles.
- [ ] `upsmon.primary.example` -> `/etc/nut/upsmon.conf` on the LLM Module only when the explicit `--arm-nut-monitor` flow is used. Do not enable by default.
- [ ] `upsmon.secondary.example` is a sanitized template for other client hosts, not a default LLM Module install target. The installer may print/copy it as operator reference, but should not install it over `/etc/nut/upsmon.conf` on the LLM Module.

Root-only, secret-bearing runtime files:

- [ ] `/etc/m5stack-ha-publish.json` for MQTT/HA publisher credentials/config.
- [ ] `/etc/power-sentinel.json` for summary API config: HA, MQTT, Proxmox token, network targets, etc.
- [ ] `/etc/power-sentinel-nut-clients.json` for optional NUT client readiness inventory.
- [ ] `/etc/nut/*` runtime files may also contain credentials.
- [ ] Create sanitized stubs only when these files do not already exist.
- [ ] Never overwrite secret-bearing `/etc/*.json` or `/etc/nut/*` without a timestamped backup and an explicit flag.
- [ ] Apply permissions for JSON config:

```bash
chown root:root /etc/m5stack-ha-publish.json /etc/power-sentinel.json /etc/power-sentinel-nut-clients.json
chmod 0600 /etc/m5stack-ha-publish.json /etc/power-sentinel.json /etc/power-sentinel-nut-clients.json
```

- [ ] Apply NUT ownership/modes compatible with Ubuntu NUT packages:
  - [ ] `/etc/nut/nut.conf`, `/etc/nut/ups.conf`, `/etc/nut/upsd.conf`: owner `root:nut`, mode `0640` unless distro defaults require stricter handling.
  - [ ] `/etc/nut/upsd.users` and any installed `/etc/nut/upsmon.conf`: owner `root:nut`, mode `0640`.
- [ ] For Mosquitto operations, read credentials from root-only config files and pass them via temporary Mosquitto option files, not command-line argv.

## Proxmox dependency / ACL manual step

For VM mini-card disk usage, print a manual step to create/verify a read-only Proxmox API token with guest-agent audit only:

- [ ] Role: `PowerSentinelReadOnly`.
- [ ] Privileges: `Sys.Audit VM.Audit Datastore.Audit VM.GuestAgent.Audit`.
- [ ] Token identifier may be site-specific; source inventory example is `power-sentinel@pve!readonly`. Do not print token secrets.
- [ ] Assign role to both the owning user and the privilege-separated token, with propagation.
- [ ] Confirm this enables `qemu/{vmid}/agent/get-fsinfo` without granting unrestricted guest-agent command execution.

## Ordering constraints

- [ ] Preflight target OS/architecture/vendor runtime before changing the system.
- [ ] Install apt dependencies before installing services that depend on `python3`, `python3-zmq`, NUT, Mosquitto clients, or systemd.
- [ ] Install executables before enabling services that reference them.
- [ ] Install config templates/stubs and set root-only permissions before starting services that read them.
- [ ] Install systemd units, run `systemctl daemon-reload`, then enable/start selected services.
- [ ] Keep NUT shutdown behavior inert by default: do not enable `nut-monitor` and do not trigger FSD/shutdown during install.
- [ ] Run validation after services are started.
- [ ] Print remaining manual steps after validation: UPS cable/driver confirmation, real MQTT/HA/Proxmox values, Proxmox ACL, optional HA publisher timer enablement, optional Standard NUT `upsmon` arming, and CoreS3 firmware flash.

## Validation commands

Run on the LLM Module after install. These commands intentionally avoid private tokens/hosts.

```bash
python3 --version
python3 - <<'PY'
import zmq
print('pyzmq', zmq.__version__)
PY
command -v \
  m5stack-healthcheck \
  m5stack-ha-publish \
  m5stack-ups-detect \
  power-sentinel-api \
  power-sentinel-stackflow-unit \
  upsc upsdrvctl upsd upsmon nut-scanner mosquitto_pub mosquitto_sub curl ss
systemctl status llm-sys.service nut-server.service power-sentinel-api.service power-sentinel-stackflow-unit.service --no-pager
systemctl is-enabled nut-monitor.service || true
systemctl is-active nut-monitor.service || true
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool >/dev/null
```

Optional checks when publisher timers were explicitly enabled:

```bash
systemctl status m5stack-ha-publish.timer m5stack-ha-publish-chat.timer --no-pager
journalctl -u m5stack-ha-publish.service -n 50 --no-pager
journalctl -u m5stack-ha-publish-chat.service -n 50 --no-pager
```

NUT observer/readiness checks, safe by default:

```bash
systemctl is-active nut-server.service
upsc homelab_ups@localhost ups.status || true
python3 - <<'PY'
import json, urllib.request
with urllib.request.urlopen('http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1', timeout=5) as r:
    data = json.load(r)
shutdown = data.get('shutdown', {})
assert shutdown.get('real_shutdown_owner') in (None, 'upsmon')
print(json.dumps({
    'armed': shutdown.get('armed'),
    'real_shutdown_owner': shutdown.get('real_shutdown_owner'),
    'primary_ready': shutdown.get('primary_ready'),
}, sort_keys=True))
PY
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

## Gaps remaining before installer implementation

- [ ] Blocking: `m5stack-healthcheck` is listed as required and is invoked by current backend code, but no matching source file exists under `backend/bin/` in this repo snapshot.
- [ ] Sanitization follow-up outside this checklist: current backend examples/units should be reviewed before public release for site-specific defaults in config templates or environment lines. The installer should not propagate private defaults into generated runtime files.
- [ ] Exact CLI flag names are proposed here (`--force`, `--arm-nut-monitor`, `--enable-ha-publisher`) and should be confirmed when `backend/install/install-llm-module.sh` is implemented.
- [ ] CoreS3 firmware flash remains a manual step; the installer should not attempt it unless a later requirement explicitly adds a safe flashing flow.
- [ ] Real UPS cable attachment/detection, MQTT/HA credentials, Proxmox token secret, Proxmox ACL assignment, and site network target values remain operator-provided manual data.

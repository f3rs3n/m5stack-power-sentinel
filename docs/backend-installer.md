# Backend Installer

The Backend Installer is `scripts/install-power-sentinel.sh`. It installs and updates the Module LLM/Linux side of Power Sentinel.

It is not a CoreS3 firmware flasher.

## What it owns

- `/usr/local/bin/power-sentinel-api`
- `/usr/local/bin/power-sentinel-stackflow-unit`
- `/usr/local/bin/m5stack-ups-detect` when NUT is selected
- `power-sentinel-api.service`
- `power-sentinel-stackflow-unit.service`
- `power-sentinel-stackflow-healthcheck.service` and `.timer`
- `/etc/power-sentinel.json`
- service enable/start and backend health verification during real installs

## What it does not own

- CoreS3 firmware flashing
- OTA updates
- `/etc/nut/` runtime policy
- Proxmox credentials beyond writing a placeholder config template
- Home Assistant or future module placeholders

The installed API service listens on port `8088` for local/LAN use. Keep that port on a trusted network and firewall it from untrusted networks.

## Usage

Inspect NUT-only install/update behavior:

```bash
scripts/install-power-sentinel.sh --modules nut --dry-run
```

Install/update NUT-only baseline:

```bash
sudo scripts/install-power-sentinel.sh --modules nut
```

Inspect NUT plus optional Proxmox behavior:

```bash
scripts/install-power-sentinel.sh --modules nut,proxmox --dry-run
```

Install/update NUT plus optional Proxmox:

```bash
sudo scripts/install-power-sentinel.sh --modules nut,proxmox
```

Unsupported module selections fail before installation:

```bash
scripts/install-power-sentinel.sh --modules bogus --dry-run
```

## Dry-run verification recorded for public v1 planning

NUT-only dry-run was run from the repo root without `sudo` because dry-run does not mutate the host. It printed install operations for the API binary, StackFlow unit binary, NUT diagnostics helper, systemd units, and generated a config with `nut.enabled=true` and `proxmox.enabled=false`.

NUT-plus-Proxmox dry-run was also run. It printed the same install operations, emitted a note to configure read-only Proxmox credentials, and generated a config with `proxmox.enabled=true` while keeping the placeholder `token_secret`. Placeholder Proxmox config does not attempt a live API call in runtime summary behavior.

Unsupported module dry-run was run with `bogus`; it returned exit code 2 and printed `Unsupported module: bogus`.

Real installs preserve an existing `/etc/power-sentinel.json` so read-only Proxmox credentials and local module settings are not overwritten. If the file is missing, the installer creates a root-only placeholder config.

## Post-install checks

```bash
systemctl is-active power-sentinel-api.service power-sentinel-stackflow-unit.service
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
power-sentinel-stackflow-healthcheck --timeout 5
```

The StackFlow healthcheck verifies the backend summary, a synthetic StackFlow TCP summary, and whether the CoreS3 firmware has recently sent its own `ps-nut-*` poll request through the live StackFlow unit.

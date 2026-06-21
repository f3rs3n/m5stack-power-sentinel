# Troubleshooting and Public Repo Audit

## Backend service checks

```bash
systemctl is-active power-sentinel-api.service power-sentinel-stackflow-unit.service
curl -fsS 'http://127.0.0.1:8088/api/v1/health' | python3 -m json.tool
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
```

If running from a checkout without installed services:

```bash
python3 backend/bin/power-sentinel-api.py --summary
```

## Installer dry-run checks

```bash
scripts/install-power-sentinel.sh --modules nut --dry-run
scripts/install-power-sentinel.sh --modules nut,proxmox --dry-run
```

Unsupported modules should fail clearly:

```bash
scripts/install-power-sentinel.sh --modules bogus --dry-run
```

## Firmware checks

```bash
cd firmware/core-s3-display
pio run -e m5stack-cores3
pio run -e m5stack-cores3 -t upload --upload-port /dev/ttyACM0
```

Capture serial evidence from the repository root:

```bash
python3 tools/core_s3_serial_capture.py --port /dev/ttyACM0 --duration 15
```

## State guide

- `disabled`: the module is intentionally off.
- `unconfigured`: the module is enabled but lacks minimum useful config.
- `unavailable`: the module cannot currently produce useful data.
- `stale`: prior useful data is no longer fresh enough to trust.
- `warning`: attention is needed but not immediately critical.
- `critical`: urgent attention or safety-relevant handoff is needed.

## Common NUT checks

```bash
upsc homelab_ups@localhost
python3 backend/bin/power-sentinel-api.py --summary
```

Remember: shutdown remains owned by NUT and upsmon. Only `OB LB` is shutdown intent; `OL ... LB` is warning/no-shutdown.

## Common Proxmox checks

- Confirm `proxmox.enabled` is `true` only when you intend to configure it.
- Replace placeholder `token_secret` with a real read-only token secret before expecting live API observation.
- Confirm `api_url` points to your Proxmox API endpoint.
- Set `network_uplink` and `network_uplink_speed_mbps` when network auto-selection is ambiguous.
- Do not paste token secrets into issues or logs.

## Public Repo Audit checklist

Before changing repository visibility or cutting a public v1 release, audit public-path files for:

- committed secrets, passwords, API keys, token secrets, private keys, or `.env` files;
- private IPs, private hostnames, usernames, absolute maintainer paths, and live device identifiers presented as public workflow;
- examples that imply admin credentials where read-only credentials are expected;
- generated artifacts such as PlatformIO builds, `.tmp-tests`, logs, captures, caches, and local configs;
- public docs that point users to Maintainer Operations Notes as the install path;
- missing or unclear `SECURITY.md` and `LICENSE` guidance;
- stale references to OTA, Module LLM-mediated flashing, web UI, mutable config CLI, or CoreS3 options page as supported v1 features.

Use documentation-reserved placeholders such as `pve.example` and `module-llm.local` in public docs.

# M5Stack Power Sentinel

Power Sentinel is a local homelab companion for the M5Stack CoreS3 + Module LLM stack. It surfaces homelab state on an Ambient Console without becoming a replacement control plane for NUT, Proxmox, Home Assistant, or other specialist tools.

The public v1 release target is a **First Public Release User**: a technically capable homelab owner who can assemble the supported M5Stack hardware stack, use Linux/SSH on the Module LLM, edit `/etc/power-sentinel.json`, configure read-only integrations, and flash the CoreS3 through USB/PlatformIO.

## What is supported in public v1

- **NUT Monitor**: supported default module. Power Sentinel observes UPS/NUT state and displays power readiness. NUT and upsmon own real shutdown behavior.
- **Proxmox Module**: supported optional module. It is API-only, read-only, disabled by default, and aimed at a single-node homelab Proxmox installation.
- **Backend Installer**: installs and updates the Module LLM/Linux services, systemd units, module config template, and backend health checks.
- **Firmware Release Path**: USB/PlatformIO build and flashing for the CoreS3 live firmware.
- **Configuration Surface**: `/etc/power-sentinel.json` plus read-only validation/troubleshooting commands.
- **Public Repo Audit**: release-blocking audit for secrets, private lab assumptions, unsafe examples, license/security guidance, and generated artifacts.

## Known public v1 limitations

- English-only public documentation.
- no OTA firmware updates in v1.
- no Module LLM-mediated flashing in v1.
- no CoreS3 setup/options page in v1.
- no mutable config CLI or web UI in v1.
- no Wi-Fi/HTTP polling fallback in the CoreS3 firmware; live data uses StackFlow/UART.
- no Proxmox SSH, remote shell commands, guest controls, backup management, storage/network changes, or console replacement.
- NUT and upsmon own real shutdown behavior; Power Sentinel only observes and displays the shutdown-relevant condition.

## Public documentation

Start here:

- [Quickstart](docs/quickstart.md)
- [Hardware and transport assumptions](docs/hardware.md)
- [Backend Installer](docs/backend-installer.md)
- [Configuration Surface](docs/configuration.md)
- [CoreS3 Firmware Release Path](docs/firmware-flashing.md)
- [NUT Monitor](docs/modules/nut.md)
- [Proxmox Module](docs/modules/proxmox.md)
- [Troubleshooting and Public Repo Audit](docs/troubleshooting.md)
- [Public Repo Audit checklist](docs/public-repo-audit.md)
- [Security policy](SECURITY.md)
- [License](LICENSE)
- [Third-party notices](NOTICE)

Maintainer/dev-unit evidence lives under `docs/operations/`. Those files can explain why hardware or architecture decisions were made, but they are not the public install path.

## Repository layout

```text
backend/
  bin/                              local API, StackFlow unit, NUT diagnostics
  config/                           sanitized configuration examples only
  systemd/                          service unit files
  tests/                            stdlib-only regression tests
firmware/core-s3-display/           CoreS3 PlatformIO firmware
scripts/install-power-sentinel.sh   Backend Installer for Module LLM/Linux
docs/                               public docs, architecture notes, ADRs, operations evidence
tools/                              test, guard, and serial-capture helpers
```

## Verification gates

The main release-readiness gates are:

```bash
python3 tools/run_tests.py
python3 tools/check_core_s3_ui.py
python3 tools/check_public_release_docs.py
scripts/install-power-sentinel.sh --modules nut --dry-run
scripts/install-power-sentinel.sh --modules nut,proxmox --dry-run
cd firmware/core-s3-display
pio run -e m5stack-cores3
```

Use the quickstart and module docs for the full install and verification flow.

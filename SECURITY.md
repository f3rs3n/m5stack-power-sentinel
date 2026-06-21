# Security Policy

## Supported scope

Public v1 supports the documented local Power Sentinel deployment only:

- Module LLM/Linux backend services installed by the Backend Installer.
- CoreS3 live firmware flashed through the documented USB/PlatformIO Firmware Release Path.
- NUT Monitor as the supported default module.
- Proxmox Module as a supported optional API-only/read-only module.

Out of scope for public v1 security support:

- OTA firmware updates.
- Module LLM-mediated CoreS3 flashing.
- Web UI or hosted service behavior.
- Mutable configuration CLI or setup wizard.
- Proxmox controls, SSH, remote shell commands, guest actions, or console replacement.
- Private forks or local modifications that are not part of the documented public release path.

## Reporting a vulnerability

If you find a vulnerability, open a private report through GitHub Security Advisories if available for the repository. If private advisories are not available, contact the maintainer privately before publishing details.

Please include:

- affected commit or release;
- affected module or firmware path;
- reproduction steps;
- expected and actual impact;
- whether secrets, credentials, or device access are involved.

## Do not publish secrets

Do not publish real values from `/etc/power-sentinel.json`, Proxmox `token_secret`, NUT passwords, private keys, `.env` files, serial captures containing credentials, or screenshots/logs that expose sensitive local details.

Public issues and examples should use placeholders such as `pve.example`, `module-llm.local`, and `REPLACE_WITH_READ_ONLY_TOKEN_SECRET`.

The installed API service listens on port `8088` for local/LAN use. Do not expose it directly to the public internet; keep it on a trusted network and use host firewall rules if your Module LLM is reachable from untrusted networks.

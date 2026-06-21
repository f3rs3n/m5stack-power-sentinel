# Public Release Readiness v1

## Intent

Make Power Sentinel public as an installable and verifiable project for a technically capable homelab owner. The first public release is not a consumer onboarding flow: it assumes the user can assemble the supported M5Stack hardware stack, use Linux/SSH on the Module LLM, edit `/etc/power-sentinel.json`, configure read-only integrations, and flash the CoreS3 via USB/PlatformIO.

The release bar is: an external user can install the backend, flash the CoreS3 firmware, enable the supported modules, and verify the system from English public documentation without private maintainer knowledge.

## Product boundaries

- Public docs are English-only.
- NUT Monitor is the supported default module.
- Proxmox is a supported optional module: API-only, read-only, single-node homelab oriented.
- Home Assistant, Hermes Buddy, Local Inference, Telemetry History, controls, OTA updates, and LLM-mediated firmware flashing are out of scope for v1.
- The Configuration Surface is `/etc/power-sentinel.json` plus read-only validation/troubleshooting commands. No mutable config CLI, web UI, or CoreS3 setup/options page in v1.
- The Backend Installer installs Module LLM/Linux services and config templates only. It does not flash CoreS3 firmware.
- The Firmware Release Path is USB/PlatformIO flashing. Release binaries may be attached later, but they are not required for the v1 promise.

## Workstream 1: public documentation cleanup

Deliverables:

- Rewrite `README.md` in English as the public entry point.
- Add or reshape public docs:
  - `docs/quickstart.md`
  - `docs/hardware.md`
  - `docs/configuration.md`
  - `docs/firmware-flashing.md`
  - `docs/modules/nut.md`
  - `docs/modules/proxmox.md`
  - `docs/troubleshooting.md`
- Keep maintainer/dev-unit evidence separate from public install docs:
  - `docs/operations/current-state.md`
  - UART discovery notes
  - stack power architecture notes
  - live validation notes/captures
- Mark operations docs clearly as maintainer/dev-unit evidence, not the universal public install path.

Definition of done:

- Public install path does not depend on private IPs, hostnames, usernames, local paths, or maintainer-only lab state.
- README links to the quickstart, hardware, configuration, firmware flashing, modules, troubleshooting, security, and license docs.
- Public docs state known limitations explicitly.

## Workstream 2: backend installer hardening

Deliverables:

- Keep `scripts/install-power-sentinel.sh` focused on Module LLM/Linux backend installation.
- Verify and document:
  - supported arguments
  - dry-run behavior
  - installed files
  - systemd units
  - config file path
  - post-install health check
- Ensure reruns are safe enough for an update path.
- Ensure unsupported modules fail clearly.
- Avoid overwriting NUT runtime config blindly.

Definition of done:

- `sudo scripts/install-power-sentinel.sh --modules nut --dry-run` prints understandable actions and generated config.
- `sudo scripts/install-power-sentinel.sh --modules nut,proxmox --dry-run` documents Proxmox credential follow-up without attempting live access with placeholder secrets.
- Installer docs explain what the script does and does not own.

## Workstream 3: firmware flashing docs/release path

Deliverables:

- Document the official v1 CoreS3 flashing path via USB/PlatformIO.
- Document firmware environments:
  - `m5stack-cores3` live firmware
  - fixture/test environments as non-live validation tools
- Document serial/log verification with `tools/core_s3_serial_capture.py`.
- State explicitly that OTA and Module LLM-mediated flashing are not v1 supported paths.

Definition of done:

- A user can build `m5stack-cores3` from docs.
- A user can flash to a visible CoreS3 USB serial device from docs.
- Firmware docs identify the expected StackFlow/UART baseline: CoreS3 RX=G18 TX=G17, `Serial2`, 115200, no HTTP/WiFi polling fallback.

## Workstream 4: configuration validation and troubleshooting

Deliverables:

- Document `/etc/power-sentinel.json` as the only mutable v1 configuration surface.
- Document module-local `enabled` fields.
- Provide clear examples for:
  - NUT-only default
  - NUT + Proxmox optional module
- Add or verify read-only config/status commands, such as:
  - `power-sentinel-api --summary`
  - a future `--validate-config` if needed
  - `curl http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1`
  - `systemctl is-active power-sentinel-api.service power-sentinel-stackflow-unit.service`
- Troubleshooting must explain `disabled`, `unconfigured`, `unavailable`, `stale`, `warning`, and `critical`.

Definition of done:

- Placeholder credentials do not trigger misleading live API attempts.
- Token secrets never appear in payloads, logs, docs examples beyond placeholders, or troubleshooting output.
- Users can distinguish config errors from integration/API failures.

## Workstream 5: NUT supported module docs

Deliverables:

- Document NUT Monitor as the supported default module.
- Document what Power Sentinel observes from NUT.
- Document that real shutdown remains owned by NUT/`upsmon`.
- Document the critical rule: do not shut down if line power is present; only `OB LB` is shutdown intent, while `OL ... LB` is warning/no-shutdown.
- Document expected unavailable and stale states.

Definition of done:

- A user can verify NUT data reaches `/api/v1/summary`.
- A user can understand why Power Sentinel is read-only for shutdown behavior.

## Workstream 6: Proxmox optional supported module docs

Deliverables:

- Document Proxmox as supported optional, disabled by default.
- Document minimum config:
  - `proxmox.enabled`
  - `proxmox.api_url`
  - `proxmox.token_id`
  - `proxmox.token_secret`
- Document read-only token expectations and least-privilege intent.
- Document API-only boundary: no SSH, no remote shell commands, no guest controls.
- Document single-node homelab v1 assumption.
- Document optional network uplink config when API auto-selection is ambiguous.
- Document expected `unconfigured`, `not_observed`, `api_unavailable`, and observed states.

Definition of done:

- A user can enable Proxmox without making Power Sentinel a Proxmox control surface.
- Failure states are understandable without exposing secrets.

## Workstream 7: public repo audit, security, and license

Deliverables:

- Run a public repo audit before changing visibility.
- Add `SECURITY.md` with supported versions/scope and vulnerability reporting guidance.
- Add `LICENSE` if missing.
- Verify `.gitignore` excludes:
  - private firmware config (`firmware/core-s3-display/include/power_sentinel_config.h`)
  - PlatformIO build artifacts (`.pio/`)
  - logs/captures and local generated files
- Search for and sanitize public-path occurrences of:
  - private IPs
  - private hostnames
  - usernames
  - absolute maintainer paths
  - tokens/secrets/passwords
  - live device identifiers that are not useful as public examples
- Keep non-sensitive operations docs only if clearly marked as maintainer/dev-unit evidence.

Definition of done:

- No committed secrets.
- No public quickstart step depends on maintainer lab state.
- All examples use placeholders such as `pve.example`, `module-llm.local`, or documentation-reserved IPs where appropriate.
- License and security guidance are present.

## Workstream 8: verification gates

Required gates before public v1:

```bash
python3 tools/run_tests.py
python3 tools/check_core_s3_ui.py
cd firmware/core-s3-display
pio run -e m5stack-cores3
```

Installer dry-run gates:

```bash
sudo scripts/install-power-sentinel.sh --modules nut --dry-run
sudo scripts/install-power-sentinel.sh --modules nut,proxmox --dry-run
```

Backend smoke gates on an installed Module LLM:

```bash
systemctl is-active power-sentinel-api.service power-sentinel-stackflow-unit.service
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
```

Hardware/live validation when the CoreS3 and Module LLM are available:

```bash
python3 tools/core_s3_serial_capture.py --port /dev/ttyACM0 --duration 15
```

Definition of done:

- Test and build gates pass in the maintainer environment.
- Installer dry-runs are captured and reviewed.
- If hardware is available, serial/display validation confirms live StackFlow data and visible module pages.
- If hardware is not available for a given release candidate, the release notes say exactly which live checks were not rerun.

## Suggested issue breakdown

1. Rewrite README and add English public docs skeleton.
2. Add hardware and firmware flashing docs.
3. Add configuration and troubleshooting docs.
4. Add NUT supported module docs.
5. Add Proxmox optional supported module docs.
6. Harden/document installer dry-run/update behavior.
7. Add `SECURITY.md`, `LICENSE`, and repo hygiene checks.
8. Run public repo audit and sanitize public-path docs.
9. Run verification gates and record release readiness evidence.

## Open questions for later releases

- Should GitHub Releases attach prebuilt CoreS3 firmware binaries?
- What recovery and rollback model is required before OTA can be supported?
- Can CoreS3 flashing through the Module LLM be made reliable enough to support publicly?
- Which lightweight, reversible actions are valuable enough to justify future on-device controls?
- When should Home Assistant graduate from Module Candidate to Supported Module?

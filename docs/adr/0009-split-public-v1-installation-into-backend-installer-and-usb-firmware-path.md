# Split public v1 installation into backend installer and USB firmware path

For the first public release, Power Sentinel has two installable units rather than one combined installer.

The Module LLM/Linux side is installed and updated by the backend installer. It owns API, StackFlow unit, systemd service installation, module configuration templates, selected module enablement, and backend health verification.

The CoreS3 side is distributed as firmware with a documented USB/PlatformIO flashing path. A GitHub Release may later attach a built firmware binary, but the v1 public promise is USB flashing, not OTA and not flashing through the Module LLM.

This split matches the real product boundary: the backend runs on Linux with service/config privileges, while the CoreS3 is a separate MCU that needs a safe bootloader/flashing path. Combining both paths would mix Linux package/service installation, ESP32 reset/boot mode behavior, USB/serial access, and hardware-stack assumptions into one fragile workflow.

Alternatives rejected:

- One mega-installer that installs backend services and flashes the CoreS3: too fragile for the first public release and too dependent on local USB topology and boot-mode behavior.
- Flashing the CoreS3 through the Module LLM as the public v1 path: potentially useful as a future experiment, but not yet proven enough to promise publicly.
- OTA firmware updates in v1: adds Wi-Fi, credentials, security, recovery, and versioning responsibilities that are outside the current StackFlow/UART-first baseline.
- On-device setup/options UI for installation: violates the Ambient Console boundary for v1; installation and module configuration remain Linux/config-file concerns.

Consequences:

- Public quickstart must document backend install and firmware flash as separate steps.
- `scripts/install-power-sentinel.sh` should stay focused on Module LLM/Linux backend installation and verification.
- Firmware documentation should make USB/PlatformIO the official v1 path and avoid promising OTA or LLM-mediated flashing.
- Future OTA or LLM-mediated flashing requires a separate decision after recovery, authentication, rollback/versioning, and hardware-flow validation are designed.

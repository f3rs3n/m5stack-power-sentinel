# Current state

Power Sentinel has been reframed to a clean NUT Monitor baseline.

Implemented now:

- Backend API `power-sentinel-api.py` with modular registry.
- `nut` module enabled by default and backed by live `upsc homelab_ups@localhost` plus lightweight NUT service checks.
- Backend summary includes a `module` status block for the CoreS3 top row: Module LLM LAN state/IP and local `HH:MM` time. The deployed Module LLM host should run in `Europe/Rome` so the display uses local 24-hour time.
- StackFlow custom unit preserved as the only CoreS3 live-data transport; the CoreS3 firmware no longer has an HTTP client/fallback path.
- CoreS3 firmware simplified to one screen: the new Ledcards NUT Monitor UI with Module LLM LAN, CoreS3 Wi-Fi, serial-link status, page dots, local time, and local battery glyph in the top row.
- LVGL MCP fixture/results retained only for the NUT Ledcards interface.
- Installer script supports `--modules nut[,proxmox,ha]`; only `nut` installs telemetry today.

Hardware notes:

- Current physical stack order is `DIN Base -> LLM Mate -> LLM Module -> CoreS3`.
- The DIN Base switch must be ON for the BAT rail to be visible through the stack. With it ON, the CoreS3/AXP2101 can see the DIN battery rail (`battery_mv` around 4190 mV and `charging=1` in the probe), but the SOC percentage may need time to settle after long storage/off time.
- A short physical power-loss test with USB power removed from the LLM Mate/Module kept the CoreS3 powered, while the LLM Mate and LLM Module powered off. Treat the DIN battery/BAT rail as CoreS3/local-display backup only, not as a full-stack UPS for the Linux/LLM side.
- Leave battery-icon heuristics conservative until a longer physical test observes voltage, percent, and charging behavior over time during and after external-power removal.
- Detailed DIN Base / LLM Mate rail notes live in `docs/operations/stack-power-architecture.md`.

Archived/removed from active baseline:

- HOME/PVE/HA/M5S multi-tab firmware UI.
- Proxmox live backend.
- Home Assistant/MQTT/Zigbee2MQTT backend and publisher units.
- Old broad UI fixtures/results.

These are not lost: recover from Git history when reintroducing each module as a page module.

Operational verification:

```bash
python3 tools/run_tests.py
python3 backend/bin/power-sentinel-api.py --summary
sudo scripts/install-power-sentinel.sh --modules nut --dry-run
```

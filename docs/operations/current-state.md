# Current state

Power Sentinel has been reframed to a clean NUT Monitor baseline.

Implemented now:

- Backend API `power-sentinel-api.py` with modular registry.
- `nut` module enabled by default and backed by live `upsc homelab_ups@localhost` plus lightweight NUT service checks.
- StackFlow custom unit preserved for internal CoreS3 transport.
- CoreS3 firmware simplified to one screen: the new Ledcards NUT Monitor UI.
- LVGL MCP fixture/results retained only for the NUT Ledcards interface.
- Installer script supports `--modules nut[,proxmox,ha]`; only `nut` installs telemetry today.

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

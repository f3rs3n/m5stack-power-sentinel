# Current state

Power Sentinel has been reframed to a clean NUT Monitor baseline.

Implemented now:

- Backend API `power-sentinel-api.py` with modular registry.
- `nut` module enabled by default and backed by live `upsc homelab_ups@localhost` plus lightweight NUT service checks.
- Backend summary includes a `module` status block for the CoreS3 top row: Module LLM LAN state/IP and local `HH:MM` time. The deployed Module LLM host should run in `Europe/Rome` so the display uses local 24-hour time.
- NUT Ambient Console contract locked in `docs/architecture/nut-ambient-console-contract.md`; the page model owns telemetry completeness, metric cards, hero priority, and touch override policy before rendering.
- `proxmox` has a first API-only/read-only backend adapter slice and a CoreS3 ambient page gated by the backend module state.
- StackFlow custom unit preserved as the only CoreS3 live-data transport; the CoreS3 firmware no longer has an HTTP client/fallback path.
- CoreS3 firmware defaults to live StackFlow data and renders the Ledcards NUT Monitor UI plus the Proxmox ambient page when available. The top row shows Module LLM LAN, CoreS3 Wi-Fi, serial-link status, page dots, local time, and local battery glyph.
- CoreS3 display standby policy is production-timed: after the first valid payload, 5 minutes without touch or meaningful state change fades `AWAKE` to `DIM`; the fade is about 900 ms. Automatic standby stops at `DIM`; `OFF` remains reserved for 3-second long-press snooze or 15 minutes without a valid payload. Current static brightness defaults are `AWAKE=120` and `DIM=36`; adaptive ALS is hardware-aware and targets physical Backlight Level curves `DIM 20/20/21/21/22/22` and `AWAKE 21/22/23/24/25/25` over raw breakpoints `0/12/45/70/95/135`, encoded as center-band nominal brightness values before calling M5GFX. Adaptive slew uses `step=1` with intervals `96/136/160/192 ms` from high to low brightness bands.
- LVGL MCP fixture/results retained only for the NUT Ledcards interface.
- Installer script supports `--modules nut[,proxmox,ha]`; `nut` and `proxmox` are active display modules when configured, and `ha` remains a placeholder.

Hardware notes:

- Current physical stack order is `DIN Base -> LLM Mate -> LLM Module -> CoreS3`.
- The DIN Base switch must be ON for the BAT rail to be visible through the stack. With it ON, the CoreS3/AXP2101 can see the DIN battery rail (`battery_mv` around 4190 mV and `charging=1` in the probe), but the SOC percentage may need time to settle after long storage/off time.
- A short physical power-loss test with USB power removed from the LLM Mate/Module kept the CoreS3 powered, while the LLM Mate and LLM Module powered off. Treat the DIN battery/BAT rail as CoreS3/local-display backup only, not as a full-stack UPS for the Linux/LLM side.
- Leave battery-icon heuristics conservative until a longer physical test observes voltage, percent, and charging behavior over time during and after external-power removal.
- Detailed DIN Base / LLM Mate rail notes live in `docs/operations/stack-power-architecture.md`.

Archived/removed from active baseline:

- HOME/PVE/HA/M5S multi-tab firmware UI.
- Proxmox CoreS3 page rendering and control surface.
- Home Assistant/MQTT/Zigbee2MQTT backend and publisher units.
- Old broad UI fixtures/results.

These are not lost: recover from Git history when reintroducing each module as a separately scoped module, but do not restore the old dashboard wholesale. Proxmox reintroduction is API-only, read-only observability with contextual handoff, not a Proxmox control surface.

Operational verification:

```bash
python3 tools/run_tests.py
python3 backend/bin/power-sentinel-api.py --summary
sudo scripts/install-power-sentinel.sh --modules nut --dry-run
/home/martino/.platformio/penv/bin/pio run -e m5stack-cores3
curl -fsS 'http://192.168.2.202:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
```

Use `m5stack-cores3` for live firmware. The `m5stack-cores3-ledcards-interface`
environment embeds a static Ledcards visual fixture and is only for layout checks.

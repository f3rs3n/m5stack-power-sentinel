# NUT Monitor

NUT Monitor is the supported default module for public v1.

Power Sentinel observes NUT and UPS state, turns it into a module condition, and displays it on the CoreS3 Ambient Console. It does not own real shutdown behavior.

## What Power Sentinel observes

The NUT Monitor summary includes UPS availability, freshness, status flags, battery charge, runtime, load, voltages, driver/server/monitor health, configured clients, and whether the current status is shutdown-relevant.

Useful verification commands:

```bash
upsc homelab_ups@localhost
python3 backend/bin/power-sentinel-api.py --summary
curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool
```

Adjust the `nut.ups` value in `/etc/power-sentinel.json` if your NUT UPS name differs.

## Shutdown ownership

Real shutdown remains owned by NUT and `upsmon`. Power Sentinel is read-only for shutdown behavior.

The critical shutdown rule is:

- `OB LB` means on battery and low battery: shutdown intent is true.
- `OL ... LB` means line power is present: warning/no-shutdown.

Do not configure Power Sentinel as a custom shutdown orchestrator.

## Conditions

- `healthy`: line power and NUT data are useful.
- `warning`: attention is needed, such as on-battery state or battery readiness below the configured full threshold.
- `critical`: shutdown-relevant `OB LB` state.
- `stale`: previously useful data is too old to trust.
- `unavailable`: NUT or the UPS cannot currently produce useful data.

Unavailable and stale states are data-flow/configuration conditions, not fake telemetry.

## CoreS3 display

The NUT Module Page shows five Ambient Cards: battery, runtime, load, input, and NUT details. The Hero Position is chosen by condition priority, default policy, or touch focus. Touch focus is presentation-only and does not trigger shutdown or remediation.

## Verification evidence

The repo regression suite includes NUT summary, condition mapping, shutdown-rule, and NUT Ambient Page model tests. Run:

```bash
python3 tools/run_tests.py
python3 tools/check_core_s3_ui.py
```

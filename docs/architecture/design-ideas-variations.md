# Design Ideas and Variations

This file keeps lightweight visual/UI exploration notes for Power Sentinel. Current normative rules live in `PRODUCT.md`, `DESIGN.md`, and `docs/architecture/core-s3-pages-v1.md`.

## Useful directions

- Dark, high-contrast embedded HMI style.
- Compact cards with one primary idea each.
- Icon-only left sidebar on CoreS3.
- Horizontal card carousel inside each tab.
- HOME should stay operational and calm when nominal.
- NUT should focus on UPS telemetry, service state, connected count, and optional connected-host list.
- PVE should focus on node health, capacity, interfaces, and compact workload summary.
- HA should focus on HA/MQTT/Zigbee2MQTT health rather than generic entity lists.
- M5S should focus on appliance health and transport freshness.

## Rejected visual directions

- Card walls and badge walls.
- Dense tables.
- Emoji status icons without proven firmware font coverage.
- Decorative charts with no decision value.
- Large empty padding on the 320x240 screen.

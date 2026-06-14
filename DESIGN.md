# DESIGN: Power Sentinel NUT Monitor

## Current UI baseline

The active UI is the Ledcards NUT Monitor fullscreen page, implemented in:

- `firmware/core-s3-display/src/ledcards-interface-page.cpp`
- `firmware/core-s3-display/src/ledcards-interface-page.h`
- `assets/lvgl-spike/power-sentinel-nut-ledcards-interface-fixture.c`

The old HOME/NUT/PVE/HA/M5S tab dashboard is archived out of the active baseline.

## Visual rules

- 320x240 CoreS3 screen.
- Large hero metric plus four compact metric tiles.
- Metrics: Battery, Runtime/TTE, Load, Input, NUT clients.
- Hero TTE can use full `mm:ss`; compact TTE uses rounded minutes with unit `m`.
- Missing/stale data must show unavailable/stale states, never plausible sample values.
- Touching a mini-card may promote it to hero through the ring transition.
- Long press sleeps only the display backlight; telemetry loop continues.
- The top status bar is local/device status only: Module LLM LAN, CoreS3 Wi-Fi, recent serial/StackFlow link, real page dot(s), Module LLM `HH:MM`, and CoreS3 battery. LAN/Wi-Fi do not affect hero/severity; link error means no valid serial payload within 10 seconds; the battery icon is CoreS3-local, not UPS/NUT battery.

## Future pages

Future `PROXMOX` and `HA` pages should reuse this quality bar but enter as separate modules. Do not reintroduce the old multi-page firmware wholesale; extract only the parts that still match the new design language.

### Future `PROXMOX` page

The `PROXMOX` page should use five single-purpose Ambient Cards: CPU, RAM, Guests, Storage, and Network. Guests is the Guest Inventory Summary (`running/total`) for visible QEMU VMs and LXC containers, not a configured allowlist. When actionable signals exist, the page should privilege the worst signal over neutral metrics. Touch interaction is for focus only, such as promoting a card to the hero area; it must not trigger Proxmox actions.

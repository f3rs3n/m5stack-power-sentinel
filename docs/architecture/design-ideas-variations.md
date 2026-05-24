# Power Sentinel design ideas and variations

Date: 2026-05-24
Status: reference notes / inspiration, not an implementation contract
Source: LVGL issue comment https://github.com/lvgl/lvgl/issues/804#issuecomment-461566479

This file records visual ideas that may be useful for future CoreS3/LVGL UI refinement. It is intentionally separate from `core-s3-pages-v1.md`: the V1 page spec remains the contract; this document is a parking lot for design references, possible theme tokens, and variations to test later.

No screenshot or third-party asset is copied into this repository. The linked material should be treated as external inspiration only.

## Source material reviewed

The LVGL issue comment suggests using well-known UI guidelines/themes as inspiration for built-in LVGL themes. The specific comment by `seyyah` points to:

1. ngx-admin / Nebular IoT dashboard
   - historical IoT dashboard link: http://akveo.com/ngx-admin/pages/iot-dashboard
   - Nebular repo: https://github.com/akveo/nebular
2. Flat UI Colors default palette
   - https://flatuicolors.com/palette/defo

The comment includes two images:

- a Nebular/ngx-admin dark IoT dashboard screenshot;
- a Flat UI Colors palette screenshot.

## High-level verdict

Useful for Power Sentinel:

- yes, as design reference;
- yes, for color-token inspiration;
- yes, for dashboard component patterns;
- no, as a direct code dependency;
- no, as a desktop layout to port literally.

The Nebular/ngx-admin dashboard is a desktop/web dashboard. Power Sentinel runs on a 320x240 CoreS3 display, so the useful material is the distilled language: dark cards, clear state blocks, vivid but restrained accents, status pills, gauges, and compact operational hierarchy.

## Nebular / ngx-admin IoT dashboard observations

The screenshot is a dark “cosmic” IoT dashboard with:

- purple/blue dark background;
- rectangular cards with soft radius;
- subtle but visible shadowing;
- saturated accents: purple, green, cyan, blue, amber/orange;
- white primary text;
- muted grey/lilac secondary text;
- large status cards at the top;
- a circular temperature gauge;
- compact tab labels such as `Temperature` / `Humidity`;
- a dense electricity card with monthly table plus a large line chart;
- large, readable metrics with small labels.

### Useful patterns for Power Sentinel

#### 1. Status cards

The top Nebular status cards are the most directly useful pattern. They can translate to small CoreS3 subsystem status cards/pills for:

- `NUT`
- `PVE`
- `HA`
- `NET`
- `M5S`

Embedded adaptation:

```text
NUT OK    PVE OK
HA OK     NET OK
M5S WARN
```

Guidelines:

- one short label;
- one state;
- severity color on border/fill/accent;
- optional small icon only if it remains legible;
- no decorative large icon block on 320x240 unless used in a dedicated card.

#### 2. Gauge / arc card

The Nebular circular temperature gauge is a useful inspiration for a NUT/UPS card, especially for:

- battery percent;
- runtime remaining;
- UPS load;
- global power state.

Embedded adaptation:

- use a compact arc or semicircle;
- central value should be large and operationally meaningful, e.g. `100%` or `6m24s`;
- use severity color on the arc;
- avoid consuming the whole HOME tab unless visual testing proves it is worth the space.

Best target location: a dedicated NUT carousel card, not necessarily the HOME overview.

#### 3. Sparkline / trend card

The Nebular electricity chart is too large and detailed for direct use, but the idea of a high-contrast neon line can become a small sparkline for:

- UPS load trend;
- input voltage trend;
- M5Stack temperature trend;
- Proxmox CPU/RAM trend;
- transport/API latency history.

Embedded constraints:

- no complex axis labels;
- no dense legends;
- few points only;
- explicit stale/unavailable state;
- avoid heavy shadows and masks.

#### 4. Dark cosmic card language

The overall visual direction is compatible with Power Sentinel V1:

- dark background;
- high contrast text;
- compact cards;
- restrained accent colors;
- severity/status-first hierarchy.

This aligns with `docs/architecture/core-s3-pages-v1.md`, which already requires a modern, clean, desk-readable dark dashboard with status pills and simple bars.

### Things not to copy

Do not copy the desktop dashboard literally:

- no large desktop grid;
- no full-width chart-heavy layout;
- no monthly table unless a future details page really needs it;
- no huge icon tiles that waste CoreS3 pixels;
- no heavy box shadows, blur, or complex masks;
- no purely decorative metrics.

For this project, visual polish must not compromise LVGL stability or hide live/offline state.

## Flat UI Colors observations

The Flat UI default palette is highly reusable as a compact color-token base. The colors are simple, saturated, and easy to map to operational severity.

Known default Flat UI values:

```text
Turquoise      #1abc9c
Green Sea      #16a085
Emerald        #2ecc71
Nephritis      #27ae60
Peter River    #3498db
Belize Hole    #2980b9
Amethyst       #9b59b6
Wisteria       #8e44ad
Wet Asphalt    #34495e
Midnight Blue  #2c3e50
Sun Flower     #f1c40f
Orange         #f39c12
Carrot         #e67e22
Pumpkin        #d35400
Alizarin       #e74c3c
Pomegranate    #c0392b
Clouds         #ecf0f1
Silver         #bdc3c7
Concrete       #95a5a6
Asbestos       #7f8c8d
```

### Candidate Power Sentinel token mapping

```text
BG_MAIN        #151a30   Nebular-like deep background
BG_CARD        #222b45   Nebular-like card surface
BG_CARD_2      #2c3e50   Flat UI Midnight Blue
TEXT_MAIN      #ecf0f1   Flat UI Clouds
TEXT_MUTED     #95a5a6   Flat UI Concrete
TEXT_DIM       #7f8c8d   Flat UI Asbestos
BORDER         #34495e   Flat UI Wet Asphalt

OK             #2ecc71   Flat UI Emerald
OK_DARK        #27ae60   Flat UI Nephritis
WARN           #f1c40f   Flat UI Sun Flower
WARN_DARK      #f39c12   Flat UI Orange
CRITICAL       #e74c3c   Flat UI Alizarin
CRITICAL_DARK  #c0392b   Flat UI Pomegranate
INFO           #3498db   Flat UI Peter River
INFO_DARK      #2980b9   Flat UI Belize Hole
ACCENT         #1abc9c   Flat UI Turquoise
ACCENT_DARK    #16a085   Flat UI Green Sea
PURPLE         #9b59b6   Flat UI Amethyst
PURPLE_DARK    #8e44ad   Flat UI Wisteria
```

Possible use:

- `OK`: NUT/PVE/HA/NET/M5S healthy states;
- `WARN`: on-battery, stale data, OpenAI/chat smoke failure, Z2M warning;
- `CRITICAL`: low battery, PVE down, HA API down, MQTT down, ZFS/SMART critical;
- `INFO`: transport/API/debug metrics in M5S tab;
- `ACCENT`: live data, StackFlow path, selected navigation;
- `PURPLE`: optional brand/cosmic accent, used sparingly.

## Candidate variation: Power Sentinel Cosmic Flat

A possible future visual variation is a “Power Sentinel Cosmic Flat” theme:

- Nebular-inspired dark cosmic surfaces;
- Flat UI severity tokens;
- low-complexity LVGL objects;
- compact cards and pills;
- minimal or disabled shadows by default;
- optional controlled shadow/gradient trial only after simulator and hardware stability are verified.

### HOME variation

Goal: keep the current HOME contract, but improve visual hierarchy.

Sketch:

```text
POWER SENTINEL       OK
GRID ONLINE

Batt 100%      Run 6m24s
Load 38%       In 223V

NUT OK   PVE OK   HA OK
NET OK   M5S OK

No active problems
```

Possible polish:

- global severity pill uses `OK/WARN/CRITICAL` color;
- main state uses larger type;
- subsystem row uses compact Nebular-like status cards/pills;
- only one accent color family per state;
- no transport counters on HOME.

### NUT variation

Use a dedicated NUT carousel card inspired by the Nebular circular gauge:

Card 1:

```text
UPS ONLINE       OL

      100%
   battery arc

Runtime 6m24s
Load 38%   In 223V
```

Card 2:

```text
NUT SERVER
Server OK
Primary ready yes
Clients 1/1 reachable
Shutdown disarmed
```

Notes:

- keep Standard NUT semantics intact;
- Power Sentinel remains observer/readiness dashboard, not custom shutdown controller;
- no `shutdown.mode` / `shutdown.strategy` fields.

### PVE variation

Use compact cards/bars rather than desktop charts:

- CPU/RAM/storage bars;
- ZFS/SMART pills;
- running VM/LXC mini-cards;
- optional tiny sparkline only if cheap and useful.

Avoid:

- permanent `Temp n/a` placeholder;
- redundant labels such as `usage` / `bar`;
- QEMU `disk: 0` as a false HDD usage value.

### HA variation

Use status-card language for:

- HA API;
- MQTT;
- Zigbee2MQTT;
- coordinator;
- updates;
- device interview status.

Avoid non-actionable details:

- Z2M software version;
- HA birth-topic retained warning as a normal user-facing issue;
- internal topic trivia.

### M5S variation

Use blue/cyan/grey token family for diagnostics:

- StackFlow state;
- API latency;
- poll age;
- source/live/stale/offline;
- OpenAI/chat-smoke status;
- local CPU/RAM/disk/temp.

M5S is the right tab for transport counters and diagnostic internals.

## LVGL / CoreS3 constraints

Any variation from this file must respect the known embedded constraints:

- display resolution is 320x240;
- content must remain legible at desk distance;
- no plausible demo/sample values after live transport fails;
- expose stale/offline/unavailable explicitly;
- prefer flat fills, borders, and small bars over heavy shadows;
- avoid excessive LVGL object hierarchy;
- avoid draw-path-heavy shadows/masks unless isolated and tested;
- keep current interaction split:
  - vertical swipe changes top-level tabs;
  - horizontal swipe changes cards within the current tab;
  - vertical scroll inside a card only for small overflow, with scroll chaining;
- verify with LVGL render harness and PlatformIO build before treating any UI change as ready.

## Licensing / repository hygiene notes

- Nebular and ngx-admin are open source and MIT-licensed, but this project should not vendor or depend on them for firmware UI.
- Do not copy screenshots or third-party visual assets into the repository.
- It is fine to keep URLs and derived design notes.
- If any future code is copied from MIT projects, preserve license attribution; however, the preferred path is independent LVGL implementation inspired by the design patterns only.

## Possible next experiment

If/when UI polish becomes the active task, create one or more LVGL fixture variations under the existing LVGL spike workflow and compare rendered 320x240 screenshots:

1. current HOME baseline;
2. Cosmic Flat HOME with Flat UI severity tokens;
3. NUT gauge/arc card variation;
4. status-card/pill subsystem row variation.

Success criteria:

- PNG renders at 320x240;
- widget tree contains the expected labels;
- no render errors;
- PlatformIO firmware still builds;
- hardware display remains stable and readable;
- no regression in explicit live/stale/offline state reporting.

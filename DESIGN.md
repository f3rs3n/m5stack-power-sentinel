# Power Sentinel CoreS3 Design System

Status: working LVGL/HMI design contract
Applies to: `firmware/core-s3-display/`, `assets/lvgl-spike/`, and any CoreS3 UI docs
Related: `PRODUCT.md`, `docs/architecture/core-s3-pages-v1.md`, `assets/lvgl-spike/README.md`

## Design thesis

Power Sentinel CoreS3 is a small embedded HMI, not a web dashboard compressed to 320x240.

Every screen should help the operator answer one of these questions quickly:

- Is everything OK?
- Is power/NUT live and fresh?
- Which subsystem is degraded?
- Is this display showing fresh live data?

The visual system should be dense but not cluttered, modern but not decorative, and truthful before attractive.

## Screen model

Target canvas:

- 320x240 landscape;
- fixed 44 px left sidebar for top-level tabs;
- 276x240 px content region;
- dark theme;
- touch-capable, with V1 mostly focused on status viewing plus safe display-local actions such as sleep display.

Current top-level tabs:

1. HOME
2. NUT
3. PVE
4. HA
5. M5S

Current tab rail:

- icon-only 18 px glyphs;
- selected tab uses a clear blue/accent pill/background;
- inactive icons remain bright/readable;
- no text suffixes in the normal live UI.

## Device and runtime profile

Target device:

- M5Stack CoreS3 with the M5Stack LLM Kit / LLM Module stack;
- 320x240 landscape display;
- capacitive touch input;
- desk/lab-visible embedded HMI;
- LVGL/M5GFX/M5Unified firmware path;
- LLM Module is the autonomous Linux appliance; CoreS3 is the display/control surface.

CoreS3 consumes normalized `power-sentinel.summary.v1` state, not raw service APIs. Missing, stale, or unavailable data must be explicit and must never fall back to plausible demo values.

## Information hierarchy

Use four HMI levels:

- Level 1: overview/glance. Example: HOME.
- Level 2: operational status. Example: NUT/PVE/HA/M5S cards.
- Level 3: diagnostics. Example: stale age, source errors, client details.
- Level 4: setup/support. Configuration, maintenance, and operator workflows. Mostly out of V1 scope.

Default rule for 320x240:

- one primary idea per screen/card;
- secondary facts in compact rows;
- tertiary/debug details hidden, omitted, or moved to a later detail card;
- avoid dense tables.

## HMI map and placement rules

Current V1 topology:

```text
Left icon rail
  HOME
  NUT
    UPS
    BATTERY
    POWER
    CLIENTS
  PVE
    NODE / CAPACITY / HEALTH cards as implemented
  HA
    HA / MQTT / Z2M health cards as implemented
  M5S
    STACK / TRANSPORT / LOCAL HEALTH / LLM smoke cards as implemented
```

Placement rule for new information:

1. Does it change what the operator does?
2. Is it needed in the first 2-7 seconds?
3. Is it product state, diagnostic state, or raw telemetry?
4. Can it be summarized instead of shown verbatim?
5. What happens when it is missing, stale, long, or unknown?

Default placement:

- urgent global state: HOME;
- subsystem summary: relevant Level 2 tab/card;
- source/freshness/error explanation: M5S or a diagnostic card;
- raw detail: omit until a real use case exists.

## Layout rules

Core rules:

- Prefer structured flex/grid-like layout and reusable components.
- Keep padding tight: typically 2-6 px.
- Keep gaps tight: typically 2-4 px.
- Use dividers, alignment, and typography before adding more boxes.
- Do not nest cards inside cards unless the nested item is independently navigable or semantically important.
- Avoid long vertical pages in V1; top-level tabs use horizontal cards/carousels where needed.
- If a card can scroll vertically, its content must still have a useful above-the-fold summary.

## Typography

- Use few font sizes.
- Primary values/states must survive a squint test.
- Dense operational text should usually be at least 12 px.
- Debug-only text may be smaller if still legible in renders.
- Do not assume arbitrary emoji glyphs exist.
- Use LVGL symbols, generated icon fonts, ASCII labels, small drawn LEDs, or tiny geometric indicators.

## Color palette semantics

- Background: dark neutral, low visual noise.
- Card/panel: slightly lifted dark neutral.
- Text primary: high-contrast near-white.
- Text secondary: muted but readable gray/blue-gray.
- Accent blue/cyan: navigation, informational, weak-positive, selected tab.
- Green: confirmed-good only.
- Amber/orange: warning, degraded, on-battery, stale-with-last-known-data.
- Red: critical, fault, low battery, unavailable when primary function is broken.
- Gray: unknown, unavailable, not configured, disabled, intentionally inactive.

Do not use green for `reachable`, `configured`, `present`, `detected`, or `seen` unless the subsystem contract explicitly defines that predicate as success.

## Standard components

### Header row

Purpose:

- identify card/screen;
- show one compact status pill on the right when useful.

Rules:

- uppercase short title;
- right status pill should not duplicate the first body row;
- header status should be the strongest summary of the card.

Examples:

- `POWER SENTINEL` + `OK`
- `PROXMOX` + `ONLINE`
- `CLIENTS` + `1/3`
- `BATTERY` + `CHARGING`

### Status pill

Use sparingly.

Good uses:

- global severity;
- current tab/card availability;
- service state;
- battery state;
- connected client count.

Bad uses:

- every row has a pill;
- decorative success badges;
- repeating the same status in header and body.

### Metric row

Use for compact facts:

```text
Label        Value
```

Unavailable value renders `--`, `UNK`, or a clear unavailable label, never a fake normal value.

### Client row

For NUT clients and similar inventories:

- one row per host;
- host/name left;
- short connection/source state right;
- avoid nested card chrome.

## Tab-specific design contracts

### HOME

Job: answer “is the homelab OK, and where should I look if not?”

Must show:

- global severity;
- grid/UPS state;
- battery/runtime/load/input voltage essentials;
- compact NUT/PVE/HA/NET/M5S status;
- problem summary.

### NUT

Job: UPS telemetry, NUT service state, and connected-client visibility.

Cards should follow the current V1 shape:

- UPS;
- BATTERY;
- POWER;
- CLIENTS.

Rules:

- normal card body should avoid raw NUT tokens like `OL`/`OB`/`LB`;
- user-facing labels should say `ONLINE`, `ON BATT`, `LOW BATT`, `UNAVAILABLE`, or `STALE`;
- client display is limited to connected count and optional host list.

### NUT Ledcards Interface adaptive overview candidate

Status: selected visual direction for the next live firmware test of the NUT home/UPS overview.

This pattern is a fullscreen `1 + 4` ledcards-interface overview, validated through real LVGL MCP renders on DOOMTRAIN against the neutral reference sample. It is currently captured in:

- `assets/lvgl-spike/power-sentinel-nut-ledcards-interface-fixture.c`
- `assets/lvgl-spike/render-nut-ledcards-interface-via-doomtrain.sh`
- `assets/lvgl-spike/results/nut-ledcards-interface-mcp-6state-contact-sheet.png`
- `assets/lvgl-spike/results/nut-ledcards-interface-mcp-6state-vs-reference.png`

#### Layout contract

Canvas is 320x240 fullscreen for the candidate view. Do not force the existing 44 px sidebar into this specific overview unless explicitly choosing the integrated/non-Ledcards Interface variant.

Top micro-status row:

- left: tiny iconographic link/Wi-Fi glyph plus compact time;
- center: three carousel dots;
- right: small local/module battery glyph;
- keep it quiet and iconographic; it must not become a product header.

Hero region:

- vertical semantic LED/accent pill at `x=20 y=47 w=7 h=79 r=4`;
- hero metric at `x=43 y=47 w=120`, D-DIN Condensed Bold 60;
- hero side label at `y=47`, Montserrat 14;
- hero side unit at `y=77`, Montserrat 10;
- hero state line at `x=45 y=108`, Montserrat 14;
- contextual chart/detail button at `x=263 y=57 w=34 h=34 r=17`.

Mini-card grid:

- card size: `139x42`, radius 7;
- left column `x=15`, right column `x=166`;
- first row `y=136`, second row `y=188`;
- vertical gap is intentionally visible; do not collapse the two rows into a table block;
- card accent LED/pill at local `x=7 y=8 w=5 h=26 r=3`;
- value at local `x=20 y=10 w=58`, D-DIN Condensed Bold 32;
- label at local `x=76 y=5 w=55`, Montserrat 14, right-aligned;
- unit at local `x=78 y=23 w=53`, Montserrat 10, right-aligned.

Avoid informal abbreviations, but domain acronyms are acceptable when they preserve typographic consistency. Prefer a real acronym over shrinking one label and making it visually different from the rest. Current rule: runtime/time-to-empty is labeled `TTE` (`time to empty`) everywhere, including both hero and mini-cards. Keep the same Montserrat 14 label treatment as other mini-cards.

Use an invisible inner-frame rule for mini-cards: no element should hug the card edge. The lower unit margin must feel comparable to the upper label margin. Numeric mini-card values must be optically centered against the vertical LED/pill. Verify this with stale `--`, because short glyphs make misalignment obvious.

Avoid abbreviations unless the full label demonstrably clips or collides at 320x240. The current mini-card label budget fits `Battery`, so use `Battery` instead of `Batt`. Keep `NUT` as an acronym because it is the actual subsystem name, not an abbreviation introduced for space.

#### Typography contract

Use two separate type layers:

- Metric layer: D-DIN Condensed Bold.
  - Hero value: generated LVGL subset around 60 px.
  - Mini-card numeric values: generated LVGL subset around 32 px.
  - Use for numeric/metric values only.
- Text layer: Montserrat normal/medium feel.
  - Use for labels, units, state text, top micro-status.
  - Do not fake bold by duplicate-label offsets; it looks too chunky on 320x240.
  - Keep mini-card labels visually similar to the hero side label.

D-DIN LVGL font generation for the MCP/fixture path must use `lv_font_conv --format lvgl --no-compress --no-kerning`. The compressed/kerning-enabled output can compile but render as empty/box glyphs in the MCP simulator.

Subset metric glyphs aggressively. Current useful glyph set:

```text
0123456789:%VWhm.-
```

#### Hero metric selection

The hero value is adaptive. It shows the most useful or critical current NUT/UPS fact, not a fixed page title.

Priority:

1. On battery/TTE:
   - hero `06:24`, label `TTE`, unit `mm:ss`, state `ON BATTERY`;
   - amber accent;
   - side label block starts farther right because `06:24` is wide.
2. Low battery:
   - hero `18`, label `Battery`, unit `%`, state `LOW BATTERY`;
   - red accent;
   - side label block must be pulled close to the short value so it reads as attached to `18`.
3. Stale/unavailable telemetry:
   - hero `--`, label `NUT`, unit `stale`, state `STALE 42s` or equivalent;
   - gray accent;
   - side label block must be pulled close to the compact `--` value.
4. High/limit load:
   - hero load percent, label `Load`, state `HIGH` or `LIMIT`;
   - orange/red accent depending on the load range.
5. Abnormal input:
   - hero input voltage or `0`, label `Input`, state `INPUT LOW` or `ON BATTERY`;
   - amber/red accent.
6. Full/normal battery:
   - hero `100`, label `Battery`, unit `%`, state `FULL` at 100%;
   - state `ALMOST FULL` in the upper green battery range, or `GOOD` in the normal green range;
   - quiet green/neutral-green accent only if the battery/input condition is confirmed-good.

TTE must be clock-like. Prefer `06:24 TTE / mm:ss`. Do not render `6m24s` or wrap explanatory text under the hero value.

#### Dynamic supporting mini-card selection

The hero metric must not be duplicated in the 2x2 mini-card grid. The screen should expose five distinct facts at a glance: one in the hero plus four supporting cards.

Use this ordered supporting pool for the NUT/UPS overview:

```text
Battery, TTE, Load, Input, NUT client
```

Selection/ordering rule:

1. Pick candidate hero metric by severity/usefulness.
2. Apply a short hero-swap cooldown before accepting a different hero, so transient sensor flips do not cause rapid visual churn. Start with a few seconds; tune on hardware after observing real data jitter.
3. When a hero swap is accepted, move that metric to position `n1` in the ordered five-slot stack and keep the previous hero/order behind it by recency of hero appearance.
4. Render positions `n2` and `n3` as the first mini-card row, and positions `n4` and `n5` as the second mini-card row.
5. If no swap is accepted, preserve the current ordering.

This means the mini-card order is a product of accepted hero swaps, not a static table. The current hero is never duplicated in the mini-cards.

Examples:

- Hero `Battery` (`FULL`, `LOW BATTERY`, `CHARGING`) -> mini-cards should be `TTE`, `Load`, `Input`, `NUT client`.
- Hero `TTE` (`ON BATTERY`) -> mini-cards should be `Battery`, `Load`, `Input`, `NUT client`.
- Hero `NUT` (`STALE 42s`) -> mini-cards should be `Battery`, `TTE`, `Load`, `Input`.
- Hero `Input` -> mini-cards should be `Battery`, `TTE`, `Load`, `NUT client`.

Do not keep a mini-card just because the old static grid had it. If the hero already shows that field, swap in the next useful fact.

#### Hero state label contract

The large state label under the hero value must be the literal state name implied by the hero accent color/state, not a generic global summary.

Rules:

- Red state examples: `LOW BATTERY`, `INPUT LOST`, `UNAVAILABLE`.
- Orange state examples: `SHORT RUNTIME`, `HIGH`, `NO CLIENTS` when clients are expected.
- Yellow state examples: `ON BATTERY`, `ELEVATED LOAD`, `MARGINAL INPUT`.
- Green battery state examples: `FULL` at 100%, `ALMOST FULL` in the upper green battery range, `GOOD` in the normal green range.
- Blue/neutral state examples should be informational labels only, not success claims. For load specifically, blue means `LOW`, not good/bad.
- Gray state examples: `STALE 42s`, `UNKNOWN`, `DISABLED`.

`NOMINAL` is too generic for the hero state label in this Ledcards Interface view. For a 100% battery hero, render `FULL`; for a green-but-not-full battery hero, render `ALMOST FULL` or `GOOD` depending on the battery range. Charging overrides battery severity wording in the hero state label: if the UPS reports charging, show `CHARGING` even when the charge percentage is low; the red/yellow severity can still be carried by accent color or supporting details if needed.

#### Current six-state fixture examples

Nominal:

```text
Hero: 100  Battery  %      FULL
Tiles: 57 TTE m | 18 Load % | 226 Input V | 1 NUT client
```

On battery:

```text
Hero: 06:24  TTE  mm:ss  ON BATTERY
Tiles: 72 Battery % | 38 Load % | 0 Input V | 1 NUT client
```

Low battery:

```text
Hero: 18  Battery  %      LOW BATTERY
Tiles: 06:24 TTE mm:ss | 42 Load % | 0 Input V | 1 NUT client
```

Stale:

```text
Hero: --  NUT  stale      STALE 42s
Tiles: -- Battery % | -- TTE mm:ss | -- Load % | -- Input V
```

High load:

```text
Hero: 86  Load  %          HIGH
Tiles: 92 Battery % | 42 TTE m | 226 Input V | 1 NUT client
```

Input low:

```text
Hero: 185  Input  V        MARGINAL INPUT
Tiles: 88 Battery % | 51 TTE m | 24 Load % | 1 NUT client
```

NUT overview semantics remain read-only: NUT means service/UPS telemetry plus connected client count/list only.

#### Color/style contract

Base colors:

- Background: near black `0x040607`.
- Primary text: near white `0xf5f6f2`.
- Hero label text: muted warm gray `0xbeb8a0`.
- Mini-card label text: muted green-gray `0xc9d0c9`.
- Unit text: subdued gray-green `0x87918c`.
- Card border/lift: `0x141e1b` only as a very quiet separation edge.

Semantic accent palette:

| Role | Accent | Code | Use |
| --- | --- | --- | --- |
| confirmed-good | green | `0x14dc78` | Only when the specific field contract defines the condition as actually good/healthy. |
| neutral telemetry/info | cyan/blue | `0x1cb5f0` | Values with no inherent good/bad polarity, e.g. connected-client count when non-zero, model/capacity facts, inventory counts. |
| caution | yellow | `0xfcca3d` | Early warning, degraded but not urgent, elevated load, on-battery when runtime is still acceptable. |
| warning | orange | `0xff8a2a` | Needs attention soon, e.g. zero clients when at least one is expected, high-but-not-critical load, short runtime. |
| critical/fault | red | `0xff4e3e` / `0xff6a57` | Low battery, input lost, unavailable primary telemetry, critical load/runtime. |
| stale/unknown/inactive | gray | `0x6c7470` / `0xc1c5c1` | Unknown, stale, disabled, intentionally inactive, unavailable values rendered as `--`. |

Do not use green for mere reachability, presence, configuration, or a connected count. Cyan/blue is the default for neutral facts that are useful but not inherently healthy. Orange/yellow/red require a field-specific threshold below.

NUT/UPS mini-card range rules:

| Field | Value/range | Accent | Notes |
| --- | --- | --- | --- |
| `Battery` | `>= 50%` and not on battery/low battery | green `0x14dc78` | Confirmed adequate battery. |
| `Battery` | `20-49%` | yellow `0xfcca3d` | Degraded reserve. |
| `Battery` | `< 20%` or backend low-battery flag | red `0xff4e3e` | Critical reserve; should usually drive the hero. |
| `Battery` | unknown/stale | gray `0x6c7470` | Render value as `--`; never fake nominal. |
| `TTE` hero | `>= 10 min` while on battery | yellow `0xfcca3d` | On-battery is still abnormal even with acceptable runtime. |
| `TTE` hero | `5-9 min 59 s` | orange `0xff8a2a` | Short reserve. |
| `TTE` hero | `< 5 min` | red `0xff4e3e` | Critical runtime. |
| `TTE` hero | unknown while on battery | red `0xff4e3e` | Unknown runtime during outage is unsafe to present as mild. |
| `Load` | `< 10%` | blue `0x1cb5f0` | `LOW`: useful neutral/low-utilization fact, not a success state. |
| `Load` | `10-69%` | green `0x14dc78` | `NORMAL`: normal operating load. |
| `Load` | `70-89%` | orange `0xff8a2a` | `HIGH`: high load, attention soon. |
| `Load` | `>= 90%` | red `0xff4e3e` | `LIMIT`: near/at limit or overload risk. |
| `Load` | unknown/stale | gray `0x6c7470` | Render `--`. |
| `Input` | nominal mains voltage present inside configured good band | green `0x14dc78` | Input voltage is not neutral telemetry; it has good/warn/fault ranges. For 230 V systems, a provisional good band is roughly 207-253 V until backend thresholds are configured. |
| `Input` | marginal voltage outside good band but still usable | yellow `0xfcca3d` | Use backend/configured thresholds where available. |
| `Input` | poor voltage outside warning band but still present | orange `0xff8a2a` | Needs attention; exact thresholds should come from UPS/backend policy. |
| `Input` | `0 V`, grid lost, or input unavailable while UPS is live | red `0xff4e3e` | Should align with `ON BATTERY`/fault semantics. |
| `Input` | unknown/stale | gray `0x6c7470` | Render `--`; do not show old voltage as live. |
| `NUT client` | `>= 1` connected client | blue `0x1cb5f0` | Neutral informational count, not “healthy green”. |
| `NUT client` | `0` connected clients when at least one is expected | orange `0xff8a2a` | Warning/attention, not critical by itself. |
| `NUT client` | unknown/stale | gray `0x6c7470` | Render `--` or stale. |
| `NUT service/driver` | telemetry fresh and service reachable | green only if the service contract explicitly defines all required checks | Avoid generic green for `reachable_via_upsc` alone. |
| `NUT service/driver` | telemetry stale/unreachable | gray or red depending on whether primary telemetry is broken | Stale data should be visually distinct from normal. |

The current fixture should be adjusted to this rule set when the next render pass touches color semantics: `NUT/client` non-zero should move from green to cyan/blue, and `0` clients should use orange only when at least one client is expected. Keep the tab read-only: these colors describe telemetry and connected-client visibility.

Avoid blur, glassmorphism, shadows, translucent overlays, and decorative gradients. The reference glass/gloss is treated as physical display/photo behavior, not software style.

#### Validation contract

For this pattern, PIL/raster mocks are exploratory only. Before promoting changes, render real LVGL screenshots through DOOMTRAIN MCP and compare against the Ledcards Interface reference.

Minimum render states:

- nominal;
- on-battery/runtime;
- low-battery;
- stale/unavailable.

Inspect at minimum:

- hero value is not vertically too high and aligns with the hero LED top intent;
- side label block aligns to the hero value, with shorter values getting closer labels;
- `06:24` does not collide with `TTE/mm:ss` or chart button;
- mini-card values are centered on the LED/pill, including stale `--`;
- unit labels `%`, `V`, `client` do not hug the bottom edge;
- labels are not fake-bold/chunky;
- no clipping in all four states;
- semantic colors match state severity.

### PVE

Job: show whether the Proxmox node is operational.

Rules:

- show node/API availability, CPU/RAM/storage, ZFS/SMART, and active interface;
- storage should use the aggregate Total Node Capacity when available;
- VM/LXC details are secondary and must not crowd out node health.

### HA

Job: show Home Assistant/MQTT/Zigbee2MQTT health, not an entity dashboard.

Rules:

- no arbitrary entity list on the CoreS3 overview;
- distinguish HA API reachability, MQTT broker health, Zigbee2MQTT bridge/coordinator state, and update count if available;
- HA Supervisor API is out of scope for V1.

### M5S

Job: show health of the M5Stack appliance and data path.

Rules:

- include stack/local service health, temperature, RAM/disk, StackFlow/API freshness, and LLM/OpenAI smoke state if present;
- `OpenAI FAIL` is warning, not critical, because it does not break the primary power-monitor function;
- this is the right place for transport/data freshness diagnostics, not HOME.

## Anti-patterns to reject

Reject or refactor these during review:

- cardocalypse: cards inside cards inside cards;
- pill wall: every field becomes a rounded badge;
- raw telemetry dump;
- large decorative padding;
- low-contrast gray text on dark/colored backgrounds;
- green for weak reachability;
- emoji status symbols without proven font coverage;
- full-screen animation or opacity-heavy effects on changing regions;
- recreating LVGL widgets on every data update;
- screenshots of only the first carousel card after changing multiple cards;
- fixture renders that do not match firmware UI topology.

## Implementation rules

LVGL implementation should:

- create widgets once;
- initialize styles once;
- update only changed labels, bars, colors, flags, or visibility;
- cache previous rendered strings when practical;
- make unavailable/stale values explicit;
- keep UI construction, data normalization, display driver, and transport/action bindings separate.

## Review checklist

Before merging a UI change:

- device profile still matches CoreS3 320x240 landscape;
- changed screens/cards have a clear HMI level and job;
- colors match semantic state, not optimism;
- no hidden sample/demo values are introduced;
- long/stale/unavailable cases are considered;
- firmware and fixture are updated together;
- every changed card/state is rendered;
- contact sheet or equivalent visual evidence is inspected;
- validation gaps are documented.

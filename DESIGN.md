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

- treat the hero as one large invisible card region at `x=12 y=25 w=296 h=91`; this is the animation object boundary even though the normal state has no visible card fill;
- vertical semantic LED/accent pill inside that card at local `x=7 y=8 w=7 h=76 r=4` (absolute `x=19 y=33`), aligned to the mini-card LED column;
- hero metric at local `x=31 y=8 w=120`, D-DIN Condensed Bold 60 (absolute `x=43 y=33`);
- hero side label at local `y=8`, Montserrat 12;
- hero side unit at local `y=38`, Montserrat 12;
- hero state line at local `x=33 y=71 w=142`, Montserrat 14 (absolute `x=45 y=96`);
- contextual chart/detail hitbox at local `x=262 y=8 w=34 h=34` (absolute `x=274 y=33`), containing the bare chart icon. It is top-aligned with the hero LED pill/value and its right edge lands at `x=308`, matching the right mini-card edge rather than protruding farther toward the display border.

Mini-card grid:

- card size: `142x46`, radius 7;
- left column `x=12`, right column `x=166`;
- first row `y=124`, second row `y=182`;
- outer margins, center gap, row gap, and bottom margin are all 12 px;
- card accent LED/pill at local `x=7 y=8 w=5 h=28 r=3`, so the left mini-card LED aligns at absolute `x=19` with the hero LED;
- value at local `x=20 y=8 w=58`, D-DIN Condensed Bold 40;
- label at local `x=76 y=6 w=55`, Montserrat 12, right-aligned;
- unit at local `x=78 y=23 w=53`, Montserrat 12, right-aligned;
- create the label/unit objects before the D-DIN value so the numeric metric is the foreground LVGL sibling on the physical TFT.

Current rule: runtime/time-to-empty is labeled `Runtime` everywhere, including both hero and mini-cards. Hero side label, hero unit, mini-card label, and mini-card unit all use Montserrat 12; hierarchy comes from color and placement rather than size mismatch.

Runtime formatting is context-sensitive: the hero uses full clock form (`mm:ss`) because it has the visual budget; mini-cards use rounded whole minutes with generic unit `m` because values can be single digit and the compact card should not imply clock-form precision.

Use an invisible inner-frame rule for mini-cards: no element should hug the card edge. The lower unit margin must feel comparable to the upper label margin. Numeric mini-card values must be optically centered against the vertical LED/pill. Verify this with stale `--`, because short glyphs make misalignment obvious.

Avoid abbreviations unless the full label demonstrably clips or collides at 320x240. The current mini-card label budget fits `Battery`, so use `Battery` instead of `Batt`. Keep `NUT` as an acronym because it is the actual subsystem name, not an abbreviation introduced for space.

#### Typography contract

Use two separate type layers:

- Metric layer: D-DIN Condensed Bold.
  - Hero value: generated LVGL subset around 60 px.
  - Mini-card numeric values: generated LVGL subset around 40 px.
  - Use for numeric/metric values only.
- Text layer: Montserrat normal/medium feel.
  - Use for labels, units, state text, top micro-status.
  - Do not fake bold by duplicate-label offsets; it looks too chunky on 320x240.
  - Keep mini-card labels and units the same Montserrat 12 size as the hero side label/unit.

D-DIN LVGL font generation for the MCP/fixture path must use `lv_font_conv --format lvgl --no-compress --no-kerning`. The compressed/kerning-enabled output can compile but render as empty/box glyphs in the MCP simulator.

Subset metric glyphs aggressively. Current useful glyph set:

```text
0123456789:%VWhm.-
```

#### Hero metric selection

The hero value is adaptive. It shows the most useful or critical current NUT/UPS fact, not a fixed page title.

Priority list — GRID ONLINE:

| Priority | Condition/range | Hero metric | Hero label/unit | State label | Accent/color |
| --- | --- | --- | --- | --- | --- |
| 1 | UPS/source unavailable (`upsAvailable=false`) | NUT | `NUT` / `client`, value `--` | `UNAVAILABLE` | purple `0x9b5cff` |
| 1 | Telemetry existed but is stale/offline (`offline` or `upsStale`) | NUT | `NUT` / `client`, value `--` | `STALE` | gray `0x6c7470` |
| 2 | Battery `<20%` or backend low-battery flag while grid is online | Battery | `Battery` / `%` | `LOW BATTERY` | red `0xff4e3e` |
| 3 | Load `>=90%` | Load | `Load` / `%` | `OVERLOAD` | red `0xff4e3e` |
| 3 | Load `70-89%` | Load | `Load` / `%` | `HIGH LOAD` | orange `0xff8a2a` |
| 4 | Input `190-209 V` | Input | `Input` / `V` | `MARGINAL INPUT` | yellow `0xfcca3d` |
| 4 | Input `>0` and `<190 V` | Input | `Input` / `V` | `INPUT LOW` | orange `0xff8a2a` |
| 5 | Battery `20-49%` | Battery | `Battery` / `%` | `CHARGING` | orange `0xff8a2a` |
| 5 | Battery `50-89%` | Battery | `Battery` / `%` | `ALMOST FULL` | yellow `0xfcca3d` |
| 5 | Battery `90-100%` | Battery | `Battery` / `%` | `FULL` | green `0x14dc78` |

Priority list — GRID OFFLINE:

| Priority | Condition/range | Hero metric | Hero label/unit | State label | Accent/color |
| --- | --- | --- | --- | --- | --- |
| 1 | UPS/source unavailable (`upsAvailable=false`) | NUT | `NUT` / `client`, value `--` | `UNAVAILABLE` | purple `0x9b5cff` |
| 1 | Telemetry existed but is stale/offline (`offline` or `upsStale`) | NUT | `NUT` / `client`, value `--` | `STALE` | gray `0x6c7470` |
| 2 | Runtime `<2:00` | Runtime | `Runtime` / `mm:ss` | `CRITICAL RUNTIME` | red `0xff4e3e` |
| 3 | Battery `<10%` | Battery | `Battery` / `%` | `CRITICAL BATTERY` | red `0xff4e3e` |
| 3 | Battery `10-19%` or backend low-battery flag | Battery | `Battery` / `%` | `LOW BATTERY` | orange `0xff8a2a` |
| 4 | Runtime `2:00-4:59` | Runtime | `Runtime` / `mm:ss` | `SHORT RUNTIME` | orange `0xff8a2a` |
| 5 | Runtime `>=5:00` | Runtime | `Runtime` / `mm:ss` | `ON BATTERY` | yellow `0xfcca3d` |
| 6 | Runtime unavailable, battery not low, load `>=90%` | Load | `Load` / `%` | `OVERLOAD` | red `0xff4e3e` |
| 6 | Runtime unavailable, battery not low, load `70-89%` | Load | `Load` / `%` | `HIGH LOAD` | orange `0xff8a2a` |

Full label/range/color contract for the five metrics is below in the color/range table; this priority table only decides which metric reaches the hero first.

Hero Runtime must be clock-like. Prefer `06:24 Runtime / mm:ss`. Mini-card Runtime must be rounded whole minutes with generic unit `m`, e.g. `6 Runtime / m` for `06:24`. Do not render `6m24s` or wrap explanatory text under the hero value.

#### Dynamic supporting mini-card selection

The hero metric must not be duplicated in the 2x2 mini-card grid. The screen should expose five distinct facts at a glance: one in the hero plus four supporting cards.

Use this ordered supporting pool for the NUT/UPS overview:

```text
Battery, Runtime, Load, Input, NUT client
```

Selection/ordering rule:

1. Pick candidate hero metric by severity/usefulness.
2. Apply a short hero-swap cooldown before accepting a different hero, so transient sensor flips do not cause rapid visual churn. Start with a few seconds; tune on hardware after observing real data jitter.
3. When a hero swap is accepted, choose the shortest direction on the physical five-slot loop, then rotate the entire ring in that direction until the selected metric reaches `HERO`; do not move/promote it as a list item.
4. Render slot `1` as top-right, slot `2` as bottom-right, slot `3` as bottom-left, and slot `4` as top-left.
5. If no swap is accepted, preserve the current ring order.
6. A mini-card tap may temporarily rotate that metric to hero for 60 seconds. When the override expires, normal severity/priority hero selection resumes and, if the priority metric differs from the current hero, runs the same ring animation path again.

This means the mini-card order is a product of accepted ring rotations, not a static table or row-major list. The current hero is never duplicated in the mini-cards.

Examples:

- Mini-card slot order is a physical bidirectional ring, not a row-major list.
- Forward path: `HERO -> top-right -> bottom-right -> bottom-left -> top-left -> HERO`.
- Reverse path: `HERO -> top-left -> bottom-left -> bottom-right -> top-right -> HERO`.
- Accepted hero changes rotate the whole ring along the shortest of those two paths, preserving circular order for chain-style animation.
- Hero `Battery` (`FULL`, `LOW BATTERY`, `CHARGING`) from the default ring -> mini-cards should be `Runtime` top-right, `Load` bottom-right, `Input` bottom-left, `NUT client` top-left.
- Hero `Runtime` (`ON BATTERY`) from the default ring -> mini-cards should be `Load` top-right, `Input` bottom-right, `NUT client` bottom-left, `Battery` top-left.
- Hero `NUT` (`STALE 42s`) from the default ring -> mini-cards should be `Battery` top-right, `Runtime` bottom-right, `Load` bottom-left, `Input` top-left.
- Hero `Input` from the default ring -> mini-cards should be `NUT client` top-right, `Battery` bottom-right, `Runtime` bottom-left, `Load` top-left.

Do not keep a mini-card just because the old static grid had it. If the hero already shows that field, swap in the next useful fact.

Animation contract:

- Accepted automatic swaps, the post-60-second priority resume after a manual override, and touch overrides all use the same visual transition path in `start_ring_transition(...)`.
- The hero is a real large invisible card region (`296x91`) for animation purposes. A metric crossing `HERO` must be drawn as the full-size hero card and slide horizontally left or right depending on rotation direction.
- Mini-card ghosts must never invade the hero region. They animate only within the mini-card ring lanes: right-column vertical moves, bottom-row horizontal moves, and left-column vertical moves.
- Use a full-screen overlay during animation to mask the old static hero lane, block touches, and queue the latest telemetry view for final redraw.
- The final committed frame must normalize back to the exact hero-card and mini-card templates above.
- Timing is weighted by visual path length, not by fixed logical steps. Hero crossings are weighted heavier than mini-card lane segments so the large hero card has more visual inertia.
- All cards in a transition share one global duration derived from the longest weighted path, clamped to the current tested range of roughly `360..720 ms`. Segment progress is distance-based along each path, so long visual segments receive more of the timeline.
- Use a subtle per-link mini-stagger (`18 ms` per chain link, under ~72 ms total) so the selected/priority target leads and the rest of the ring follows without feeling laggy.
- Use a gentle ease-in-out path for the chain. Avoid blur, glass, shadows, full-screen transforms, or expensive opacity over large regions.
- Static fallback/final render must preserve the ring semantics even if animation is unavailable.

#### Hero state label contract

The large state label under the hero value must be the literal state implied by the hero metric, grid state, and severity. Keep GRID ONLINE and GRID OFFLINE semantics separate.

Battery:
- GRID ONLINE: `LOW BATTERY`, `CHARGING`, `FULL`, `ALMOST FULL`, `STALE`, `UNAVAILABLE`.
- GRID OFFLINE: `ON BATTERY`, `LOW BATTERY`, `CRITICAL BATTERY`, `STALE`, `UNAVAILABLE`.

For GRID ONLINE battery, use `FULL` at 90-100%, `ALMOST FULL` at 50-89%, and `CHARGING` at 20-49% instead of reintroducing `GOOD`.

Runtime:
- GRID ONLINE: `RESERVE`, `LOW RESERVE`, `CRITICAL RESERVE`, `STALE`, `UNAVAILABLE`.
- GRID OFFLINE: `ON BATTERY`, `SHORT RUNTIME`, `CRITICAL RUNTIME`, `STALE`, `UNAVAILABLE`.

Load:
- GRID ONLINE: `LOW`, `NORMAL`, `HIGH LOAD`, `OVERLOAD`, `STALE`, `UNAVAILABLE`.
- GRID OFFLINE: `LOW`, `NORMAL`, `HIGH LOAD`, `OVERLOAD`, `STALE`, `UNAVAILABLE`.

Input:
- GRID ONLINE: `GRID ONLINE`, `MARGINAL INPUT`, `INPUT LOW`, `STALE`, `UNAVAILABLE`.
- GRID OFFLINE: `GRID OFFLINE`, `STALE`, `UNAVAILABLE`.

NUT:
- GRID ONLINE: `CLIENTS`, `NO CLIENTS`, `STALE`, `UNAVAILABLE`.
- GRID OFFLINE: `CLIENTS`, `NO CLIENTS`, `STALE`, `UNAVAILABLE`.

`NOMINAL`, `GOOD`, and generic `UNKNOWN` are not Ledcards Interface hero labels for this view. Distinguish `STALE` from `UNAVAILABLE`: stale means a source existed but is old/offline and stays gray; unavailable means the UPS/source is not available and uses purple.

#### Current six-state fixture examples

Nominal:

```text
Hero: 100  Battery  %      FULL
Tiles ring: TR 57 Runtime m | BR 18 Load % | BL 226 Input V | TL 1 NUT client
```

On battery:

```text
Hero: 06:24  Runtime  mm:ss  ON BATTERY
Tiles ring: TR 38 Load % | BR 0 Input V | BL 1 NUT client | TL 72 Battery %
```

Low battery:

```text
Hero: 18  Battery  %      LOW BATTERY
Tiles ring: TR 6 Runtime m | BR 42 Load % | BL 0 Input V | TL 1 NUT client
```

Stale:

```text
Hero: --  NUT  stale      STALE 42s
Tiles ring: TR -- Battery % | BR -- Runtime m | BL -- Load % | TL -- Input V
```

High load:

```text
Hero: 86  Load  %          HIGH
Tiles ring: TR 226 Input V | BR 1 NUT client | BL 92 Battery % | TL 42 Runtime m
```

Input low:

```text
Hero: 185  Input  V        MARGINAL INPUT
Tiles ring: TR 1 NUT client | BR 88 Battery % | BL 51 Runtime m | TL 24 Load %
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
| critical/fault | red | `0xff4e3e` / `0xff6a57` | Low battery, input lost/grid offline, critical load/runtime. UPS/source unavailable is purple, not red. |
| stale/inactive | gray | `0x6c7470` / `0xc1c5c1` | Stale, disabled, intentionally inactive, old values rendered as `--`. |
| unavailable | purple | `0x9b5cff` / `0xc4b5fd` | UPS/source unavailable; distinguish from stale data. |

Do not use green for mere reachability, presence, configuration, or a connected count. Cyan/blue is the default for neutral facts that are useful but not inherently healthy. Orange/yellow/red require a field-specific threshold below.

NUT/UPS mini-card range rules:

| Field | Value/range | Accent | Notes |
| --- | --- | --- | --- |
| `Battery` online | `< 20%` or backend low-battery flag | red `0xff4e3e` | `LOW BATTERY`; should usually drive the hero. |
| `Battery` online | `20-49%` | orange `0xff8a2a` | `CHARGING`: lower normal/charging band. |
| `Battery` online | `50-89%` | yellow `0xfcca3d` | `ALMOST FULL`. |
| `Battery` online | `90-100%` | green `0x14dc78` | `FULL`. |
| `Battery` offline | `< 10%` | red `0xff4e3e` | `CRITICAL BATTERY`. |
| `Battery` offline | `10-19%` or backend low-battery flag | orange `0xff8a2a` | `LOW BATTERY`. |
| `Battery` offline | `>= 20%` | yellow `0xfcca3d` | `ON BATTERY`. |
| `Battery` | stale | gray `0x6c7470` | Render value as `--`; never fake nominal. |
| `Battery` | unavailable | purple `0x9b5cff` | Render value as `--`; source unavailable. |
| `Runtime` online | `>= 5 min` | blue `0x1cb5f0` | `RESERVE`. |
| `Runtime` online | `2-4 min 59 s` | yellow `0xfcca3d` | `LOW RESERVE`. |
| `Runtime` online | `< 2 min` | red `0xff4e3e` | `CRITICAL RESERVE`. |
| `Runtime` offline | `>= 5 min` | yellow `0xfcca3d` | `ON BATTERY`. |
| `Runtime` offline | `2-4 min 59 s` | orange `0xff8a2a` | `SHORT RUNTIME`. |
| `Runtime` offline | `< 2 min` | red `0xff4e3e` | `CRITICAL RUNTIME`. |
| `Load` | `< 10%` | blue `0x1cb5f0` | `LOW`: useful neutral/low-utilization fact, not a success state. |
| `Load` | `10-69%` | green `0x14dc78` | `NORMAL`: normal operating load. |
| `Load` | `70-89%` | orange `0xff8a2a` | `HIGH LOAD`: high load, attention soon. |
| `Load` | `>= 90%` | red `0xff4e3e` | `OVERLOAD`: near/at limit or overload risk. |
| `Load` | stale | gray `0x6c7470` | Render `--`. |
| `Load` | unavailable | purple `0x9b5cff` | Render `--`. |
| `Input` online | `>= 210 V` | green `0x14dc78` | `GRID ONLINE`. Input voltage is not neutral telemetry; it has good/warn/fault ranges. |
| `Input` online | `190-209 V` | yellow `0xfcca3d` | `MARGINAL INPUT`. |
| `Input` online | `>0` and `<190 V` | orange `0xff8a2a` | `INPUT LOW`. |
| `Input` offline | `0 V` / on-battery grid lost | red `0xff4e3e` | `GRID OFFLINE`; do not label this `INPUT LOST` in the hero contract. |
| `Input` | stale | gray `0x6c7470` | Render `--`; do not show old voltage as live. |
| `Input` | unavailable | purple `0x9b5cff` | Render `--`. |
| `NUT client` | `>= 1` connected client | blue `0x1cb5f0` | Neutral informational count, not “healthy green”. |
| `NUT client` | `0` connected clients | orange `0xff8a2a` | `NO CLIENTS`; warning/attention, not critical by itself. |
| `NUT client` | stale | gray `0x6c7470` | Render `--` or stale. |
| `NUT client` | unavailable | purple `0x9b5cff` | Render `--`. |
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
- hero `06:24` does not collide with `Runtime/mm:ss` or chart button;
- mini-card Runtime uses rounded minutes with generic unit `m`, not clock-form `mm:ss`;
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

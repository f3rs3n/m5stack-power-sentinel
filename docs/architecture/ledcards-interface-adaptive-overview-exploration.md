# Ledcards Interface adaptive overview exploration

Status: exploration note. Surviving decisions have been promoted into `DESIGN.md` under `NUT Ledcards Interface adaptive overview candidate`; treat `DESIGN.md` as the canonical design contract.

## Source reference

The reference is a neutral ledcards-interface appliance screen reviewed during UI exploration: dark TFT surface, one dominant metric, compact supporting metric tiles, vertical semantic accents, a small contextual round action at the right of the hero metric, and minimal carousel dots.

The apparent gloss/glass effect in the reference is treated as a rendering/photo effect from the physical display glass, not a software effect to reproduce. Power Sentinel should not implement blur, glassmorphism, expensive shadows, or decorative transparency for this pattern.

## Core pattern

Use a `1 + 4` structure:

- one large adaptive hero metric;
- four smaller supporting metric cards in a 2x2 grid;
- a contextual round chart/detail button bound to the currently displayed hero metric;
- compact carousel dots instead of a heavy top bar where possible;
- dark, high-contrast background;
- short labels and strong numeric hierarchy;
- vertical colored accents for semantic state.

This is preferred over the earlier `1 + 6` interpretation for CoreS3 because the 320x240 target leaves enough room for a near-direct adaptation of the reference design without crushing typography.

## Dynamic hero metric

The hero metric is not fixed. It should show the most critical or most useful current value for the selected dashboard/card.

For NUT/UPS, candidate priority order:

1. `on_battery` / grid lost
   - hero value: runtime remaining in compact clock form, e.g. `06:24`
   - metric label/unit block at the right of the value, Ledcards Interface: `Runtime` above `mm:ss`
   - state: `ON BATTERY`
   - accent: amber/red depending on severity
   - avoid wrapping explanatory text below the hero value; keep hero Runtime as `mm:ss`, but use rounded whole minutes in supporting mini-cards
2. low battery
   - hero value: battery percent, e.g. `18%`
   - label: `Battery`
   - state: `LOW BATTERY`
   - accent: red/amber
3. stale or unavailable UPS/NUT data
   - hero value: `--`
   - label: `UPS` or `NUT`
   - state: `STALE 42s` / `UNREACHABLE`
   - accent: gray/red depending on condition
4. high/limit UPS load
   - hero value: load percent, e.g. `86%`
   - label: `Load`
   - state: `HIGH` or `LIMIT`
   - accent: orange/red depending on range
5. abnormal input voltage
   - hero value: input voltage, e.g. `185V` or `0V`
   - label: `Input`
   - state: `INPUT LOW` / `ON BATTERY`
   - accent: amber/red
6. nominal/full battery state
   - hero value: battery percent or runtime, e.g. `100%`
   - label: `Battery`
   - state: `FULL` at 100%, `ALMOST FULL` in the upper green battery range, or `GOOD` in the normal green range
   - accent: green only if the product semantics define the battery/input condition as confirmed-good.

Hero state label rule: the large label below the hero value should be the literal state associated with the color/severity, not a generic summary. `NOMINAL` is too generic for the Ledcards Interface hero. Charging overrides battery charge wording: show `CHARGING` if the UPS reports charging, even when the current percentage is low.

Dynamic mini-card rule: do not duplicate the hero metric in the 2x2 supporting grid. The overview exposes five distinct facts as a directional ring: `HERO -> top-right -> bottom-right -> bottom-left -> top-left -> HERO`. The default ring starts as `Battery`, `Runtime`, `Load`, `Input`, `NUT client`. Accepted hero changes rotate the whole ring clockwise/forward until the selected/candidate metric reaches `HERO`; do not reverse direction to take a shorter counter-clockwise path. Mini-cards are then the next four ring slots. Example: when low battery keeps `Battery` in the hero, `Runtime` is top-right, `Load` bottom-right, `Input` bottom-left, and `NUT client` top-left.

Label wording rule: use `Runtime` for time-to-empty everywhere now that mini-cards hide seconds and show rounded minutes; keep hero and mini-card labels consistent.

Hero swap rule: severity still picks the candidate hero, but accepted hero changes must pass a short cooldown of a few seconds to avoid flicker. When a swap is accepted, rotate the five-slot ring clockwise/forward, preserving circular order, until that metric reaches `HERO`. If the candidate changes during cooldown, keep the current hero/order stable until a swap is accepted.

The hero should always use a compact numeric/token value. Long explanations belong in the state line or detail page, not in the hero value.

## Contextual round button

The round button on the right of the hero region means: open the temporal chart or diagnostic detail for the metric currently shown in the hero.

Examples:

- hero `Runtime` -> opens runtime/battery/load trend;
- hero `Battery` -> opens battery history;
- hero `Input` -> opens input-voltage history;
- hero `Load` -> opens load history;
- hero `UPS stale` -> opens source freshness / diagnostic detail instead of a fake chart.

The button should not imply a generic menu. It is context-bound to the active hero metric.

## Top micro-status row and carousel dots

The top row should be mostly iconographic. It may keep compact status indicators for local Wi-Fi/link, time, and CoreS3/module battery, but it should not become a product top bar.

Recommended structure:

- left: Wi-Fi/link icon plus compact time, e.g. `⌁ 10:23` or an LVGL symbol/icon equivalent;
- center: carousel dots;
- right: small battery glyph/state for the CoreS3/module battery or local power state.

The dots indicate horizontal dashboard/card position while consuming very little vertical space.

For the existing Power Sentinel interaction model:

- vertical swipe changes tab;
- horizontal swipe changes card inside the current tab;
- the dots represent the horizontal card position inside the current tab.

If a future design removes the left sidebar for a more Ledcards Interface-like fullscreen surface, the dots can become the primary dashboard position indicator.

## NUT/UPS home-card content proposal

Default supporting 2x2 facts:

- `Battery` — battery percent;
- `Load` — UPS load percent or watts when more relevant;
- `Input` — input voltage / grid presence;
- `NUT` or `Clients` — local NUT server/client status, using only read-only telemetry and connected client count/list.

Prefer full labels unless the full wording demonstrably clips or collides at 320x240. `Battery` fits in the current mini-card budget, so do not abbreviate it to `Batt`. Avoid long explanatory text such as `Connected clients`, `Input voltage`, or hostnames in this overview.

Example nominal:

```text
100   Battery   %      FULL
1     NUT       client 57   Runtime    m
226   Input     V      18   Load   %
```

Example power event:

```text
06:24  Runtime      mm:ss  ON BATTERY
72     Battery  %      38   Load   %
1      NUT      client 0    Input  V
```

Example stale:

```text
--     UPS      stale  STALE 42s
--     Input    V      --   Battery %
--     Load     %      --   Runtime     m
```

## Typography

Current direction after the mock review:

- Metric layer: D-DIN Condensed Bold.
  - Use it for all primary numeric telemetry, not only the hero.
  - This includes the hero value and the numeric values inside the 2x2 mini-cards.
  - Reason: it preserves horizontal room for runtime, label/unit blocks, chart affordance, and future long states.
  - Using the same numeric face for hero and mini-cards gives the metric layer one coherent instrument identity.
- Text layer: Montserrat Medium/SemiBold.
  - Use it for labels, units, state text, top micro-status, and other non-numeric explanatory text.
  - Reason: it stays crisp in mini-card labels and matches the compact appliance label feel.
- Do not depend on an exact Ledcards Interface font match unless it is identified from an official/redistributable source. Treat the reference as a metric/typographic direction: tall condensed DIN-like numerals with strong weight and compact clock punctuation.

Recommended approximate sizes for the 320x240 fullscreen mock:

```text
Hero numeric value:      D-DIN Condensed Bold ~60 px
Mini-card numeric value: D-DIN Condensed Bold ~30-32 px
Hero label/unit:         Montserrat SemiBold/Medium ~11-14 px
Mini-card label/unit:    Montserrat SemiBold/Medium ~11-12 px
Top micro-status:        Montserrat Medium ~10 px plus drawn/LVGL icons
```

LVGL font implementation:

- Generate D-DIN Condensed Bold as a custom subset via `lv_font_conv`.
- Keep labels on normal Montserrat.
- Subset the metric font aggressively: digits, `:`, `%`, `V`, `W`, `m`, `h`, `-`, and only any additional glyphs actually used in numeric values.

## LVGL implementation notes

- Build with simple `lv_obj` containers, `lv_label`, vertical accent bars, and one small custom circular action button.
- Create all widgets once and update only label text, accent colors, and state styles.
- Avoid software blur, glass, heavy shadows, and animated full-screen transitions.
- Built-in LVGL Montserrat is available in normal widths only; `LV_FONT_MONTSERRAT_28_COMPRESSED` is storage-compressed, not visually condensed.
- Validate with rendered 320x240 fixtures for nominal, on-battery, low-battery, stale, and high-load states.
- Test every changed carousel card, not only the first visible card.


## Live firmware mapping

The fullscreen Ledcards Interface build now uses live data rather than `PS_NUT_HOME_STATE` fixture constants. `main.cpp` normalizes the existing `SummaryState` into a narrow `LedcardsInterfaceNutView`; `ledcards-interface-page.cpp` owns only display logic. Current physical geometry: hero top `33`, hero accent `19,33,7,76`, hero state `45,96,142`, chart button hitbox `263,56,34,34`, mini rows `124` and `182`, mini-card size `142x46`, left/right margins `12`, inter-card gaps `12`, bottom margin `12`, accent height `28`.

Runtime behavior:

1. Hero candidate priority is stale/unavailable -> `NUT`, low battery -> `Battery`, on battery -> `Runtime`, high load -> `Load`, marginal input -> `Input`, otherwise `Battery`.
2. The active hero is held behind `kHeroCooldownMs` so noisy telemetry does not flicker the whole page.
3. Accepted hero swaps rotate the directional five-slot ring clockwise (`HERO -> top-right -> bottom-right -> bottom-left -> top-left -> HERO`) until that metric reaches `HERO`. The four mini-cards are the next four ring slots, so the hero never appears twice and circular order is preserved.
4. The live page now animates accepted automatic swaps and touch overrides as a clockwise-only chain with constant perceived speed: each ring segment takes about 250 ms, so farther mini-cards get proportionally longer transitions instead of accelerating through the same fixed duration. Compact ghost cards scroll only inside the mini-card grid lanes, touch is blocked during the transition, small ghost-card fades handle the hero boundary, and the final frame redraws with the exact hero/mini templates.
5. Tapping a mini-card temporarily rotates that metric to the hero for 60 seconds, overriding the default priority. After expiry the normal severity/priority hero selection resumes and may rotate the ring again after cooldown.
6. Unknown/stale values render as `--` with gray accent/fill. No live firmware path uses the previous compile-time demo constants.
7. `Runtime` is context-sensitive: hero uses full `mm:ss`; mini-cards use rounded whole minutes with generic unit `m` to avoid physical TFT clipping in the 142x46 card geometry and to fit single-digit values cleanly.
8. `NUT` client count is cyan for one or more clients, orange for zero expected clients, and gray for unknown/stale. It remains read-only telemetry only.

## Render artifacts from exploration

The current firmware-test candidate is a real LVGL MCP fixture render, not a PIL/raster mock:

- `firmware/core-s3-display/src/ledcards-interface-page.cpp` — live firmware page implementation. It consumes `LedcardsInterfaceNutView`, preserves the accepted Ledcards Interface geometry, selects the hero from live telemetry, and renders mini-cards from the remaining four metrics.
- `firmware/core-s3-display/src/ledcards-interface-page.h` — small view adapter boundary between `main.cpp`/`SummaryState` and the Ledcards Interface renderer.
- `assets/lvgl-spike/power-sentinel-nut-ledcards-interface-fixture.c` — focused exploratory LVGL fixture for the fullscreen NUT/UPS adaptive overview.
- `assets/lvgl-spike/ps_font_ddin_condensed_bold_60.c` — D-DIN metric font subset for the hero value.
- `assets/lvgl-spike/ps_font_ddin_condensed_bold_40.c` — D-DIN metric font subset for mini-card numeric values, reduced from the earlier 42 px subset to prevent Runtime clipping while staying close to the reference sample. The current mini-card LED is 28 px high at local `x=7, y=8`; with the 12 px outer margin the left mini-card LED sits at absolute `x=19`, aligned with the hero LED. The numeric value uses D-DIN 40 px at local `y=8`. Main mini-card labels use Montserrat 12 at local `y=6`; sub/unit labels also use Montserrat 12 at local `y=23` so the size matches the reference while weight/color provide hierarchy. Hero side label and hero unit also use Montserrat 12, matching the mini-card label/sub-label size.
- `assets/lvgl-spike/render-nut-ledcards-interface-via-doomtrain.sh` — fallback renderer that builds the MCP-compatible combined source, renders six states on DOOMTRAIN, and copies the PNGs back.
- `assets/lvgl-spike/results/nut-ledcards-interface-mcp-6state-contact-sheet.png` — current six-state render sheet: nominal, on-battery, low-battery, stale, high-load, and input-low.
- `assets/lvgl-spike/results/nut-ledcards-interface-mcp-6state-vs-reference.png` — current six-state render sheet compared against the Ledcards Interface sample.

Interaction note: the fullscreen Ledcards Interface page intentionally has no visible sleep button. In the `m5stack-cores3-ledcards-interface` firmware environment, a hidden 3 second long press anywhere on the screen calls the same display sleep path as the old HOME control (`psDisplaySetBrightness(0)`). Sleep wake is intentionally gated: after entering sleep, firmware requires at least a 1 second cooldown and a touch release before the next tap can wake the display, so the same held finger cannot immediately turn it back on.

The live-test threshold used: no clipping in the rendered states, runtime label fits, mini-card values remain readable, abnormal states are visually stronger than nominal, and remaining defects are physical-panel polish only (Wi-Fi micro-icon, battery glyph, gamma/color tuning). The original static physical-panel test firmware belonged in a dedicated PlatformIO environment (`m5stack-cores3-ledcards-interface`) rather than a global default build flag; keep the default `m5stack-cores3` environment for normal five-tab firmware.

Regenerate with:

```bash
cd /home/martino/projects/m5stack-power-sentinel
assets/lvgl-spike/render-nut-ledcards-interface-via-doomtrain.sh
```

The selected state coverage is sufficient for an initial live firmware test:

- nominal battery hero;
- on-battery runtime hero;
- low-battery hero;
- stale/unavailable hero.

Latest alignment correction: mini-card numeric values align visually with the vertical accent LED/bar, using an invisible inner-frame rule so no element hugs an edge. Mini-card units are raised from the bottom edge until their lower margin feels comparable to the top-label margin; numeric values are centered against the LED pill, including short glyphs like stale `--` whose bounding box makes misalignment obvious. Mini-card labels use the same normal Montserrat treatment as the hero side label; the previous fake-bold duplicate-label approach was rejected as too chunky. Short hero side labels such as low-battery `Battery/%` and stale `NUT/stale` are pulled closer to their compact hero values, while long runtime labels keep more distance.

Non-blocking polish after live testing: replace the drawn Wi-Fi approximation with a real tiny icon glyph, tune the local/module battery glyph, and adjust palette values on the physical CoreS3 panel if the TFT gamma differs from the MCP render.

## Open design choice

There are two coherent directions:

1. Ledcards Interface-pure fullscreen
   - no left sidebar;
   - dots carry navigation context;
   - closest visual match to the reference;
   - preferred direction for the NUT/UPS adaptive overview after the mock comparison.

2. Power Sentinel integrated
   - keep the existing left icon sidebar;
   - use the `1 + 4` adaptive overview inside the NUT/UPS tab content area;
   - less exact visual match, but more consistent with the current multi-tab appliance model.

The integrated/sidebar option was mocked and works mechanically, but it weakens the ledcards-interface reference feel and should not be the default for this specific adaptive overview pattern.

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

For NUT/UPS, candidate priority is split by grid state and must carry the complete label/range/color contract into the hero.

GRID ONLINE:

| Priority | Condition/range | Hero metric | State label | Color |
| --- | --- | --- | --- | --- |
| 1 | UPS/source unavailable | NUT | `UNAVAILABLE` | purple `0x9b5cff` |
| 1 | Telemetry stale/offline | NUT | `STALE` | gray `0x6c7470` |
| 2 | Battery `<20%` | Battery | `CHARGING` | orange `0xff8a2a` |
| 3 | Load `>=90%` | Load | `OVERLOAD` | red `0xff4e3e` |
| 3 | Load `70-89%` | Load | `HIGH LOAD` | orange `0xff8a2a` |
| 4 | Input `190-209 V` | Input | `MARGINAL INPUT` | yellow `0xfcca3d` |
| 4 | Input `>0` and `<190 V` | Input | `INPUT LOW` | orange `0xff8a2a` |
| 5 | Battery `20-49%` | Battery | `CHARGING` | orange `0xff8a2a` |
| 5 | Battery `50-89%` | Battery | `ALMOST FULL` | yellow `0xfcca3d` |
| 5 | Battery `90-100%` | Battery | `FULL` | green `0x14dc78` |

GRID OFFLINE:

| Priority | Condition/range | Hero metric | State label | Color |
| --- | --- | --- | --- | --- |
| 1 | UPS/source unavailable | NUT | `UNAVAILABLE` | purple `0x9b5cff` |
| 1 | Telemetry stale/offline | NUT | `STALE` | gray `0x6c7470` |
| 2 | Runtime `<2:00` | Runtime | `CRITICAL RUNTIME` | red `0xff4e3e` |
| 3 | Battery `<10%` | Battery | `CRITICAL BATTERY` | red `0xff4e3e` |
| 3 | Battery `10-19%` or low-battery flag | Battery | `LOW BATTERY` | orange `0xff8a2a` |
| 4 | Runtime `2:00-4:59` | Runtime | `SHORT RUNTIME` | orange `0xff8a2a` |
| 5 | Runtime `>=5:00` | Runtime | `ON BATTERY` | yellow `0xfcca3d` |
| 6 | Runtime unavailable, battery not low, load `>=90%` | Load | `OVERLOAD` | red `0xff4e3e` |
| 6 | Runtime unavailable, battery not low, load `70-89%` | Load | `HIGH LOAD` | orange `0xff8a2a` |

Hero state label rule: the large label below the hero value should be the literal state associated with the color/severity and grid state, not a generic summary. `NOMINAL`, `GOOD`, and generic `UNKNOWN` are not Ledcards Interface hero labels. Use gray `STALE` when a source existed but is old/offline; use purple `UNAVAILABLE` when the UPS/source is not available. Runtime thresholds are 5 min / 2 min: online `RESERVE` >=5m, `LOW RESERVE` 2-4:59, `CRITICAL RESERVE` <2m; offline `ON BATTERY` >=5m, `SHORT RUNTIME` 2-4:59, `CRITICAL RUNTIME` <2m. Battery thresholds are split by grid state: online ignores the NUT `LB` flag for display buckets because `OL CHRG LB` can persist during recharge; online `<50%` is orange `CHARGING`, `50-89%` yellow `ALMOST FULL`, `90-100%` green `FULL`; offline `<10%` red `CRITICAL BATTERY`, `10-19%` or `LB` orange `LOW BATTERY`, `>=20%` yellow `ON BATTERY`.

Dynamic mini-card rule: do not duplicate the hero metric in the 2x2 supporting grid. The overview exposes five distinct facts as a physical bidirectional ring. Forward path is `HERO -> top-right -> bottom-right -> bottom-left -> top-left -> HERO`; reverse path is `HERO -> top-left -> bottom-left -> bottom-right -> top-right -> HERO`. The default ring starts as `Battery`, `Runtime`, `Load`, `Input`, `NUT client`. Accepted hero changes rotate the whole ring along the shortest path until the selected/candidate metric reaches `HERO`. Mini-cards are then the next four ring slots. Example: when low battery keeps `Battery` in the hero, `Runtime` is top-right, `Load` bottom-right, `Input` bottom-left, and `NUT client` top-left.

Label wording rule: use `Runtime` for time-to-empty everywhere now that mini-cards hide seconds and show rounded minutes; keep hero and mini-card labels consistent.

Hero swap rule: severity still picks the candidate hero, but accepted hero changes must pass a short cooldown of a few seconds to avoid flicker. When a swap is accepted, rotate the five-slot ring along the shortest physical direction, preserving circular order, until that metric reaches `HERO`. If the candidate changes during cooldown, keep the current hero/order stable until a swap is accepted.

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

The fullscreen Ledcards Interface build now uses live data rather than `PS_NUT_HOME_STATE` fixture constants. `main.cpp` normalizes the existing `SummaryState` into a narrow `LedcardsInterfaceNutView`; `ledcards-interface-page.cpp` owns only display logic. Current physical geometry: hero is a large invisible card region `12,25,296,91`; hero accent inside it is `7,8,7,76` (absolute `19,33,7,76`); hero value is local `31,8` (absolute `43,33`); hero state is local `33,71,142` (absolute `45,96,142`); chart icon hitbox is local `251,31,34,34` (absolute `263,56,34,34`). Mini rows are `124` and `182`, mini-card size is `142x46`, left/right margins `12`, inter-card gaps `12`, bottom margin `12`, mini accent height `28`.

Runtime behavior:

1. Hero candidate priority follows the approved GRID ONLINE / GRID OFFLINE lists: stale/unavailable first; online then low battery -> high load -> marginal input -> normal Battery; offline then critical runtime -> low battery -> short runtime -> on battery -> high load.
2. The active hero is held behind `kHeroCooldownMs` so noisy telemetry does not flicker the whole page.
3. Accepted hero swaps use a physical bidirectional five-slot loop. The forward path is `HERO -> top-right -> bottom-right -> bottom-left -> top-left -> HERO`; the reverse path is `HERO -> top-left -> bottom-left -> bottom-right -> top-right -> HERO`. The implementation chooses the shorter direction to bring the selected metric to `HERO`. The four mini-cards are the next four ring slots, so the hero never appears twice and circular order is preserved.
4. The live page animates accepted automatic swaps, touch overrides, and the priority resume after a 60-second manual override using the same chain transition. The hero is a large invisible card; metrics crossing it render as a full-size hero card sliding horizontally left/right. Compact mini-card ghosts stay out of the hero region and move only inside the right-column, bottom-row, and left-column mini-card lanes. A full-screen overlay masks the old static hero lane and blocks touches until the final redraw.
5. Animation timing is weighted by visual path length rather than fixed logical steps. Hero crossings are weighted heavier for large-card inertia, all ghosts share one global duration based on the longest weighted path and clamped around `360..720 ms`, segment progress is distance-based, and a subtle `18 ms` per-link mini-stagger lets the selected/priority target lead the chain. The animation uses gentle ease-in-out timing.
6. Tapping a mini-card temporarily rotates that metric to the hero for 60 seconds, overriding the default priority. After expiry the normal severity/priority hero selection resumes and, when it differs from the current hero, runs the same ring animation path again after cooldown.
7. Unknown/stale values render as `--` with gray accent/fill. No live firmware path uses the previous compile-time demo constants.
8. `Runtime` is context-sensitive: hero uses full `mm:ss`; mini-cards use rounded whole minutes with generic unit `m` to avoid physical TFT clipping in the 142x46 card geometry and to fit single-digit values cleanly.
9. `NUT` client count is cyan for one or more clients, orange for zero expected clients, and gray for unknown/stale. It remains read-only telemetry only.

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
   - less exact visual match, but more consistent with a future modular appliance model.

The integrated/sidebar option was mocked and works mechanically, but it weakens the ledcards-interface reference feel and should not be the default for this specific adaptive overview pattern.

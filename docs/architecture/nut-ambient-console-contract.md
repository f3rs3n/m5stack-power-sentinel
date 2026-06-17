# NUT Ambient Console contract

The NUT Ambient Console is the first stable Power Sentinel Ambient Console adapter shape. It is intentionally narrow: it turns the current NUT summary into a glanceable CoreS3 page model and leaves broader product surfaces to later module slices.

## Scope

In scope:

- NUT Monitor page model for the CoreS3 Ambient Console.
- Five interpreted metric cards: battery, runtime, load, input, and NUT clients/status.
- Canonical telemetry states: `live`, `partial`, `stale`, and `unavailable`.
- Canonical module conditions: `healthy`, `warning`, `critical`, `stale`, and `unavailable`.
- Metric card display fields: `metricId`, `label`, `value`, `compactValue`, `unit`, `compactUnit`, `stateText`, `stateClass`, and `visualClass`.
- Hero priority and accepted touch-override policy through the page model seam.
- Compatibility with the current Ledcards renderer and CoreS3 + LLM Module StackFlow path.

Out of scope for this contract:

- Proxmox page rendering.
- Overview page rendering.
- Telemetry History.
- Controls or remediation flows.
- Replacing NUT/`upsmon` as the shutdown owner.

## Backend summary contract

The public summary endpoint remains:

- `GET /api/v1/summary`
- `GET /api/v1/summary?stackflow_safe=1`

The schema remains `power-sentinel.summary.v1` and the profile remains `nut-monitor-clean-baseline`.

The summary contract keeps these compatibility guarantees for current clients:

- `modules.nut` is the canonical NUT module payload.
- Top-level `ups` remains a compatibility alias for `modules.nut.ups`.
- Top-level `nut` remains a compatibility alias for `modules.nut.nut`.
- `condition` is the canonical interpreted state.
- `severity` remains a legacy alias for current firmware and older clients.
- `stackflow_safe` is echoed when the StackFlow-safe path is requested.
- `module.lan_connected`, `module.lan_ip`, and `module.time_hhmm` remain present for the CoreS3 top row.

The StackFlow unit wraps the same summary payload with object `power-sentinel.summary.v1`; it does not define a separate data contract.

## Page model contract

`NutAmbientPageModel` owns the interpreted state used by the renderer:

- `condition`: user-facing module condition.
- `telemetryState`: source freshness/completeness state.
- `shutdownRelevant`: true only for the safety-relevant NUT shutdown condition.
- `hasMissingMetrics`: true when the source is live but individual metrics are missing.
- `heroMetric`: model-derived most important metric.
- `cards[]`: interpreted metric cards ready for rendering.

The renderer may map `visualClass` to LVGL colors and layout objects, but it must not re-own NUT metric classification, stale/unavailable/missing classification, or hero priority policy.

## Hero and touch policy

The model seam provides:

- `chooseNutAmbientHeroMetric(view)` for the most important raw NUT metric.
- `acceptNutAmbientHeroMetric(model, input)` for cooldown-aware accepted hero selection.
- `makeNutAmbientTouchHeroOverride(metric, nowMillis, durationMs)` for touch override construction.

Current behavior preserved:

- Stale or unavailable telemetry promotes the NUT card.
- Critical runtime on battery promotes runtime.
- Low battery promotes battery when runtime is not more urgent.
- High load promotes load.
- Marginal input promotes input.
- Healthy line power defaults to battery.
- A live touch override wins until its timeout expires.
- After touch expiry, automatic hero changes still respect the existing cooldown.

## Visual compatibility

The migrated contract keeps visual classification explicit:

- `green`, `yellow`, `orange`, `red`, `blue`, `purple`, and `gray` are renderer color classes, not backend API conditions.
- Healthy observations should not use warning colors such as yellow or orange.
- A NUT battery below the full threshold is a warning condition, not a healthy observation with yellow/orange decoration, because it indicates either recent line-power loss or a UPS/battery problem.
- The NUT battery full threshold is 90% for condition and visual treatment.
- Battery warning visual intensity is presentation only: 50-89% uses yellow, 20-49% uses orange, and below 20% on line power uses orange with low-battery wording unless the UPS is also on battery and shutdown-relevant.
- The NUT module condition should reflect the worst relevant NUT card condition for observability, safety, power readiness, service readiness, electrical input, and load pressure. Battery below 90%, high load, marginal input, missing clients, or degraded NUT services can make the module warning even when line power is currently online.
- Runtime reserve on line power should remain a card-level observation unless it is critical or the UPS is on battery, because UPS runtime estimates can be conservative and should not create permanent module warnings while the battery is otherwise ready.
- Battery warning wording should prefer direct NUT status when available: use `CHARGING` when the UPS reports charging, and use `NOT READY` when line power is online but the battery is below the full threshold without an explicit charging status.
- Stale telemetry remains visually gray.
- Unavailable telemetry remains visually purple.
- Missing individual metric values render as `--` and do not synthesize healthy values.

## Display standby and brightness policy

The CoreS3 page remains visible by default and treats `DIM` as the automatic sleep state:

- After the first valid summary payload, 5 minutes (`POWER_SENTINEL_DISPLAY_STANDBY_MS=300000UL`) without touch or meaningful state change fades `AWAKE` to `DIM`.
- The hardware brightness fade is about 900 ms in the shipped config example.
- Automatic standby stops at `DIM`; it does not continue to `OFF` while valid payloads are present.
- `OFF` is reserved for deliberate 3-second long-press snooze or for a prolonged missing-payload condition after 15 minutes (`POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS=900000UL`).
- Meaningful state changes, not noisy telemetry values, reset the standby timer: condition/severity, UPS availability/staleness, on-battery, low-battery, charging, normalized UPS status, and NUT client count.

Current brightness defaults:

- Static fallback: `AWAKE=120`, `DIM=36`.
- Adaptive ALS raw breakpoints: `0/12/45/70/95/135`.
- Adaptive `DIM` Backlight Level curve: `20/20/21/21/22/22`.
- Adaptive `AWAKE` Backlight Level curve: `21/22/23/24/25/25`.
- The firmware encodes each selected Backlight Level as a center-band nominal CoreS3 brightness value before calling M5GFX (`20->15`, `21->46`, `22->78`, `23->110`, `24->142`, `25->174`).
- Adaptive slew stays at `step=1`, but the step is now a physical Backlight Level step. Intervals remain `high=96 ms`, `mid_high=136 ms`, `mid_low=160 ms`, and `low=192 ms`. This keeps the existing reaction delay/debounce while avoiding fake 0-255 transitions that collapse to the same physical output.

The brightness policy should be hardware-aware: choose the target `Backlight Level` first, then encode it as a nominal CoreS3 brightness value for the hardware driver. Nominal 0-255 brightness values are not a behavioral contract because the CoreS3 backlight maps them into coarse physical levels. Adaptive transitions should smooth target changes between physical levels rather than animate through nominal values that collapse to the same Backlight Level.

## Validation

Regression coverage lives in:

- `backend/tests/test_power_sentinel_api.py` for public summary, StackFlow-safe fields, aliases, and top-row status fields.
- `backend/tests/test_power_sentinel_stackflow_unit.py` for StackFlow response wrapping.
- `backend/tests/test_nut_ambient_page_model.py` for page model card interpretation, missing telemetry states, hero priority, and touch override policy.
- `tools/check_core_s3_ui.py` for static firmware/UI contract guardrails.

Full local validation:

```bash
python3 tools/run_tests.py
cd firmware/core-s3-display
pio run -e m5stack-cores3-ledcards-interface
```

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

The migrated contract preserves the current Ledcards appearance by keeping visual classification explicit:

- `green`, `yellow`, `orange`, `red`, `blue`, `purple`, and `gray` are renderer color classes, not backend API conditions.
- Stale telemetry remains visually gray.
- Unavailable telemetry remains visually purple.
- Missing individual metric values render as `--` and do not synthesize healthy values.

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

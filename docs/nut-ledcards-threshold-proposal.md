# NUT Ledcards threshold proposal

This is the agreed proposal from the NUT Ledcards visual-class grilling session. It separates user-facing condition from visual depth: visual classes can use blue/green/yellow for healthy or informational depth, but warning/critical conditions still mean attention.

## Global rules

| Case | stateClass / condition | visualClass | Notes |
|---|---|---|---|
| Telemetry unavailable | `unavailable` | `purple` | Source cannot produce useful data. |
| Telemetry stale | `stale` | `gray` | Previously useful data is not fresh enough to trust. |
| Missing individual live metric | `unavailable` for that card | `purple` | Card value renders `--`; do not synthesize fake healthy values. |

## Battery

| Situation | stateClass | visualClass | stateText |
|---|---|---|---|
| Line power, battery >= 90% | `healthy` | `green` | `FULL` |
| Line power, 70-89% | `warning` | `blue` | `CHARGING` or `NOT READY` |
| Line power, 40-69% | `warning` | `yellow` | `CHARGING` or `NOT READY` |
| Line power, 20-39% | `warning` | `orange` | `CHARGING` or `NOT READY` |
| Line power, <20% | `warning` | `orange` | `LOW BATTERY` |
| On battery, >=90% | `healthy` | `green` | `FULL` |
| On battery, 70-89% | `warning` | `blue` | `DISCHARGING` |
| On battery, 50-69% | `warning` | `yellow` | `DISCHARGING` |
| On battery, 20-49% | `warning` | `orange` | `ON BATTERY` |
| On battery, 10-19%, not lowBattery | `warning` | `orange` | `LOW BATTERY` |
| On battery, 10-19%, lowBattery | `critical` | `orange` | `LOW BATTERY` |
| On battery, <10% | `critical` | `red` | `CRITICAL BATTERY` |

Rationale: keep >=90 as the fully-ready battery state even when the UPS is currently on battery; in that case any module-level warning comes from missing line power, not from battery reserve. Use blue for early discharge at 70-89% so the visual shows the direction of travel without implying low reserve; reserve orange/red for genuinely low reserve.

## Runtime

| Situation | stateClass | visualClass | stateText |
|---|---|---|---|
| Line power, >=240s | `healthy` | `blue` | `RESERVE` |
| Line power, 120-239s | `healthy` | `yellow` | `RESERVE` |
| Line power, 60-119s | `warning` | `orange` | `LOW RESERVE` |
| Line power, <60s | `critical` | `red` | `CRITICAL RESERVE` |
| On battery, >=240s | `warning` | `blue` | `ON BATTERY` |
| On battery, 120-239s | `warning` | `yellow` | `ON BATTERY` |
| On battery, 60-119s | `warning` | `orange` | `SHORT RUNTIME` |
| On battery, <60s | `critical` | `red` | `CRITICAL RUNTIME` |

Rationale: Power Sentinel targets homelab UPS hardware; 3-4 minutes can be normal runtime under real load, so critical starts below 60s rather than 120s. Healthy runtime reserve stays blue because it is neutral/informational reserve, not a separate health-positive signal.

## Load

| Load | stateClass | visualClass | stateText |
|---|---|---|---|
| 0-9% | `healthy` | `blue` | `LOW` |
| 10-39% | `healthy` | `green` | `NORMAL` |
| 40-69% | `healthy` | `yellow` | `NORMAL` |
| 70-89% | `warning` | `orange` | `HIGH LOAD` |
| >=90% | `critical` | `red` | `OVERLOAD` |

Rationale: keep current functional thresholds at 70/90, add yellow as visual depth for medium load without making it warning.

## Input voltage

| Input voltage | stateClass | visualClass | stateText |
|---|---|---|---|
| <=0V and on battery | `critical` | `red` | `GRID OFFLINE` |
| <=0V and not on battery | `unavailable` | `gray` | `UNAVAILABLE` |
| 0-189V | `warning` | `orange` | `INPUT LOW` |
| 190-209V | `warning` | `yellow` | `MARGINAL INPUT` |
| 210-219V | `healthy` | `blue` | `GRID ONLINE` |
| >=220V | `healthy` | `green` | `GRID ONLINE` |

Rationale: preserve warning below 210V, add blue for borderline-normal online input, and keep red for actual grid-offline-on-battery.

## NUT clients

| Client count | stateClass | visualClass | stateText |
|---|---|---|---|
| <0 | `unavailable` | `purple` | `UNAVAILABLE` |
| 0 | `warning` | `orange` | `NO CLIENTS` |
| 1 | `healthy` | `blue` | `CLIENTS` |
| >=2 | `healthy` | `green` | `CLIENTS` |

Rationale: distinguish minimum coverage from fuller client coverage without inventing a new condition.

## Backend/module condition impact

Proposed backend/module condition changes:

- Runtime critical threshold changes from `<120s` to `<60s`.
- Runtime warning covers the final non-critical reserve window (`60-119s`) and any on-battery runtime below the normal reserve band; critical starts below `60s`.
- Battery `<90%`, load `>=70%`, input `<210V`, zero clients, degraded services, stale, unavailable, and shutdown relevance keep their current module-condition roles unless explicitly revised later.

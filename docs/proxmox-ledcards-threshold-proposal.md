# Proxmox Ledcards threshold proposal

This is the working proposal from the Proxmox Ledcards visual-class grilling session. It separates user-facing condition from visual depth: visual classes can use blue/green/yellow for healthy or informational depth, while orange/red remain attention semantics. Stale and unavailable keep the shared treatment.

Label style follows the NUT page: state text should be readable status copy, not implementation vocabulary. Avoid repeating the card label in the state text when the hero/card already says `CPU`, `RAM`, `Guests`, `Storage`, or `Network`; prefer phrases like `LIGHT LOAD`, `ALMOST FULL`, or `ALL RUNNING` over acronyms such as `CPU WARN` or `STOR CRIT`.

## Global rules

| Case | stateClass / condition | visualClass | Notes |
|---|---|---|---|
| Module or card unavailable | `unavailable` | `purple` | Source/card cannot produce useful data. |
| Stale telemetry | `stale` | `gray` | Previously useful data is not fresh enough to trust. |
| Missing individual live metric | `unavailable` for that card | `purple` | Card value renders `--`; do not synthesize fake healthy values. |

## CPU

| CPU | stateClass | visualClass | stateText |
|---|---|---|---|
| 0-9% | `healthy` | `blue` | `IDLE` |
| 10-59% | `healthy` | `green` | `LIGHT LOAD` |
| 60-79% | `healthy` | `yellow` | `MEDIUM LOAD` |
| 80-94%, sustained for 2 samples | `warning` | `orange` | `HIGH LOAD` |
| >=95%, sustained for 2 samples | `critical` | `red` | `OVERLOAD` |

Rationale: sustained high CPU should surface earlier than the current 85/98 split on a homelab node, but medium-high CPU remains visual depth rather than attention.

## RAM

| RAM | stateClass | visualClass | stateText |
|---|---|---|---|
| 0-24% | `healthy` | `blue` | `MOSTLY FREE` |
| 25-69% | `healthy` | `green` | `NORMAL USE` |
| 70-84% | `healthy` | `yellow` | `HIGH USE` |
| 85-94%, sustained for 2 samples | `warning` | `orange` | `LOW HEADROOM` |
| >=95%, sustained for 2 samples | `critical` | `red` | `NO HEADROOM` |

Rationale: blue means a mostly idle/empty node rather than medium usage; Proxmox memory use can be cache-heavy, so 70-84% is visual depth but not attention. Sustained 85%+ becomes warning because it reduces headroom for guests and reclaim.

## Guests

| Guests | stateClass | visualClass | stateText |
|---|---|---|---|
| 0/0 | `healthy` | `blue` | `NO GUESTS` |
| running == total and total > 0 | `healthy` | `green` | `ALL RUNNING` |
| 0 < running < total | `warning` | `orange` | `SOME STOPPED` |
| running == 0 and total > 0 | `critical` | `red` | `ALL STOPPED` |

Rationale: Guests is a discrete inventory summary rather than a continuous utilization gauge. Blue remains the neutral healthy empty environment; any configured/visible guest stopped is attention-worthy.

## Storage

| Storage used | stateClass | visualClass | stateText |
|---|---|---|---|
| 0-39% | `healthy` | `blue` | `LOTS FREE` |
| 40-69% | `healthy` | `green` | `NORMAL USE` |
| 70-84% | `healthy` | `yellow` | `FILLING UP` |
| 85-94% | `warning` | `orange` | `ALMOST FULL` |
| >=95% | `critical` | `red` | `CRITICALLY FULL` |

Rationale: storage pressure is slow-moving, so the existing 85/95 attention thresholds remain appropriate. Blue and yellow add capacity-depth cues without turning normal fill levels into attention.

## Network

| Uplink saturation | stateClass | visualClass | stateText |
|---|---|---|---|
| 0-4% | `healthy` | `blue` | `IDLE` |
| 5-39% | `healthy` | `green` | `LIGHT TRAFFIC` |
| 40-69% | `healthy` | `yellow` | `BUSY` |
| 70-89%, sustained for 2 samples | `warning` | `orange` | `SATURATED` |
| >=90%, sustained for 2 samples | `critical` | `red` | `OVERLOADED` |

Rationale: sustained uplink saturation above 70% is already useful attention for a homelab node. Medium saturation is visual depth only; missing uplink or speed remains unavailable rather than fake zero traffic.

## Hero priority

Severity rank wins first: `critical` > `warning` > `stale`/`unavailable` > `healthy`. Within the same non-healthy severity tier, keep the current tie-break order:

1. Storage
2. CPU
3. RAM
4. Network
5. Guests

Rationale: storage pressure is slow-moving and harder to recover when it reaches attention thresholds; CPU/RAM are host resource pressure; network saturation is often transient; Guests already carries an explicit running/total ratio and should not outrank host risks at the same severity.

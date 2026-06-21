# Proxmox Ledcards threshold proposal

This is the working proposal from the Proxmox Ledcards visual-class grilling session. It separates user-facing condition from visual depth: visual classes can use blue/green/yellow for healthy or informational depth, while orange/red remain attention semantics. Stale and unavailable keep the shared treatment.

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
| 10-59% | `healthy` | `green` | `CPU OK` |
| 60-79% | `healthy` | `yellow` | `CPU BUSY` |
| 80-94%, sustained for 2 samples | `warning` | `orange` | `CPU WARN` |
| >=95%, sustained for 2 samples | `critical` | `red` | `CPU CRIT` |

Rationale: sustained high CPU should surface earlier than the current 85/98 split on a homelab node, but medium-high CPU remains visual depth rather than attention.

## RAM

| RAM | stateClass | visualClass | stateText |
|---|---|---|---|
| 0-24% | `healthy` | `blue` | `RAM LOW` |
| 25-69% | `healthy` | `green` | `RAM OK` |
| 70-84% | `healthy` | `yellow` | `RAM HIGH` |
| 85-94%, sustained for 2 samples | `warning` | `orange` | `RAM WARN` |
| >=95%, sustained for 2 samples | `critical` | `red` | `RAM CRIT` |

Rationale: blue means a mostly idle/empty node rather than medium usage; Proxmox memory use can be cache-heavy, so 70-84% is visual depth but not attention. Sustained 85%+ becomes warning because it reduces headroom for guests and reclaim.

## Guests

| Guests | stateClass | visualClass | stateText |
|---|---|---|---|
| 0/0 | `healthy` | `blue` | `GUEST EMPTY` |
| running == total and total > 0 | `healthy` | `green` | `GUEST OK` |
| 0 < running < total | `warning` | `orange` | `GUEST WARN` |
| running == 0 and total > 0 | `critical` | `red` | `GUEST CRIT` |

Rationale: Guests is a discrete inventory summary rather than a continuous utilization gauge. Blue remains the neutral healthy empty environment; any configured/visible guest stopped is attention-worthy.

## Storage

| Storage used | stateClass | visualClass | stateText |
|---|---|---|---|
| 0-39% | `healthy` | `blue` | `STOR LOW` |
| 40-69% | `healthy` | `green` | `STOR OK` |
| 70-84% | `healthy` | `yellow` | `STOR HIGH` |
| 85-94% | `warning` | `orange` | `STOR WARN` |
| >=95% | `critical` | `red` | `STOR CRIT` |

Rationale: storage pressure is slow-moving, so the existing 85/95 attention thresholds remain appropriate. Blue and yellow add capacity-depth cues without turning normal fill levels into attention.

## Network

| Uplink saturation | stateClass | visualClass | stateText |
|---|---|---|---|
| 0-4% | `healthy` | `blue` | `NET IDLE` |
| 5-39% | `healthy` | `green` | `NET OK` |
| 40-69% | `healthy` | `yellow` | `NET BUSY` |
| 70-89%, sustained for 2 samples | `warning` | `orange` | `NET WARN` |
| >=90%, sustained for 2 samples | `critical` | `red` | `NET CRIT` |

Rationale: sustained uplink saturation above 70% is already useful attention for a homelab node. Medium saturation is visual depth only; missing uplink or speed remains unavailable rather than fake zero traffic.

## Hero priority

Severity rank wins first: `critical` > `warning` > `stale`/`unavailable` > `healthy`. Within the same non-healthy severity tier, keep the current tie-break order:

1. Storage
2. CPU
3. RAM
4. Network
5. Guests

Rationale: storage pressure is slow-moving and harder to recover when it reaches attention thresholds; CPU/RAM are host resource pressure; network saturation is often transient; Guests already carries an explicit running/total ratio and should not outrank host risks at the same severity.

# Proxmox Module v1

## Intent

Reintroduce Proxmox as an independently scoped Power Sentinel module. The v1 module provides API-only, read-only observability for one Proxmox Environment, focused on typical single-node homelab installations, host-level health at a glance, and contextual handoff data.

## Boundaries

- Use the Proxmox API only.
- Use read-only credentials.
- Do not use SSH, remote shell commands, or local node commands.
- Do not control guests, storage, networking, backup jobs, or cluster operations.
- Do not recreate the Proxmox console.
- Do not add Telemetry History in this slice.

## Preparatory Refactor

Introduce module-level `condition` while keeping `severity` as a compatibility alias.

- `healthy` maps to legacy `ok`.
- `warning` maps to legacy `warn`.
- `critical` maps to legacy `critical`.
- `stale` maps to legacy `warn`.
- `unavailable` maps to legacy `warn` when the module is enabled.

Apply this to the existing NUT module before or with the first Proxmox slice.

## Configuration

Store Proxmox config under the existing Power Sentinel config file:

```json
{
  "proxmox": {
    "enabled": true,
    "api_url": "https://pve.example:8006",
    "token_id": "power-sentinel@pve!monitor",
    "token_secret": "CHANGE_ME",
    "node_name": "pve",
    "network_uplink": "eno1",
    "network_uplink_speed_mbps": 1000,
    "thresholds": {
      "storage_warning_percent": 85,
      "storage_critical_percent": 95,
      "cpu_warning_percent": 85,
      "cpu_critical_percent": 98,
      "memory_warning_percent": 90,
      "memory_critical_percent": 98,
      "network_warning_percent": 80,
      "network_critical_percent": 95,
      "resource_pressure_window_seconds": 300,
      "resource_pressure_consecutive_polls": 5
    }
  }
}
```

## Freshness

- Poll Proxmox at most every 30 seconds.
- Mark data `stale` when the last successful observation is older than 120 seconds.
- Use a 3-5 second API request timeout.
- If there has never been a successful observation, report `unavailable`, not `stale`.
- If a poll fails while the last successful observation is still fresh, keep serving the cached condition without changing the user-facing condition for a single transient failure.
- Use an in-memory cache for v1; no database or history storage.

## Payload Shape

`modules.proxmox` should follow this general shape:

```json
{
  "enabled": true,
  "implemented": true,
  "page": "PROXMOX",
  "condition": "warning",
  "severity": "warn",
  "status": "active",
  "observed_at": "2026-06-13T10:15:00Z",
  "age_seconds": 12,
  "environment": {
    "name": "pve",
    "node_count": 1,
    "guest_count": 12,
    "running_guest_count": 11
  },
  "signals": [
    {
      "kind": "storage_warning",
      "condition": "warning",
      "summary": "local-lvm 88%",
      "context": {
        "node": "pve",
        "storage": "local-lvm",
        "usage_percent": 88
      }
    }
  ],
  "nodes": [
    {
      "name": "pve",
      "condition": "healthy",
      "status": "online",
      "cpu_percent": 23,
      "memory_percent": 61
    }
  ]
}
```

Rules:

- `condition` is the source of truth.
- `severity` remains for compatibility.
- `signals[]` contains non-healthy conditions or attention-worthy context.
- `condition=healthy` implies `signals=[]`.
- A non-healthy condition should include at least one signal when enough context is available.
- `nodes[]` and guest summary context provide limited context, not a full inventory.

## CoreS3 Page Direction

The `PROXMOX` page should answer "do I need to care?" before presenting metrics.

- Hero: Proxmox Environment condition plus a short label for the most important signal.
- Ambient cards: CPU, RAM, Guests, Storage, and Network.
- Each ambient card has one responsibility and one compact value; any card can be promoted into the hero position by condition priority, default policy, or touch focus.
- CPU: single-node host CPU usage percent, such as `61%`.
- RAM: single-node host memory usage percent, such as `74%`.
- Guests: running guests over total visible QEMU VM and LXC container guests, such as `7/8`.
- Storage: worst observed storage usage percent, such as `72%`.
- Network: maximum percent saturation among physical uplinks, such as `42%`; derive link speed from the Proxmox API when available, otherwise require explicit config.

When `signals[]` is non-empty, the page should privilege the worst actionable signal over neutral inventory numbers.

Hero promotion priority:

- Critical card conditions outrank warning or unavailable card conditions.
- Within critical conditions, promote Storage before CPU, RAM, Network, then Guests.
- Within warning or unavailable card conditions, promote Storage before CPU, RAM, Network, then Guests.
- When all cards are healthy, the default hero is CPU.

Touch interaction in v1 is for focus only, such as cycling or promoting a tile/signal to the hero area. The page must not trigger Proxmox actions.

## Signal Kinds

Initial `signals[].kind` values:

- `unconfigured`
- `api_unavailable`
- `stale_data`
- `storage_warning`
- `storage_critical`
- `cpu_pressure`
- `memory_pressure`
- `network_pressure`
- `guest_down`

## Condition Aggregation

Observation state is evaluated before observed problems:

- If the module cannot observe Proxmox and has never had a successful observation, condition is `unavailable`.
- If the last successful observation is older than the stale threshold, condition is `stale`.
- If one ambient card is unavailable but the single-node host and the rest of the page can be observed, keep the module available and aggregate that card as at least warning-level attention rather than failing the whole module.
- Otherwise aggregate fresh actionable card/signal conditions with worst condition wins: `critical` beats `warning`, and no signals means `healthy`. Each card applies its own actionability rule before participating in module aggregation, such as sustained thresholds for CPU, RAM, and Network.

## Condition Inputs

- API unreachable or auth failure -> `unavailable`.
- No fresh data after prior success -> `stale`.
- If the v1 single-node Proxmox Environment cannot be observed coherently, the Proxmox module is `unavailable`; node availability is not a separate ambient card in v1.
- If exactly one node is visible, select it automatically; if multiple nodes are visible, require explicit `proxmox.node_name` or report `unavailable` rather than guessing.
- Storage usage >= 85% -> `warning`.
- Storage usage >= 95% -> `critical`.
- Running guests below total visible guests -> `warning`.
- Zero running guests when total guests is greater than zero -> `critical`.
- Sustained node-level CPU above warning threshold -> `warning`.
- Sustained node-level CPU above critical threshold -> `critical`.
- Sustained node-level memory above warning threshold -> `warning`.
- Sustained node-level memory above critical threshold -> `critical`.
- Sustained physical uplink saturation >= 80% -> `warning`.
- Sustained physical uplink saturation >= 95% -> `critical`.

CPU and memory pressure:

- Evaluate CPU and memory pressure as separate ambient cards and separate signal kinds for the observed single-node Proxmox host.
- Cards show the latest observed value; warning and critical conditions require sustained threshold crossings.
- Do not warn on a single spike.
- Default warning or critical behavior is 5 consecutive polls above the relevant threshold at the configured polling interval.
- Guest-level CPU/RAM diagnosis is out of scope for v1.

Network pressure:

- Cards show the latest observed physical uplink saturation value; warning and critical conditions require sustained threshold crossings.
- Use the maximum percent saturation among physical uplinks.
- Auto-select the physical uplink only when the Proxmox network configuration makes it unambiguous; otherwise require explicit `proxmox.network_uplink` rather than guessing.
- Derive link speed from the Proxmox API when available; otherwise require explicit `proxmox.network_uplink_speed_mbps` rather than guessing.
- Use 80% sustained as warning and 95% sustained as critical.
- Do not warn on a single traffic burst.

Guests:
- Count both QEMU virtual machines and LXC containers.
- The Guests card is an inventory summary, not a configured watched-guest allowlist.
- `0/0` is neutral, not warning or critical, and should use the same neutral/blue visual language used by NUT for connected clients.
- Neutral is a visual treatment only: `0/0` remains a healthy condition for aggregation and compatibility aliases.
- Healthy observed CPU, RAM, Storage, and Network cards use green because they communicate health-positive operational state.
- Guests with all visible guests running and total greater than zero uses green because it communicates health-positive workload state.
- Guest start/stop/reboot control is out of scope.

## Test Cases

Preparatory condition migration:

- NUT summary includes `condition` and still includes legacy `severity`.
- NUT healthy maps to `condition=healthy`, `severity=ok`.
- NUT on battery or service warning maps to `condition=warning`, `severity=warn`.
- NUT `OB LB` maps to `condition=critical`, `severity=critical`.
- Enabled but unavailable NUT data maps to a non-healthy condition and preserves compatibility aliases.

Proxmox module availability:

- Disabled Proxmox module does not appear as a problem.
- Enabled Proxmox module with missing minimum config reports `condition=unavailable`.
- API auth failure reports `condition=unavailable` with an `api_unavailable` signal.
- First successful healthy observation reports `condition=healthy` and `signals=[]`.
- No successful observation ever reports `unavailable`, not `stale`.
- Previously successful observation older than the stale threshold reports `condition=stale` with a `stale_data` signal.

Proxmox condition inputs:

- Single-node environment unavailable reports module `condition=unavailable`, not a separate node card signal.
- Storage at or above warning threshold reports `warning` with `storage_warning`.
- Storage at or above critical threshold reports `critical` with `storage_critical`.
- Running guests below total visible guests reports `warning` with `guest_down`.
- Zero running guests when total guests is greater than zero reports `critical` with `guest_down`.
- Sustained node CPU above warning threshold reports `warning` with `cpu_pressure`.
- Sustained node CPU above critical threshold reports `critical` with `cpu_pressure`.
- Sustained node memory above warning threshold reports `warning` with `memory_pressure`.
- Sustained node memory above critical threshold reports `critical` with `memory_pressure`.
- Sustained physical uplink saturation >= 80% reports `warning` with `network_pressure`.
- Sustained physical uplink saturation >= 95% reports `critical` with `network_pressure`.
- Brief CPU/RAM spike does not affect condition.

Payload compatibility:

- `condition` is the source of truth for new module logic.
- `severity` remains present as a legacy alias.
- `condition=healthy` always has `signals=[]`.
- Non-healthy Proxmox conditions include at least one signal when enough context is available.
- `nodes[]` and guest summary context stay limited to handoff context, not full inventory.

## Out of Scope

- Guest start/stop/reboot controls.
- Proxmox backup management or backup-health card.
- Disk health signals, unless later exposed by the Proxmox API as a lightweight actionable signal in a separate slice.
- Storage/network/cluster changes.
- SSH or command execution.
- Console/log streaming.
- Guest-level CPU/RAM diagnosis.
- Historical charts or retention.

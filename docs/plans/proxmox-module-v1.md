# Proxmox Module v1

## Intent

Reintroduce Proxmox as an independently scoped Power Sentinel module. The v1 module provides API-only, read-only observability for one Proxmox Environment, focused on cluster/node condition at a glance and contextual handoff data.

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
  "modules": {
    "nut": true,
    "proxmox": true,
    "ha": false
  },
  "proxmox": {
    "api_url": "https://pve.example:8006",
    "token_id": "power-sentinel@pve!monitor",
    "token_secret": "CHANGE_ME",
    "watched_guests": [101, 120],
    "thresholds": {
      "storage_warning_percent": 85,
      "storage_critical_percent": 95,
      "cpu_warning_percent": 85,
      "memory_warning_percent": 90,
      "resource_pressure_window_seconds": 300,
      "resource_pressure_consecutive_polls": 5,
      "backup_failure_window_hours": 24
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
    "watched_guest_count": 2
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
  ],
  "watched_guests": [
    {
      "vmid": 101,
      "name": "homeassistant",
      "kind": "vm",
      "condition": "healthy",
      "status": "running",
      "node": "pve"
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
- `nodes[]` and `watched_guests[]` provide limited context, not a full inventory.

## CoreS3 Page Direction

The `PROXMOX` page should answer "do I need to care?" before presenting metrics.

- Hero: Proxmox Environment condition plus a short label for the most important signal.
- Tile 1: node summary, such as `1/1 online` or `1 down`.
- Tile 2: Watched Guest summary, such as `2/2 running` or `1 down`.
- Tile 3: storage summary, such as `max 72%` or `local-lvm 96%`.
- Tile 4: the most relevant backup, resource pressure, or disk health summary available.

When `signals[]` is non-empty, the page should privilege the worst actionable signal over neutral inventory numbers.

Touch interaction in v1 is for focus only, such as cycling or promoting a tile/signal to the hero area. The page must not trigger Proxmox actions.

## Signal Kinds

Initial `signals[].kind` values:

- `unconfigured`
- `api_unavailable`
- `stale_data`
- `node_down`
- `node_degraded`
- `storage_warning`
- `storage_critical`
- `backup_failed`
- `watched_guest_down`
- `resource_pressure`
- `disk_health`

## Condition Aggregation

Observation state is evaluated before observed problems:

- If the module cannot observe Proxmox and has never had a successful observation, condition is `unavailable`.
- If the last successful observation is older than the stale threshold, condition is `stale`.
- Otherwise aggregate fresh signals with worst condition wins: `critical` beats `warning`, and no signals means `healthy`.

## Condition Inputs

- API unreachable or auth failure -> `unavailable`.
- No fresh data after prior success -> `stale`.
- Any node offline or degraded -> `critical`.
- Storage usage >= 85% -> `warning`.
- Storage usage >= 95% -> `critical`.
- Recent failed backup -> `warning`.
- Watched Guest down -> `critical`.
- Sustained node-level CPU or RAM pressure -> `warning`.
- Disk health issue -> `warning` or `critical` only when exposed through the Proxmox API with lightweight, actionable signals.

Backup failures:

- Observe recent backup failures across all backup jobs, not only Watched Guests.
- Default failure window is 24 hours, configurable.
- Backup signals should include concrete context such as job, guest VMID/name, timestamp, storage/target, or concise error text when available.

Resource pressure:

- Evaluate CPU/RAM pressure at node level only.
- Do not warn on a single spike.
- Default warning behavior is 5 consecutive polls above threshold at the configured polling interval.
- Guest-level CPU/RAM diagnosis is out of scope for v1.

## Watched Guests

- Watched Guests are configured manually by Proxmox VMID.
- A Watched Guest is expected to be running.
- If a Watched Guest is down, the Proxmox condition is `critical`.
- Unwatched guests may appear as context later, but should not influence condition by themselves.

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

- Node offline reports `condition=critical` with `node_down`.
- Node degraded reports `condition=critical` with `node_degraded`.
- Storage at or above warning threshold reports `warning` with `storage_warning`.
- Storage at or above critical threshold reports `critical` with `storage_critical`.
- Recent failed backup reports `warning` with `backup_failed`.
- Backup failures outside the configured window do not affect condition.
- Watched Guest down reports `critical` with `watched_guest_down`.
- Unwatched Guest down does not affect condition.
- Sustained node CPU or RAM pressure reports `warning` with `resource_pressure`.
- Brief CPU/RAM spike does not affect condition.
- Disk health issue reports warning or critical only when surfaced by the Proxmox API as a lightweight actionable signal.

Payload compatibility:

- `condition` is the source of truth for new module logic.
- `severity` remains present as a legacy alias.
- `condition=healthy` always has `signals=[]`.
- Non-healthy Proxmox conditions include at least one signal when enough context is available.
- `nodes[]` and `watched_guests[]` stay limited to summary context, not full inventory.

## Out of Scope

- Guest start/stop/reboot controls.
- Proxmox backup management.
- Storage/network/cluster changes.
- SSH or command execution.
- Console/log streaming.
- Guest-level CPU/RAM diagnosis.
- Historical charts or retention.

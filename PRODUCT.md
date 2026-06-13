# PRODUCT: Power Sentinel

## Reframe

Power Sentinel is a local homelab companion. It currently ships as a clean NUT Monitor baseline, with the Ledcards NUT UI as the design anchor; future capabilities return as independently scoped modules, not as coupled legacy dashboard code.

## Baseline product

NUT Monitor on M5Stack CoreS3 + LLM Kit:

- local Linux backend reads UPS/NUT state;
- CoreS3 renders one polished fullscreen NUT page;
- CoreS3 uses the internal StackFlow/UART path as the current transport;
- HTTP polling from CoreS3 is out of scope for the current baseline;
- top status bar shows local Module LLM/CoreS3 status only: LAN, Wi-Fi, serial link recency, page dot(s), Module LLM time, and CoreS3 battery;
- no plausible demo telemetry when live data is missing.

## Module roadmap

Stable module/page names:

| Module | Page | Status |
| --- | --- | --- |
| `nut` | `NUT` | implemented baseline |
| `proxmox` | `PROXMOX` | initial read-only API adapter |
| `ha` | `HA` | placeholder, to be reintroduced |

Each module must be independently installable/updateable and independently testable. A module should provide glanceable value, stay within a lightweight integration boundary, or improve a frequent contextual handoff/action.

## Proxmox reintroduction direction

The Proxmox module should return as API-only, read-only observability for one Proxmox Environment. Its first job is cluster/node condition at a glance, with contextual handoff data such as node, storage, guest VMID/name, backup job, timestamp, or concise error context.

The module should not use SSH, run remote commands, control guests, or recreate the Proxmox console. Watched Guests are configured manually by VMID and are expected to be running; a watched guest down is critical. CPU/RAM should affect condition only as sustained node-level pressure. Disk health belongs in scope only when exposed through the Proxmox API with lightweight, actionable signals.

Implementation planning lives in `docs/plans/proxmox-module-v1.md`.

## Integration boundary

Power Sentinel observes and displays conditions. It does not invent a custom shutdown orchestrator or replace specialist consoles.

Real shutdown owner: Standard NUT `upsmon`.

Shutdown intent is true only for `OB LB`. `OL ... LB` means line power is present and must not trigger shutdown.

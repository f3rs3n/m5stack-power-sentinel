# PRODUCT: Power Sentinel

## Reframe

Power Sentinel is a local homelab companion. It currently ships with NUT Monitor and Proxmox as independently scoped Ambient Console modules, with the Ledcards UI as the design anchor; future capabilities return as independently scoped modules, not as coupled legacy dashboard code.

## Baseline product

Ambient Console on M5Stack CoreS3 + LLM Kit:

- local Linux backend reads UPS/NUT state;
- CoreS3 renders polished fullscreen NUT and Proxmox pages when those modules are enabled and observable;
- CoreS3 uses the internal StackFlow/UART path as the current transport, with serial diagnostics for timeout, JSON parse, StackFlow error, and stale-response failures;
- HTTP polling from CoreS3 is out of scope for the current baseline;
- top status bar shows local Module LLM/CoreS3 status only: LAN, Wi-Fi, serial link recency, page dot(s), Module LLM time, and CoreS3 battery;
- Ledcards pages share module-neutral graphics helpers for Ambient Card render data, visual treatment, and physical ring slot geometry while keeping module policy in page models;
- no plausible demo telemetry when live data is missing.

## Module roadmap

Stable module/page names:

| Module | Page | Status |
| --- | --- | --- |
| `nut` | `NUT` | implemented baseline |
| `proxmox` | `PROXMOX` | API-only/read-only five-card Ambient Console module |

Home Assistant remains a Module Candidate, not a runtime placeholder. Add it to the runtime registry only when it exists as a real implemented capability.

Each module must be independently installable/updateable and independently testable. A module should provide glanceable value, stay within a lightweight integration boundary, or improve a frequent contextual handoff/action.

## Proxmox reintroduction direction

The Proxmox module should return as API-only, read-only observability for one Proxmox Environment. Its first job is cluster/node condition at a glance, with contextual handoff data such as node, storage, guest VMID/name, backup job, timestamp, or concise error context.

The module should not use SSH, run remote commands, control guests, or recreate the Proxmox console. Guest Inventory Summary counts visible QEMU VMs and LXC containers, showing running over total and raising attention when visible guests are not running. CPU/RAM should affect condition only as sustained node-level pressure. Disk health belongs in scope only when exposed through the Proxmox API with lightweight, actionable signals.

Implementation planning lives in `docs/plans/proxmox-module-v1.md`.

## Integration boundary

Power Sentinel observes and displays conditions. It does not invent a custom shutdown orchestrator or replace specialist consoles.

Real shutdown owner: Standard NUT `upsmon`.

Shutdown intent is true only for `OB LB`. `OL ... LB` means line power is present and must not trigger shutdown.

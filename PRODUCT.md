# PRODUCT: Power Sentinel

## Reframe

Power Sentinel is now rebuilt around a clean NUT Monitor core. The new Ledcards NUT UI is the design anchor; every other capability returns as an optional page module, not as coupled legacy code.

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
| `proxmox` | `PROXMOX` | placeholder, to be reintroduced |
| `ha` | `HA` | placeholder, to be reintroduced |

Each module must be independently installable/updateable and independently testable.

## Safety boundary

Power Sentinel observes and displays readiness. It does not invent a custom shutdown orchestrator.

Real shutdown owner: Standard NUT `upsmon`.

Shutdown intent is true only for `OB LB`. `OL ... LB` means line power is present and must not trigger shutdown.

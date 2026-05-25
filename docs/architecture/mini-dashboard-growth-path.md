# Mini-dashboard candidates and navigation growth path

Date: 2026-05-25
Status: decision/spec for the next CoreS3 dashboard growth steps
Device: M5Stack CoreS3 + LLM Kit Power Sentinel

## Decision summary

Keep the current five top-level tabs for the next iteration:

1. `HOME`
2. `NUT`
3. `PVE`
4. `HA`
5. `M5S`

Do not add a sixth top-level tab yet. The 320x240 CoreS3 layout already spends 44 px on the left icon rail, leaving about 276 px of content width. That is enough for one dense fixed-height card at a time, not for desktop-style dashboard sprawl. The next useful expansion should happen as additional horizontal carousel cards inside the existing tabs.

Recommended next implementation order:

1. Network detail card inside `HOME` or as a second/third HOME carousel card.
2. NUT clients / Standard NUT readiness card inside `NUT`.
3. M5Stack service / appliance-maintenance card inside `M5S`.
4. PVE storage/ZFS/SMART detail cards inside `PVE`, only where they add actionable state beyond the existing compact PVE card.
5. HA entity/power detail card inside `HA`, only after an explicit allowlist exists.
6. Local LLM incident-summary / companion card, later, after cached/event-driven summary generation exists.

The growth rule is: add cards before tabs; add subpages only after cards become awkward; add a sixth top-level tab only after a subsystem has independent daily operational value and cannot be understood as part of HOME/NUT/PVE/HA/M5S.

## Current navigation model to preserve

Current model:

- fixed 44 px left sidebar rail;
- icon-only top-level tabs;
- vertical swipe changes top-level tabs;
- horizontal swipe changes cards within the active tab;
- short vertical scroll is allowed only inside an overflowing fixed-size card;
- stale/offline/unavailable state must be explicit;
- no plausible demo/sample values after live transport fails.

This interaction split is the right basis for mini-dashboard growth. It protects top-level navigation from becoming a long vertical page and keeps each subsystem discoverable on a desk-visible 320x240 display.

Layout budget:

- screen: 320x240;
- rail: 44 px;
- content width: about 276 px;
- practical card content width after padding: roughly 252 px;
- practical card height: about 220 px;
- safe live card density: one primary status block plus 3-6 short rows/bars/pills;
- unsafe density: tables, long logs, desktop charts, multi-column entity lists, or more than a few large colored widgets.

## Recommended candidates

### P1: Network / router / internet detail

Placement: `HOME` carousel, after the main HOME overview card.

Why it should be next:

- HOME already exposes `NET OK/DOWN/UNK`, so a deeper card is a natural expansion instead of a new tab.
- Network status is a root-cause signal for HA, PVE API, MQTT, and StackFlow/API reachability.
- The backend already has a `network` object based on default-route state plus TCP probe; this can grow incrementally.

Card shape:

```text
NETWORK            OK
Default route      OK
Internet probe     1.1.1.1:53 OK
Gateway            OK / UNK
LAN iface          eth0 up
Last probe         23 ms
Age                2s
```

Optional future rows:

- DNS probe status, if cheap and bounded;
- router reachability, if configured explicitly;
- WAN/IP status only if available without scraping a heavy external service.

Do not include:

- packet graphs;
- full interface tables;
- public IP display by default;
- WiFi/router admin trivia;
- dependency on ICMP ping.

API contract changes:

Extend the existing optional `network` object, tolerating missing fields:

```json
{
  "network": {
    "available": true,
    "severity": "ok",
    "default_route": true,
    "default_interface": "eth0",
    "gateway_ok": true,
    "internet_ok": true,
    "probe": "tcp",
    "target": "1.1.1.1:53",
    "latency_ms": 23,
    "dns_ok": null,
    "router_ok": null,
    "age_seconds": 2,
    "last_error": null
  }
}
```

Firmware changes:

- add a HOME carousel card renderer for `network`;
- preserve the current compact HOME `NET` pill;
- render missing fields as `UNK`, not `DOWN`;
- add static guard labels for `NETWORK`, `Default route`, `Internet probe`, and `Age`;
- update the LVGL MCP fixture/sample payloads and batch screenshots for HOME.

### P2: NUT clients / shutdown-readiness detail

Placement: `NUT` carousel, as a dedicated NUT readiness/client card.

Why it should be next:

- It directly supports the Power Sentinel product goal.
- The Standard NUT decision is already made: real shutdown belongs to `upsmon`, not a custom Proxmox API action.
- The UI currently needs to make staged/disarmed/readiness state clear without implying control.

Card shape:

```text
NUT READINESS       DISARMED
Owner               upsmon
Primary             ready / monitor off
PVE secondary       reachable
Clients             1 total / 0 armed
LB threshold        15% / 180s
```

If NUT can expose connected clients cheaply:

```text
CONNECTED CLIENTS
pve                 reachable_via_upsc
ha                  not_configured
```

Do not include:

- shutdown buttons;
- Proxmox API shutdown controls;
- custom shutdown strategies;
- physical UPS load inventory as if it were live client state.

API contract changes:

Keep using the existing `shutdown` object and optional `nut` object. Add only optional fields needed for display clarity:

```json
{
  "nut": {
    "server_active": true,
    "driver_active": true,
    "monitor_active": false,
    "mode": "netserver",
    "client_count": 1,
    "clients": [
      {"name": "pve", "state": "reachable_via_upsc", "armed": false}
    ]
  },
  "shutdown": {
    "real_shutdown_owner": "upsmon",
    "primary_ready": true,
    "primary_monitor_active": false,
    "secondary_ready": false,
    "armed": false,
    "nut_client_summary": {"total": 1, "connected": 0, "armed": 0},
    "proxmox_nut_client": {"state": "reachable_via_upsc", "armed": false}
  }
}
```

Firmware changes:

- add a NUT readiness/client card;
- keep the current UPS essentials card as the first NUT card;
- ensure the PVE `NUT armed/disarmed` pill still reads only `shutdown.proxmox_nut_client.armed`, not aggregate client readiness;
- add a regression fixture where a non-Proxmox secondary is armed while Proxmox is not, and verify the PVE pill remains disarmed;
- update LVGL MCP fixture samples for NUT and PVE.

### P3: M5S services / appliance-maintenance detail

Placement: `M5S` carousel.

Why it belongs soon:

- M5S is the correct place for transport, service, backend, and appliance diagnostics.
- It avoids polluting HOME with debug counters or version trivia.
- It helps answer whether the physical display is stale because data sources failed, StackFlow failed, or the local API failed.

Card shape:

```text
APPLIANCE           OK
llm-sys             active
sentinel-api        active
stackflow-unit      active
nut-server          active
StackFlow           OK
OpenAI              OK / WARN
Disk/RAM            20GB / 695MB
```

A second diagnostic card can hold transport timing:

```text
TRANSPORT           STACKFLOW
Poll                153 ok / 2 fail
Last fetch          84 ms
Last good           1s ago
Schema              summary.v1
FW                  stackflow-...
```

Do not include:

- long model IDs;
- package versions on the normal card;
- raw systemd logs;
- serial numbers or private configuration.

API contract changes:

The existing `m5stack` object is sufficient, but it can grow optional service detail:

```json
{
  "m5stack": {
    "available": true,
    "severity": "ok",
    "temperature_c": 42.4,
    "ram_available_mb": 695,
    "disk_free_gb": 20.0,
    "uptime_seconds": 86400,
    "services": {
      "llm-sys": "active",
      "power-sentinel-api": "active",
      "power-sentinel-stackflow-unit": "active",
      "nut-server": "active"
    },
    "stackflow_ok": true,
    "openai_ok": true,
    "chat_smoke_ok": null
  }
}
```

Firmware changes:

- split M5S into at least two cards if current content becomes crowded: appliance health, then transport/debug;
- keep firmware-local transport counters local to the display;
- keep OpenAI/chat smoke WARN-level only; never let it imply the sentinel is failing as a power monitor;
- add MCP fixture coverage for the full M5S carousel.

### P4: PVE storage / ZFS / SMART detail

Placement: `PVE` carousel, after the current main Proxmox resource card and workload mini-cards.

Why it is useful but not first:

- ZFS/SMART failures are important, but the main PVE card already summarizes them as compact pills.
- A detail card is useful for fault isolation when those pills are not OK.
- It should not become a full Proxmox UI.

Card shape:

```text
PVE STORAGE         OK
Total               7.2TB
Used                8%
ZFS rpool           ONLINE
SMART               ok
Warnings            0
```

Conditional detail when degraded:

```text
ZFS DEGRADED
Pool rpool          DEGRADED
Errors              2
Last scrub          9d
Disk warning        nvme0
```

Do not include:

- full disk tables;
- every datastore path;
- stopped VM/LXC lists;
- Proxmox API latency in the resource card.

API contract changes:

Prefer extending already-present `proxmox.zfs`, `proxmox.smart`, and storage fields:

```json
{
  "proxmox": {
    "storage_total_bytes": 7897066643456,
    "storage_percent": 8,
    "zfs": {
      "status": "ONLINE",
      "pools": [
        {"name": "rpool", "status": "ONLINE", "capacity_percent": 8, "errors": 0, "last_scrub_age_days": 9}
      ]
    },
    "smart": {
      "status": "OK",
      "failing_count": 0,
      "warning_count": 0,
      "max_temperature_c": null
    }
  }
}
```

Firmware changes:

- add a compact storage detail card only after verifying the current PVE carousel remains readable;
- prefer problem-first rendering: if all OK, show a terse summary; if not OK, spend rows on the failing pool/disk;
- update static UI guard to prevent return of `Temp n/a`, `PVE RO`, or generic desktop table labels.

### P5: HA entity / power-detail allowlist

Placement: `HA` carousel.

Why it is conditional:

- HA can easily explode into an unbounded entity browser.
- The CoreS3 should show operational sentinel data, not duplicate the Home Assistant UI.
- A small allowlisted set could be useful if it answers power/network/sensor questions that belong on the sentinel.

Acceptable card shape:

```text
HA POWER            OK
UPS plug            online
Router plug         online
Rack temp           24.1C
Updates             0
Z2M devices         29/29
```

Requirements before implementation:

- explicit backend allowlist in root-only runtime config or sanitized config template;
- short display names;
- known units/classes;
- clear unavailable state;
- cap visible entities to 4-6 rows.

Do not include:

- arbitrary HA entity browsing;
- automations/scripts/buttons;
- Supervisor internals;
- Z2M version trivia or birth-topic debug noise.

API contract changes:

Add optional allowlisted entity summaries under `homeassistant`, not a raw `/api/states` dump:

```json
{
  "homeassistant": {
    "available": true,
    "severity": "ok",
    "updates": 0,
    "entities": [
      {"key": "router_plug", "label": "Router plug", "state": "on", "severity": "ok"},
      {"key": "rack_temp", "label": "Rack temp", "value": 24.1, "unit": "C", "severity": "ok"}
    ]
  }
}
```

Firmware changes:

- render only the allowlisted summaries;
- cap rows and clip labels;
- show `No HA details configured` when the list is empty;
- never let HA entity failures override core HA API/MQTT severity without an explicit severity mapping.

### P6: Local LLM incident summary / companion

Placement: later `HOME`, `M5S`, or a future sixth tab only after proving it is useful.

Why it should wait:

- Local LLM inference is not yet part of the implemented dashboard contract.
- The CoreS3 cannot afford slow, changing prose during routine polling.
- Generated text must not replace direct telemetry.

Acceptable first version:

- backend generates a cached incident summary only on state transition or explicit slow timer;
- CoreS3 displays a short, cached, timestamped summary card;
- summary has explicit source/age and falls back to structured problems when unavailable.

Card shape:

```text
INCIDENT SUMMARY    12:41
UPS online. PVE, HA, MQTT OK.
No active problems.

Source llm cached 9m
```

Do not include:

- free-form chat as a default top-level tab;
- live token streaming to the CoreS3;
- LLM-controlled shutdown or remediation;
- ambiguous advice that conflicts with telemetry.

API contract changes:

Add a small optional object only after backend caching exists:

```json
{
  "assistant_summary": {
    "available": true,
    "severity": "ok",
    "text": "UPS online. PVE, HA, MQTT OK. No active problems.",
    "generated_at": "2026-05-25T10:41:00Z",
    "age_seconds": 540,
    "source": "local_llm_cached"
  }
}
```

Firmware changes:

- fixed-height text box with clipping/ellipsis;
- show source and age;
- no streaming UI;
- no automatic navigation to the card.

## Rejected near-term candidates

### Sixth top-level tab now

Reject for now.

Reason: the 44 px icon rail and 320x240 display can technically hold another icon, but a sixth tab increases cognitive load and fixture/test surface before any new subsystem has earned independent daily value. The current five tabs already map cleanly to the product model: overview, power/NUT, Proxmox, Home Assistant, and local appliance/transport.

Revisit only if a subsystem is used frequently, has its own severity model, and feels buried even after a well-designed carousel card exists.

### UPS physical load inventory as a live dashboard

Reject as a near-term live mini-dashboard.

Reason: known physical UPS loads are useful documentation, but they are not dynamically observable through NUT as client state. Showing them as `OK` would be misleading. Static load labels may appear only as low-priority informational text if clearly marked static; live readiness should come from NUT/upsmon-visible client state.

### Generic desktop charts/tables

Reject.

Reason: charts with axes, legends, dense histories, and tables waste the CoreS3's limited pixels and increase LVGL object/draw cost. Tiny sparklines may be tested later, but only for a few values and only after fixture and hardware stability checks.

### Raw debug/API detail page

Reject as user-facing navigation.

Reason: schema names, request IDs, full model IDs, serial numbers, package versions, and logs belong in backend docs or M5S diagnostics, not a top-level page. The normal display should stay operational and readable.

### Proxmox shutdown/control UI

Reject.

Reason: real shutdown is Standard NUT via `upsmon`. Power Sentinel is an observer/readiness dashboard. Do not add Proxmox API shutdown tokens, buttons, or custom shutdown orchestration.

### Standalone local LLM chat tab

Reject for now.

Reason: a chat tab is attractive but not yet justified for this appliance. It would add latency, input complexity, and expectation mismatch. Prefer cached incident summaries first; consider a companion tab only after the summary feature proves useful.

### Router/vendor controller tab

Reject for now.

Reason: network health is important, but a vendor/router page would require another integration surface and could become a full network appliance UI. Start with a compact Network card fed by route/probe/router status.

## Firmware/UI contract changes needed for growth

1. Introduce a per-tab card registry in firmware.
   - Keep five top-level tabs fixed.
   - Let each tab declare multiple card renderers.
   - Keep current horizontal carousel behavior.

2. Preserve scroll position per tab.
   - Continue saving/restoring horizontal carousel scroll offsets across re-renders.
   - Never reset the user to the first card on every poll unless the active card disappeared.

3. Make card rendering allocation-safe.
   - Create/update only the active tab where possible.
   - Keep inactive tab cleanup/lazy rendering to protect heap.
   - Use `LV_LABEL_LONG_CLIP` or bounded labels before `lv_label_set_text` on dynamic text.
   - Null-check LVGL object creation in dynamic helpers.

4. Add fixture parity for every new card.
   - Update `assets/lvgl-spike/power-sentinel-dashboard-fixture.c` whenever firmware UI changes.
   - Keep the 44 px sidebar, horizontal carousel, bright inactive icons, and exact tab/card topology faithful to firmware.
   - Add representative OK/WARN/CRITICAL/UNAVAILABLE payload samples for new mini-dashboard cards.

5. Extend `tools/check_core_s3_ui.py` before adding cards.
   - Guard the five tab names/order.
   - Guard the 44 px left rail and horizontal carousel assumptions.
   - Add required labels for each new card.
   - Add negative checks for rejected strings/patterns, such as `Temp n/a`, generic `Running workloads` fallback, or accidental shutdown-control labels.

6. Keep firmware tolerant of missing fields.
   - `power-sentinel.summary.v1` should grow through optional fields/objects.
   - Missing fields render `unknown`, `unavailable`, `n/a`, or hidden secondary rows depending on context.
   - Missing fields must not become `OK` by default.

7. Keep visual density embedded-safe.
   - Prefer rows, bars, compact pills, and tiny status LEDs/dots.
   - Avoid pill walls, large decorative icon blocks, shadows/masks on dense cards, and multi-column tables.
   - Use LVGL/MCP screenshots as regression evidence, not final physical proof.

8. Keep demo/offline boundaries strict.
   - No boot/demo/sample payload in normal firmware.
   - Every new card must have explicit stale/offline/unavailable rendering.
   - Transport and backend source details belong on M5S, not on every card.

## Backend/API contract principles

1. Keep `power-sentinel.summary.v1` stable.
   - Add optional fields; do not replace the current top-level objects.
   - Old firmware must continue to render honest unavailable state.

2. Keep probes bounded.
   - Network/router/DNS/HA/PVE probes must have short timeouts.
   - StackFlow safe endpoint must remain comfortably inside the CoreS3 serial timeout budget.

3. Avoid raw upstream payloads.
   - Do not send raw HA `/api/states`, raw Proxmox lists, raw NUT dumps, or logs to the CoreS3.
   - Normalize into small display-ready summaries.

4. Keep secrets and private internals out of the display contract.
   - No tokens, serial numbers, private hostnames beyond configured short labels, or full internal logs.
   - Use sanitized templates for any allowlists/config examples.

5. Separate operational severity from debug detail.
   - PVE/HA/MQTT/NUT failures affect operational severity.
   - OpenAI/chat smoke failure is WARN at most.
   - Staged/disarmed NUT monitors are not failures unless the expected armed state is explicitly configured.

## Navigation growth path

### Phase A: five tabs plus richer cards

Use now.

- Add Network card under HOME.
- Add NUT readiness/client card under NUT.
- Split M5S appliance and transport cards if crowded.
- Keep all top-level navigation unchanged.

### Phase B: conditional detail cards

Use after Phase A is stable.

- Add PVE storage/ZFS/SMART detail card.
- Add HA allowlisted power/entity card.
- Add problem-first detail cards that become useful when a subsystem is WARN/CRITICAL.

### Phase C: tap-to-open subpages

Use only if a carousel card becomes too cramped but does not deserve a top-level tab.

Requirements:

- obvious back affordance;
- no modal traps;
- no hidden control actions;
- fixture coverage for both parent card and detail view;
- physical touch usability validation.

Likely subpage candidates:

- ZFS/SMART detail when unhealthy;
- NUT client list if there are many clients;
- HA allowlisted entities if more than six become useful.

### Phase D: sixth top-level tab

Use only after evidence shows a subsystem deserves daily independent navigation.

A sixth tab must pass all gates:

- not expressible as one or two cards under existing tabs;
- independent severity and troubleshooting workflow;
- frequent enough to justify rail clutter;
- icon remains readable in the 44 px rail;
- LVGL MCP fixture and physical display validation pass;
- heap/touch/readability validation on real CoreS3 passes.

Possible future sixth-tab candidates, not approved now:

- `NET`, if network/router detail grows into a daily operational page;
- `AI` or `LLM`, if cached incident summaries mature into a real companion workflow;
- `LOAD`, only if physical/load/client inventory becomes dynamically observable and actionable.

## Acceptance checklist for any new mini-dashboard card

Before claiming a card is ready:

- contract fields are optional and documented;
- backend tests cover present, missing, unavailable, and stale states;
- firmware static guard covers required labels and rejects known bad labels;
- LVGL MCP fixture mirrors firmware topology exactly;
- batch screenshots include every affected tab;
- PlatformIO build passes;
- if data payload grows materially, StackFlow safe endpoint latency and JSON size stay within budget;
- physical CoreS3 readability/touch/heap is validated before promoting from fixture-ready to device-ready.

## Final recommendation

Implement the next dashboard growth in this order:

1. HOME Network detail card.
2. NUT readiness/client card.
3. M5S appliance/transport split if M5S density requires it.
4. PVE storage/ZFS/SMART problem-first detail.
5. HA allowlisted entity/power detail.
6. Cached LLM incident summary only after backend caching exists.

Keep the five-tab model. The current sidebar plus horizontal card carousel is the right scaling mechanism for the CoreS3. A sixth tab should remain a deliberate V2 decision, not the default response to every new mini-dashboard idea.

# Power Sentinel Product Brief

Status: working product contract
Audience: humans and coding agents changing the backend, CoreS3 firmware, LVGL fixture, or documentation

## One-sentence product definition

Power Sentinel is a local homelab power and infrastructure monitor optimized for honest glance-status and fast diagnosis. The current implementation runs on an autonomous M5Stack LLM Module with a CoreS3 embedded HMI observing UPS/NUT, Proxmox, Home Assistant, network, and stack health.

## Product role

Power Sentinel is:

- a local-first observability appliance for power and homelab infrastructure state;
- a NUT server for the directly attached UPS;
- a compact CoreS3 HMI for 2-7 second glance checks;
- an MQTT/Home Assistant publisher for stack sensors and health;
- an extensible base for future small dedicated dashboards and optional local-LLM enrichment.

Power Sentinel is not:

- a general Proxmox/Home Assistant remote-control panel;
- a miniaturized Home Assistant dashboard;
- a raw telemetry browser;
- a demo UI that may show plausible sample values after live transport fails.

## Target user

Primary user: Martino or another technical homelab operator.

Usage context:

- desk/lab visible device;
- quick glance during normal work;
- occasional diagnostic check when something looks degraded;
- poor attention budget: the UI must be readable without studying every value;
- local infrastructure may be partially degraded during power/network events.

The operator is technical, but the CoreS3 screen must still answer operational questions directly. Do not require the operator to mentally decode raw NUT tokens, API names, or implementation details for normal use.

During power events the operator may be stressed, multitasking, or physically away from the main workstation. The CoreS3 must prioritize unambiguous state over completeness. Expert-only interpretation is a failure for Level 1/glance screens.

## Core user questions

The product should quickly answer:

1. Is mains/UPS power OK, and if not, how much battery/runtime remains?
2. Is the UPS/NUT service live, and how many NUT clients are connected?
3. Are Proxmox, Home Assistant, MQTT/Zigbee2MQTT, network, and the M5Stack appliance available, healthy, degraded, or unknown?
4. If something is wrong, which subsystem should I inspect first?
5. Is the CoreS3 display showing fresh live data or stale/offline state?

## V1 product scope

In scope for V1:

- M5Stack LLM Module as the autonomous Linux appliance;
- directly attached UPS over USB OTG served by NUT;
- local `power-sentinel.summary.v1` API consumed by the CoreS3;
- CoreS3 LVGL dashboard with top-level HOME, NUT, PVE, HA, and M5S tabs;
- UPS/NUT live status, including charge level, runtime, load, voltage, service state, connected NUT client count, and optional connected-host list;
- Proxmox node status, capacity, health, interface;
- Home Assistant/MQTT/Zigbee2MQTT health summary;
- M5Stack appliance health summary;
- unknown/unavailable/stale rendering for missing or old data;
- MCP/LVGL rendered visual validation for UI changes.

## Semantic state definitions

Use these terms consistently in backend data, UI labels, colors, and docs.

- `available`: a data source responded and produced data inside the freshness window required by that subsystem.
- `reachable`: a host/service can be contacted. This is weaker than available and much weaker than healthy.
- `online`: a service is up enough for the product-defined function. Use `online` only after the subsystem-specific contract defines what minimum checks it includes.
- `nominal`: the subsystem is operating within expected parameters; stronger than merely available.
- `healthy`: a subsystem is available and has no known warning/critical condition according to its product-defined checks.
- `configured`: config exists or a client/source is known to the system. This is not active or healthy by itself.
- `not_configured`: no usable configuration exists for a feature/client/source. This is not a fault unless the product expects it to be configured.
- `disabled`: intentionally inactive by configuration/operator choice. This is not a fault unless the product expects it to be active.
- `stale`: data exists but is older than the accepted freshness window.
- `unknown`: the system cannot determine the state. Unknown must not be rendered as OK.
- `unavailable`: the source or metric is currently not available.
- `degraded`: available but not fully nominal; operator attention may be useful.
- `critical`: operator attention is urgent or a power/service condition is unsafe.

## Color semantics

Color must follow product semantics, not optimism.

- Green: confirmed-good for a domain-defined predicate, such as healthy, nominal, or within range.
- Cyan/blue: informational or weak-positive states such as reachable, detected, present, configured, synced, or standby.
- Amber/orange: warning, degraded, on-battery, stale-with-last-known-data, unexpectedly disabled/inactive, or attention-needed.
- Red: critical, fault, low battery, unavailable when it breaks the primary monitor function, or dangerous state.
- Gray: unknown, unavailable, not configured, deliberately disabled, or intentionally inactive.

Do not use green for merely `reachable`, `configured`, or `seen` unless the subsystem contract explicitly defines that predicate as success.

## Product personality

The UI should feel:

- precise;
- calm when nominal;
- unmistakable when abnormal;
- compact and modern;
- polished, but never decorative at the expense of state truth;
- operational rather than decorative.

Avoid:

- SaaS-card visual noise;
- badge/pill walls;
- ambiguous feel-good wording;
- optimistic wording that turns missing evidence into success;
- fake controls;
- decorative charts with no decision value;
- hidden dependency on demo/sample data.

## Future direction

Future versions may add:

- backend-owned, downsampled UPS/NUT history/trends for charge, runtime, load, and voltage;
- more mini-dashboard modules;
- a local LLM companion tab or enrichment layer for explanation/summarization;
- richer diagnostic detail pages;
- OTA or improved deployment flow.

LLM output must not be the source of truth for severity or safety-critical state.

Future work must preserve the core direction: the CoreS3 remains a truthful embedded HMI for local observability first, not a tiny general-purpose admin console.

# Power Sentinel

Power Sentinel is a local homelab companion. It makes homelab state visible and hosts lightweight operational modules without being limited to a single infrastructure concern.

## Language

**Power Sentinel**:
A local homelab companion that surfaces homelab state and provides lightweight operational modules. The NUT Monitor is the first module, not the product boundary.
_Avoid_: UPS monitor, infrastructure dashboard

**Module**:
An independently scoped, individually enableable capability of Power Sentinel with its own operational responsibility, configuration/lifecycle, and user-facing surface. A module may be observational, interactive, or both, but interactive actions must stay lightweight, frequent, reversible, and inside the Integration Boundary.
_Avoid_: Page module, dashboard tile

**Page**:
A user-interface view that presents an enabled module on the CoreS3 display. A page is one possible surface of a module, not the module itself. Page visibility is decided separately from the module summary: disabled modules do not expose active pages; enabled modules with unavailable or unconfigured data should remain visible with clear unavailable/unconfigured treatment rather than disappearing.
_Avoid_: Module

**Module Page**:
The firmware-side page owned by a specific module. A Module Page owns that module's Page model, Ambient Card mapping, Hero Position policy, unavailable/unconfigured treatment, rendering policy, and touch focus inside the page. It should not own the page registry, current page index, cross-page transitions, top bar, transport status, display mode, or standby policy.
_Avoid_: Ambient Console Shell, backend module, generic plugin

**Page model**:
The module-owned interpreted view that turns summary data into renderer-ready Ambient Cards, page condition, hero priority, and unavailable/unconfigured copy. A Page model is the boundary between module-specific policy and LVGL rendering: the renderer may draw visual classes and geometry, but it should not reinterpret module status, condition priority, or Hero Position rules.
_Avoid_: Raw API payload, LVGL widget tree, shell state

**Overview**:
A page that summarizes the conditions of enabled modules. It should emerge when multiple real modules exist, and should not recreate the old generic HOME dashboard.
_Avoid_: HOME dashboard, widget board

**Local Inference**:
A capability available to Power Sentinel modules when a concrete operational job needs local model reasoning. It is not a requirement for every module and should not replace deterministic safety or status rules.
_Avoid_: AI feature, mandatory module intelligence

**Telemetry History**:
A future cross-module capability for retaining sampled observations over time. Modules should expose current observations first; history should not be reinvented separately inside each module.
_Avoid_: Per-module mini history, current summary

**Integration Boundary**:
The rule that Power Sentinel should prefer lightweight integrations and avoid owning heavy control paths into specialized systems. When investigation or resolution belongs in another system's native interface, Power Sentinel should provide contextual handoff rather than duplicate that interface.
_Avoid_: Full control plane, replacement console

**Contextual Handoff**:
A product behavior where Power Sentinel identifies the authoritative place to continue investigation or action and carries enough context to avoid rediscovery. It is concrete routing context, not generic guidance to open another tool.
_Avoid_: Generic instruction, embedded admin console, full remediation flow

**Ambient Console**:
The CoreS3 display surface for glanceable status and lightweight interaction. Touch interactions belong here only when they are frequent, convenient, and meaningfully simpler than using the authoritative tool.
_Avoid_: Primary workstation, full configuration UI

**Ambient Console Shell**:
The firmware seam that owns cross-page Ambient Console lifecycle. The Shell receives live summaries and transport diagnostics, keeps the page registry/page count/current page index, routes page-level touch intent, runs page transitions, renders the top bar, and applies display mode, standby, snooze, and common refresh policy. It delegates module-specific Page model construction, Ambient Card interpretation, Hero Position policy, and page rendering policy to Module Pages.
_Avoid_: Module Page, module condition interpreter, generic plugin host

**Auto Page Focus**:
A Shell behavior that temporarily moves the Ambient Console to the enabled Module Page needing attention when a module Condition becomes warning or critical. Auto Page Focus is attention routing, not a replacement for manual page selection or module-owned Hero Position policy.
_Avoid_: Auto module switch, alert takeover, automatic remediation

**Backlight Level**:
A physical brightness step that the CoreS3 display backlight can actually render after display hardware maps requested brightness values to its available output levels. Backlight levels are coarser than the firmware's nominal brightness values.
_Avoid_: PWM step, brightness value, ALS level

**Ambient Card**:
A glanceable module signal with one responsibility and one compact value. An ambient card can be shown in reduced form or promoted into the hero position when its condition is the most relevant signal.
_Avoid_: Dashboard widget, multi-metric panel

**Hero Position**:
The prominent Ambient Console position occupied by the ambient card currently selected by condition priority, default policy, or touch focus. The hero position may add short explanatory or handoff context, but it is not a separate signal from the promoted card.
_Avoid_: Separate status card, page title

**Module Candidate**:
A proposed module that earns product scope by offering glanceable value, staying within the integration boundary, or improving a frequent contextual handoff or lightweight action. Weak candidates should not be added just because data is technically available.
_Avoid_: Data source, integration idea

**Module Configuration**:
A module-local configuration section that contains the module's `enabled` flag and integration-specific settings. NUT Monitor may default to enabled for current baseline compatibility; new modules default disabled unless explicitly enabled. Install, update, and configuration scripts may discover module configuration through the shared `enabled: true|false` convention, but the Module owns interpretation and validation of its own settings. The old central `modules` enablement registry is superseded; do not preserve compatibility for it unless a real deployed migration need is identified.
_Avoid_: Central module registry as source of all settings, scattered global flags, per-module synonyms for enabled, compatibility adapters for superseded config shapes

**Not Implemented**:
A module availability state where the module is declared for roadmap or compatibility reasons but does not exist yet as a real capability. Not implemented modules should not produce fake telemetry. The ModuleRuntime should normally register only real implemented modules; Not Implemented is for roadmap or explicit compatibility needs, not a placeholder pattern.
_Avoid_: Unconfigured, stale, runtime placeholder module

**Disabled**:
A module lifecycle state where the module exists but the user has not enabled it. Real implemented modules should still produce a disabled summary so configuration and diagnostics stay visible, but disabled modules should not expose an active Page and should not contribute a Condition. Do not add a separate availability field just to restate disabled/enabled.
_Avoid_: Unavailable condition, disappearing module, redundant availability field

**Unconfigured**:
A module lifecycle/status state where the module is enabled but lacks the minimum configuration needed to produce useful data. Unconfigured modules should stay visible because the user explicitly enabled them; they normally contribute an unavailable Condition with configuration context rather than introducing a separate unconfigured Condition. On the Ambient Console, an enabled but unconfigured Page should preserve the module's normal shape where useful, render unavailable visual treatment, replace metric values with `--`, and show clear unconfigured copy in card state text instead of showing fake telemetry. Do not call this an overlay unless the renderer actually draws a separate overlay layer.
_Avoid_: Stale, hidden enabled module, unconfigured as canonical condition, fake telemetry

**NUT Monitor**:
The Power Sentinel module that surfaces power state and shutdown condition from NUT and the UPS. It is observational; real shutdown remains owned by NUT and `upsmon`.
_Avoid_: UPS controller, shutdown orchestrator

**Homelab Runtime Reserve**:
A UPS runtime reserve interpreted at homelab scale rather than enterprise scale. A few minutes of estimated runtime can be normal for a healthy homelab UPS under real load; runtime thresholds should reserve critical treatment for the final roughly minute-scale shutdown window instead of treating every sub-5-minute estimate as enterprise-style failure.
_Avoid_: Enterprise runtime target, long-runtime SLA

**Hermes Buddy**:
A candidate module for surfacing Hermes workflow state and lightweight user interrupts. Its first useful job is to show whether Hermes is idle, running, blocked, or needs review, and to support convenient intervention for frequent prompts.
_Avoid_: Generic chatbot, Claude Code clone, IDE on CoreS3

**Home Assistant Module**:
A candidate module for glanceable home or homelab state and lightweight controls that are frequent, reversible, and convenient on the ambient console. It should not replace the Home Assistant dashboard or configuration surface.
_Avoid_: Home Assistant replacement, automation editor

**Proxmox Module**:
A candidate module for synthetic Proxmox observability and contextual handoff, initially aimed at typical single-node homelab installations. Its first job is host-level health at a glance, with enough context to know where to investigate, without becoming a Proxmox control surface.
_Avoid_: Proxmox console, VM control panel

**API-Only Integration**:
A module integration style where Power Sentinel reads from the target system's API and does not use SSH or local shell commands against that system. API-only integrations keep ownership and diagnostics in the authoritative system.
_Avoid_: SSH fallback, remote command runner

**Read-Only Credentials**:
Credentials that allow observation but not mutation in the authoritative system. Modules that do not own control should use read-only credentials so the boundary is enforced outside Power Sentinel code.
_Avoid_: Admin token, write-capable token

**Proxmox Environment**:
The Proxmox area observed by Power Sentinel. In v1 this normally means one single-node homelab installation rather than a clustered installation.
_Avoid_: Cluster-first model, multi-node dashboard

**Guest**:
A Proxmox workload observed by Power Sentinel, covering both virtual machines and containers. Guest state may contribute to a Proxmox condition, but Power Sentinel should not become the guest control surface.
_Avoid_: VM when containers also apply

**Guest Inventory Summary**:
A compact Proxmox observation of how many guests are running out of the total guests visible to Power Sentinel. It is a summary signal for ambient status, not a configured allowlist of important guests.
_Avoid_: Watched Guest, per-guest dashboard, VM-only summary

**Sustained Resource Pressure**:
A Proxmox condition input where CPU or memory pressure remains high long enough to matter. Brief spikes should not affect the user-facing condition.
_Avoid_: Transient spike, instantaneous load

**Physical Network Throughput**:
A compact Proxmox observation of current traffic on physical network interfaces. It excludes virtual guest interfaces and bridge-only noise unless the physical uplink cannot be observed directly.
_Avoid_: Network configuration, virtual NIC inventory, bridge dashboard

**Disk Health**:
A Proxmox condition input for storage or disk problems that are exposed through a lightweight and reliable integration. Disk health should influence condition only when the signal is actionable enough for contextual handoff.
_Avoid_: Deep disk diagnostics

**Condition**:
The interpreted state of a module or homelab area, such as healthy, stale, warning, or critical. A condition tells the user whether attention or contextual handoff is needed; it does not imply Power Sentinel owns remediation.
_Avoid_: Readiness, status when meaning interpreted severity

**Healthy**:
A condition where the module or homelab area does not require attention.
_Avoid_: OK

**Neutral Visual Treatment**:
A visual treatment for healthy observations that are informational rather than health-positive. Neutral is a subset of healthy observations for presentation only; it is not a canonical condition and does not participate separately in condition aggregation.
_Avoid_: Neutral condition, OK, unknown

**Stale**:
A condition where previously useful data is no longer fresh enough to trust. Stale points to a recency or data-flow problem, not necessarily a problem in the observed system.
_Avoid_: Unavailable, offline

**Warning**:
A condition where attention is needed but the situation is not immediately critical.
_Avoid_: Error

**Critical**:
A condition where urgent attention or a safety-relevant contextual handoff is needed.
_Avoid_: Fatal

**Unavailable**:
A condition where the module or source cannot currently produce useful data, usually because it is disabled, unconfigured, unreachable from startup, or not implemented.
_Avoid_: Stale, unknown

**Unknown**:
A non-canonical condition value to avoid. Prefer a specific condition such as stale or unavailable with enough context to explain why.
_Avoid_: Canonical condition

**Status**:
A raw or directly reported fact from a module or external system. Status can feed a condition, but it is not the user-facing interpretation of severity or attention.
_Avoid_: Condition, severity

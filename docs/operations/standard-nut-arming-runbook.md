# Standard NUT arming runbook and control boundary

Date: 2026-05-25
Status: deliberate operations note; no service changes executed by this document
Scope: M5Stack LLM Module NUT server/service side, CoreS3 dashboard behavior, and observed downstream client readiness

## Short version

Power Sentinel is a Standard NUT dashboard/readiness observer. It does not own arbitrary downstream clients and must not become a Proxmox shutdown orchestrator.

Current verified state:

- The UPS is attached to the M5Stack LLM Module over USB OTG and served by NUT as `homelab_ups`.
- `nut-server` / `upsd` and the driver path are active so telemetry remains available.
- The M5Stack LLM Module primary `upsmon` path was tested successfully and then deliberately stopped/disabled.
- The first Proxmox secondary `upsmon` path was tested successfully and then deliberately stopped/disabled.
- The current project state is therefore tested-but-disarmed: telemetry and readiness observation are live, shutdown automation is not armed.

Normal operation, when intentionally armed later, is Standard NUT only:

- M5Stack LLM Module: `upsmon` primary for the directly attached UPS.
- Proxmox and other hosts: each host owns its own `upsmon` secondary configuration, arming, disarming, and local shutdown command.
- Power Sentinel reports readiness and monitor state. It does not remotely enable, disable, start, stop, or otherwise manage downstream clients.

## Terminology

Avoid the ambiguous phrase "server disarm" in user-facing docs and UI. It can mean too many unsafe things in NUT.

Use these component-scoped terms instead:

- NUT telemetry online: `upsd` plus the UPS driver are running and `upsc` can read UPS variables.
- NUT shutdown automation armed: the relevant host's `upsmon` service is enabled/running with a valid `MONITOR` line and shutdown configuration.
- NUT shutdown automation hold: a deliberate maintenance state where local `upsmon` is stopped/disabled or otherwise prevented from initiating local shutdown, while telemetry can remain online.
- Downstream client readiness: Power Sentinel can observe whether a known client appears configured/reachable/connected/armed, but the client remains outside Power Sentinel control.
- FSD: NUT forced-shutdown signaling. Do not use FSD as a maintenance/disarm mechanism. `upsmon -c fsd` is an emergency shutdown/control path, not a reversible UI toggle.

NUT upstream docs describe `upsmon` as the shutdown controller: it monitors UPS state and shuts down systems during power failures. `upsmon.conf` defines the monitored UPSes and the local shutdown behavior. Primary/secondary roles are NUT roles: a primary generally runs on the host directly attached to the UPS; a secondary draws power from the UPS but talks to a remote `upsd`.

Compatibility note: the current M5Stack LLM Module NUT version uses legacy `master`/`slave` config keywords. In project docs and UI, those map to Standard NUT primary/secondary roles.

## Current tested-but-disarmed state

Expected current LLM Module service policy:

```text
nut-server: enabled/active
nut-driver: static/active
nut-monitor: disabled/inactive on the M5Stack LLM Module primary
/etc/nut/nut.conf: MODE=netserver
```

Expected current first Proxmox secondary policy:

```text
nut-monitor: disabled/inactive after explicit safety disable
nut.target: disabled/inactive after explicit safety disable
/etc/nut/nut.conf: MODE=netclient
/etc/nut/upsmon.conf: prepared, but the service is not enabled/started
```

This is intentional during project development. It preserves live NUT telemetry and API/UI readiness state without leaving an unattended shutdown path armed.

## Normal armed operation model

When the operator deliberately chooses to arm Standard NUT later, the intended order is:

1. Verify the LLM Module NUT server/driver and UPS telemetry are healthy.
2. Verify the LLM Module primary `upsmon` config, credentials, `SHUTDOWNCMD`, and `POWERDOWNFLAG` behavior.
3. Enable/start the LLM Module primary `nut-monitor`.
4. Verify the primary monitor is active and the Power Sentinel API reports `shutdown.primary_monitor_active=true` / `shutdown.armed=true` as applicable.
5. On each downstream client, using that host's own admin procedure, verify and enable/start that host's secondary `nut-monitor`.
6. Verify downstream client readiness from both sides: the client sees `upsc homelab_ups@192.168.2.202`, and Power Sentinel reports the client's readiness/armed status if it is in the configured client inventory.

Important boundary: step 5 is performed on the downstream host by the downstream host operator/procedure. Power Sentinel may show readiness state for Proxmox or another known client, but it must not remotely arm or disarm that host.

## Maintenance/disarmed modes

Use the narrowest hold that satisfies the maintenance need.

### Telemetry-only development hold (current default)

Purpose: keep UPS telemetry/API/UI active while avoiding automatic local shutdown.

LLM Module state:

```text
nut-server/upsd: running
NUT driver: running
nut-monitor/upsmon: stopped and disabled
```

Downstream clients:

```text
Each client chooses its own state. Power Sentinel only observes/reports readiness.
```

This is the safest default while firmware/backend/UI work is ongoing.

### LLM Module local shutdown-automation hold

Purpose: temporarily prevent the LLM Module local primary `upsmon` from initiating local shutdown during maintenance or controlled development, while keeping `upsd` and the driver running so telemetry remains visible.

Mechanism if manually used:

```text
Stop/disable only the LLM Module `nut-monitor` / `upsmon` service.
Leave `nut-server` / `upsd` and the UPS driver running.
Do not alter downstream clients.
Do not issue FSD.
Do not stop all NUT services unless telemetry itself is the maintenance target.
```

Do not describe this as a future CoreS3 feature. The product decision is read-only observation: Power Sentinel should report the local primary `upsmon` state and downstream client readiness, but should not expose a hold/release button or any API path that controls NUT client services.

### Full NUT telemetry outage

Purpose: driver/server maintenance, package upgrade, USB/UPS troubleshooting.

Mechanism:

```text
Stop affected NUT server/driver services intentionally and expect Power Sentinel to show NUT unavailable/stale.
```

This is not a shutdown-automation hold; it removes telemetry. It should stay a shell/runbook action, not a CoreS3 touch control.

### FSD / forced-shutdown paths

FSD is not a maintenance hold. Do not expose FSD as a CoreS3 button. Do not use FSD to test whether a UI toggle works.

## Verification commands

These are operator commands, not commands executed by this documentation task.

### LLM Module telemetry and service state

```bash
ssh m5stack 'upsc homelab_ups@localhost ups.status battery.charge battery.runtime ups.load input.voltage'
ssh m5stack 'systemctl is-enabled nut-server nut-driver nut-monitor 2>/dev/null || true'
ssh m5stack 'systemctl is-active nut-server nut-driver nut-monitor 2>/dev/null || true'
ssh m5stack 'systemctl status nut-server nut-driver nut-monitor --no-pager'
ssh m5stack 'ss -ltnp | grep :3493 || true'
```

Expected in the current staged/disarmed state:

```text
upsc returns live UPS variables, usually including ups.status=OL while grid is online
nut-server active
nut-driver active/static depending on packaging
nut-monitor disabled/inactive on the M5Stack LLM Module
upsd listening on 3493 locally and on the LAN address used by clients
```

### LLM Module API/readiness state

```bash
ssh m5stack "curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool"
```

Fields to inspect:

```text
ups.available
ups.status
shutdown.real_shutdown_owner
shutdown.primary_ready
shutdown.primary_monitor_active
shutdown.armed
shutdown.nut_clients[]
shutdown.proxmox_nut_client
proxmox.shutdown_state   # compatibility/display only; real path is NUT secondary upsmon
```

### First Proxmox secondary readiness, observed only

Run from the operator shell or on the Proxmox host; Power Sentinel should not run these as remote control actions.

```bash
ssh pve 'command -v upsc upsmon || true'
ssh pve 'dpkg-query -W nut-client nut-server nut 2>/dev/null || true'
ssh pve 'timeout 3 bash -lc "</dev/tcp/192.168.2.202/3493" && echo tcp_3493_ok || echo tcp_3493_fail'
ssh pve 'upsc homelab_ups@192.168.2.202 ups.status battery.charge battery.runtime ups.load'
ssh pve 'systemctl is-enabled nut-monitor nut.target 2>/dev/null || true'
ssh pve 'systemctl is-active nut-monitor nut.target 2>/dev/null || true'
```

Expected in the current staged/disarmed state:

```text
Proxmox can read the remote UPS with upsc.
Proxmox nut-monitor/nut.target are disabled/inactive unless the Proxmox operator deliberately armed them.
Power Sentinel may report `reachable_via_upsc` / not armed for this client.
```

## Manual arming procedure, when deliberately chosen

Preconditions:

- UPS telemetry is healthy.
- The operator accepts that Standard NUT may shut down systems on a real low-battery event.
- Config files are backed up or otherwise recoverable.
- `upsmon.conf` on each host contains correct credentials and role.
- Primary host has a valid `SHUTDOWNCMD` and primary-side late-shutdown/`POWERDOWNFLAG` behavior appropriate for that OS/package.
- Each downstream host owner has approved arming that host's own `upsmon`.

LLM Module primary:

```bash
ssh m5stack 'systemctl enable --now nut-monitor'
ssh m5stack 'systemctl is-enabled nut-monitor && systemctl is-active nut-monitor'
ssh m5stack 'systemctl status nut-monitor --no-pager'
ssh m5stack "curl -fsS 'http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1' | python3 -m json.tool"
```

First Proxmox secondary, if and only if the Proxmox operator chooses to arm it:

```bash
ssh pve 'systemctl enable --now nut-monitor nut.target'
ssh pve 'systemctl is-enabled nut-monitor nut.target && systemctl is-active nut-monitor nut.target'
ssh pve 'systemctl status nut-monitor --no-pager'
```

Postconditions:

- LLM Module primary monitor is active.
- Each intentionally armed downstream host shows its own `nut-monitor` active.
- Power Sentinel shows the LLM Module primary and configured clients as armed/ready as applicable.
- No Power Sentinel API endpoint or CoreS3 control is required to perform client arming.

## Rollback / recovery commands

Use these to return to the current development-safe state.

LLM Module primary shutdown-automation hold:

```bash
ssh m5stack 'systemctl disable --now nut-monitor'
ssh m5stack 'systemctl is-enabled nut-monitor 2>/dev/null || true; systemctl is-active nut-monitor 2>/dev/null || true'
ssh m5stack 'upsc homelab_ups@localhost ups.status battery.charge battery.runtime ups.load'
```

Expected rollback state:

```text
nut-monitor disabled/inactive
nut-server and driver still active
upsc still works
Power Sentinel API/UI still reports UPS telemetry and a disarmed/not-armed shutdown state
```

Downstream client rollback is not a Power Sentinel action. If a Proxmox operator chooses to disarm Proxmox:

```bash
ssh pve 'systemctl disable --now nut-monitor nut.target'
ssh pve 'systemctl is-enabled nut-monitor nut.target 2>/dev/null || true; systemctl is-active nut-monitor nut.target 2>/dev/null || true'
```

If telemetry is broken after maintenance:

```bash
ssh m5stack 'systemctl status nut-server nut-driver --no-pager'
ssh m5stack 'journalctl -u nut-server -u nut-driver -n 100 --no-pager'
ssh m5stack 'upsc homelab_ups@localhost || true'
ssh m5stack 'lsusb | grep -i "051d:0002\|American Power" || true'
```

## CoreS3 / API control boundary

Product decision: do not implement a CoreS3 button or backend write API for NUT arming, disarming, hold, release, or LAN-wide maintenance control.

Reason: NUT does not provide a native, safe remote-control function for disabling all registered `upsmon` clients. Implementing such a button would require custom orchestration such as SSH, agents, Ansible, systemd remote control, or client-side APIs. That reintroduces the custom shutdown/orchestration role that Power Sentinel intentionally avoids.

Therefore:

- No LAN-wide NUT disarm/hold button.
- No local LLM Module `upsmon` hold/release button in the normal CoreS3 UI.
- No SSH/control requirement from Power Sentinel to Proxmox, Home Assistant, or other NUT clients.
- No client agent requirement.
- No Power Sentinel API endpoint that starts/stops/enables/disables `nut-monitor` on clients.
- No FSD control in the UI/API.

Power Sentinel remains an observer:

- Show whether `upsd`/driver telemetry is available.
- Show local primary `upsmon` armed/disarmed state.
- Show known client readiness/armed state when available from inventory/probes.
- Leave actual client arming/disarming to each host's own NUT/admin procedure.

### Product/UI state and follow-up

The current CoreS3 NUT tab is a read-only Mini Nutify-style carousel: `UPS`, `BATTERY`, `POWER`, and `PROTECTION`.

- `UPS` shows synthetic UPS state, model, battery/runtime, load/power, and input voltage without duplicating `ONLINE` as body text.
- `BATTERY` uses a header pill for compact battery state (`CHARGING`, `DISCHARGING`, `LOW`, `UNKNOWN`) and keeps charge/runtime/voltage in the body.
- `POWER` shows electrical/load metrics and hides output voltage when NUT does not expose it.
- `PROTECTION` separates `upsd`/driver service health from client readiness, summarizes `Connected clients N/T`, and renders each known `upsmon` client as a thin role/name row with a state badge.

Preserve these semantic boundaries:

- `upsd`/driver telemetry health is separate from `upsmon` shutdown automation.
- Primary/secondary are `upsmon` roles, not `upsd` roles.
- A reachable client is not necessarily armed.
- Avoid implying that Power Sentinel can or should control downstream clients.

## Design constraints to preserve

- Power Sentinel observes downstream clients; it does not own them.
- Proxmox shutdown remains Standard NUT secondary `upsmon` on Proxmox, not Proxmox API orchestration from Power Sentinel.
- LLM Module primary `upsmon` is the final local shutdown participant for the appliance; disabling it is a maintenance/development hold, not the main dashboard goal.
- NUT server/driver telemetry should remain online whenever possible, even when local shutdown automation is on hold.
- Public docs and repository files must stay sanitized; do not commit live credentials, monitor passwords, or UPS serials beyond already accepted sanitized/current-state context.

## Source references

- Network UPS Tools `upsmon(8)`: https://networkupstools.org/docs/man/upsmon.html — `upsmon` is the shutdown controller and supports primary/secondary roles plus `upsmon -c fsd`, `stop`, and `reload` control commands.
- Network UPS Tools `upsmon.conf(5)`: https://networkupstools.org/docs/man/upsmon.conf.html — `upsmon.conf` defines monitored UPS systems and shutdown behavior; `MONITOR` includes role, credentials, and power value; primary systems need correct late-shutdown behavior such as `POWERDOWNFLAG`.
- Network UPS Tools `upsd.users(5)`: https://networkupstools.org/docs/man/upsd.users.html — `upsmon primary` / `upsmon secondary` user roles grant the corresponding monitor capabilities; administrative `FSD` capability is powerful and must not be exposed casually.
- Project current state: `docs/operations/current-state.md`.
- Backend operations and client-readiness details: `docs/operations/backend-ops.md`.
- CoreS3 UI contract: `docs/architecture/core-s3-pages-v1.md`.

# Standard NUT arming runbook and maintenance-control gate

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

This is the only mechanism that is plausibly compatible with a future conservative CoreS3 maintenance control, and only if additional safety gates are implemented.

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

## CoreS3 local maintenance control evaluation

Recommendation for V1: do not add a normal CoreS3 NUT tab button for arming/disarming. Keep the default path as operator shell/runbook action.

A future control is only conditionally viable if it is narrowly scoped as LLM Module local NUT shutdown-automation hold/release. It must not control downstream clients. It must not trigger FSD. It must not stop telemetry by default. It must not implement custom shutdown orchestration.

### If implemented later, the control should mean exactly this

- Hold: stop/disable `nut-monitor` / `upsmon` on the LLM Module only.
- Release: enable/start `nut-monitor` / `upsmon` on the LLM Module only, after preflight checks pass.
- Leave `nut-server` / `upsd` and NUT drivers running so telemetry remains available.
- Do not change Proxmox, Home Assistant, or any other downstream client's service state.
- Do not edit NUT config files from the CoreS3 path in V1.
- Do not call `upsmon -c fsd`.

Operator-facing label should be explicit, for example:

```text
Local NUT shutdown hold
LLM Module only. Telemetry stays online. Clients are not controlled.
```

Avoid labels like `Disarm NUT server`, `Shutdown off`, or `Disable UPS`, because those imply the wrong component or hide the blast radius.

### Required safety gates before any UI/API implementation

Backend/API:

- Feature flag disabled by default.
- Local-only admin API; no unauthenticated LAN write endpoint.
- Authentication/authorization appropriate for a privileged service action.
- Explicit action names such as `hold_local_upsmon` and `release_local_upsmon`, not vague `disarm`.
- Mandatory finite TTL for a hold, with auto-release or persistent degraded-safety alarm when TTL expires/fails.
- Operator reason required and audit logged.
- Preflight checks before release: UPS telemetry available, config present, credentials sane enough for `upsmon` start, service not masked, no active low-battery/FSD state.
- Idempotent service operations with read-after-write verification.
- Failure must leave a clear degraded state in the API summary and logs.

CoreS3 UX:

- Hidden behind an admin/maintenance affordance, not on the normal first NUT card.
- Multi-step confirmation with clear blast-radius text.
- Accidental-touch prevention: long press, hold-to-confirm, or equivalent deliberate gesture; no single tap.
- Visible active hold banner with remaining TTL and `LLM only` scope.
- Release/re-arm flow must be at least as deliberate as hold, because release re-enables shutdown automation.
- If auth/session/feature flag/preflight is unavailable, render as disabled with a reason, not as a tappable control.

Failure modes that must be designed before implementation:

- API request succeeds but systemd action fails.
- `nut-monitor` starts then exits due to config/credential errors.
- CoreS3 loses transport during a hold.
- LLM Module reboots during a hold.
- UPS goes on battery/low battery during a hold.
- TTL expiry cannot release the hold.
- Operator believes Proxmox was controlled by the button; UI text must prevent this.

### Product decision

For now, this belongs in the runbook, not on-device. The CoreS3 should continue to show state/readiness and make the disarmed/armed condition obvious. If the project later needs field maintenance without shell access, implement the conservative local-only hold/release design above behind a disabled-by-default feature flag and security review.

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

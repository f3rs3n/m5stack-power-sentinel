# Proxmox Module

The Proxmox Module is a supported optional module for public v1. It is disabled by default, API-only, read-only, and designed for a typical single-node homelab Proxmox installation.

It is not a Proxmox console replacement.

## Boundaries

Public v1 Proxmox integration uses the Proxmox API only:

- API-only observation.
- read-only credentials.
- single-node-oriented v1 behavior.
- no SSH.
- no remote shell commands.
- no guest start/stop/reboot controls.
- no backup management.
- no storage or network changes.
- no console/log streaming.

## Minimum configuration

Enable Proxmox in `/etc/power-sentinel.json`:

```json
{
  "proxmox": {
    "enabled": true,
    "api_url": "https://pve.example:8006",
    "token_id": "power-sentinel@pve!monitor",
    "token_secret": "REPLACE_WITH_READ_ONLY_TOKEN_SECRET"
  }
}
```

Use the least-privilege read-only token that can read the API resources documented in `docs/architecture/api-contract-v1.md`. Do not use an admin token unless you have deliberately accepted that risk outside Power Sentinel.

If network observation is ambiguous, add:

```json
{
  "proxmox": {
    "network_uplink": "eno1",
    "network_uplink_speed_mbps": 1000
  }
}
```

`network_uplink_speed_mbps` is only needed when the API does not expose reliable speed data.

## Observed data

With real credentials, the module reads lightweight API resources for cluster/node status, storage, QEMU guests, LXC containers, physical network interfaces, and latest network traffic rates.

The CoreS3 Proxmox Module Page uses five Ambient Cards:

- CPU.
- RAM.
- Guests.
- Storage.
- Network.

The Hero Position is selected by condition priority, default policy, or touch focus. Touch focus is presentation-only and does not control Proxmox.

## Expected states

- `disabled`: Proxmox exists but is not enabled; it does not expose an active page or affect aggregate condition.
- `unconfigured`: the module is enabled but missing minimum config; it reports `condition: unavailable` and uses unavailable visual treatment.
- `not_observed`: the example placeholder secret such as `CHANGE_ME` is still present; the module does not attempt a live API call.
- `api_unavailable`: configured API/auth/transport failed; token secrets must not appear in payloads or signal summaries.
- `observed`: real read-only credentials produced a useful observation.
- `stale`: a prior useful observation is too old to trust.

## Secret safety

Never paste a real `token_secret` into issues, logs, screenshots, serial captures, or examples. The backend tests include coverage that Proxmox API failures do not expose token secrets.

## Verification evidence

The repo regression suite includes Proxmox optional-module behavior, placeholder-secret handling, API failure, first healthy observation, card model, and secret non-leakage tests. Run:

```bash
python3 tools/run_tests.py
python3 tools/check_core_s3_ui.py
```

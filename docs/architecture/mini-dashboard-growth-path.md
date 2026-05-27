# Mini-dashboard Growth Path

Status: design notes for future Power Sentinel dashboard expansion.

## Purpose

Keep future dashboard growth useful and compact. A new mini-dashboard or card must earn its screen space by answering a concrete operational question within the CoreS3 constraints.

## Current top-level tabs

- `HOME`: overall power and homelab status.
- `NUT`: UPS telemetry, NUT service state, connected-client count, optional host list.
- `PVE`: Proxmox node health/capacity/interfaces/workload summary.
- `HA`: Home Assistant, MQTT, Zigbee2MQTT health.
- `M5S`: M5Stack appliance and transport health.

## Candidate future cards

### NUT connected hosts

Placement: `NUT` carousel, as a compact `CLIENTS` card.

Purpose:

- show NUT service state;
- show connected client count;
- optionally list connected host names when the list remains readable.

Example:

```text
CLIENTS             1/3
Server              OK
Driver              OK
Connected           pve
```

Data direction:

```json
{
  "nut": {
    "server_available": true,
    "driver_available": true,
    "connected_client_count": 1,
    "connected_clients": ["pve"]
  }
}
```

### NUT trends

Placement: `NUT` carousel, after current live cards.

Purpose:

- compact load/voltage/runtime trend;
- backend-owned history;
- downsampled points suitable for CoreS3.

Avoid full desktop charts, axes-heavy graphs, or long histories on-device.

### Network / router mini-dashboard

Only promote to a top-level tab if it gains daily operational value beyond the HOME `NET` status. Otherwise keep it as a detail card or omit.

### Storage / workload details

Keep PVE detail compact. VM/LXC lists should not crowd out node health.

## Rules for adding a mini-dashboard

Before adding a new dashboard/card, answer:

1. What question does this answer in 2-7 seconds?
2. Is it more important than existing content on a 320x240 display?
3. What is the unknown/stale/unavailable state?
4. Can it be represented as one metric row, one compact card, or one sparkline?
5. Does it require fixture/render updates and screenshots for every affected card?

## Anti-patterns

- Turning CoreS3 into a full Home Assistant dashboard.
- Adding cards because data exists, not because it changes a decision.
- Showing static labels as if they were live health checks.
- Adding another top-level tab before a card proves daily value.
- Reintroducing raw telemetry dumps.

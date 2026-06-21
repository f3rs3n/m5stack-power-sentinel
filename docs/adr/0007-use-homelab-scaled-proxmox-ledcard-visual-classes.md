# Use homelab-scaled Proxmox Ledcard visual classes

Proxmox Ledcard visual classes should use the full palette for glanceable depth while preserving condition semantics: blue/green/yellow can represent healthy or informational depth, orange means warning attention, red means critical attention, gray means stale telemetry, and purple means unavailable telemetry. CPU, RAM, Storage, and Network keep sustained-pressure semantics where appropriate; Storage remains immediate because capacity pressure is slow-moving.

The Proxmox page is intentionally tuned for a single-node homelab rather than an enterprise cluster console. Medium utilization should add visual texture without becoming alert noise, while sustained high resource pressure, high storage usage, saturated uplink, and stopped guests should still promote the relevant card through the existing hero priority policy.

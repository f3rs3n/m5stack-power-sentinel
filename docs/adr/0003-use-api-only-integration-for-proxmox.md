# Use API-only integration for Proxmox

Power Sentinel will integrate with Proxmox through the Proxmox API only, without SSH fallbacks or remote shell commands. Proxmox credentials should be read-only so the no-control boundary is enforced by the authoritative system, not only by Power Sentinel code. This may defer signals that are not cheaply available through the API, but it keeps Power Sentinel inside its lightweight integration boundary and avoids turning the Proxmox module into a remote administration tool.

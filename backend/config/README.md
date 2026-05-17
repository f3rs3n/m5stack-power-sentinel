# Runtime config directory

This directory is for sanitized examples/templates only.

Do not commit real files copied from `/etc/*` if they contain:

- MQTT passwords;
- NUT user passwords;
- SSH material;
- Home Assistant tokens;
- Proxmox API tokens.

Use `.example.json` / `.example.conf` files for templates.

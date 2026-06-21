# Hardware and transport assumptions

Public v1 is for a technically capable homelab owner using an M5Stack CoreS3 with a Module LLM stack.

## Supported public v1 stack

- M5Stack CoreS3 display.
- M5Stack Module LLM stack running Linux.
- StackFlow service on the Module LLM.
- USB access to the CoreS3 for firmware flashing.
- NUT available on the Module LLM when using the default NUT Monitor module.
- Optional Proxmox API access when using the Proxmox Module.

## CoreS3 to Module LLM transport

The public v1 live firmware uses the internal StackFlow/UART path:

- CoreS3 RX=G18.
- CoreS3 TX=G17.
- `Serial2`.
- 115200 baud.
- StackFlow action: `summary`.

The CoreS3 firmware does not use HTTP polling as a live-data fallback in public v1.

## Power and battery scope

Power Sentinel displays local CoreS3/stack status, but it is not a full-stack UPS for the Module LLM Linux side. UPS shutdown remains the responsibility of NUT and upsmon.

## Maintainer Operations Notes

Hardware discovery and physical validation evidence live in `docs/operations/`. Those notes are maintainer/dev-unit evidence, not the public install path. They may mention specific lab measurements or hardware observations, but a First Public Release User should follow the public docs instead.

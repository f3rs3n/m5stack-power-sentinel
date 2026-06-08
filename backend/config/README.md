# Backend config templates

This directory contains sanitized templates only. Do not commit deployed secrets.

Current clean baseline:

- `power-sentinel.example.json`: module selection; defaults to `nut` only.
- `nut*.example`, `ups*.example`: NUT reference templates.
- `nut-clients.example.json`: optional read-only client inventory for the NUT page.

Removed from active baseline:

- Home Assistant/MQTT publisher config. Reintroduce it only as the `ha` module backend, with separate tests and installer handling.

# CoreS3 display firmware

Clean baseline: NUT Monitor only.

## Build

```bash
platformio run -e m5stack-cores3-ledcards-interface
```

If `platformio` is not on PATH:

```bash
~/.platformio/penv/bin/platformio run -e m5stack-cores3-ledcards-interface
```

## Transport

Default transport is StackFlow over the internal stacked UART:

- CoreS3 RX=G18
- CoreS3 TX=G17
- `Serial2`
- 115200 baud
- `work_id: sentinel`, `action: summary`

HTTP is kept as a development fallback when WiFi is configured in ignored local config.

## UI

`src/main.cpp` only fetches/parses NUT summary fields and drives `ledcards-interface-page.*`.

Legacy HOME/PVE/HA/M5S tab rendering is intentionally absent from this baseline. Reintroduce future pages as separate modules after their backend contracts are restored.

# CoreS3 Firmware Release Path

The public v1 Firmware Release Path is USB/PlatformIO flashing. There is no OTA support and no Module LLM-mediated flashing support in v1.

## Live firmware environment

Use the live firmware environment for real devices:

```bash
cd firmware/core-s3-display
pio run -e m5stack-cores3
```

Before flashing, identify the CoreS3 serial port from PlatformIO's detected devices:

```bash
pio device list
```

Pick the port that corresponds to the CoreS3, such as `/dev/ttyACM0` on Linux or a `COM` port on Windows. Do not assume the example port is correct when several USB serial devices are attached.

Flash over USB with the port you identified:

```bash
pio run -e m5stack-cores3 -t upload --upload-port /dev/ttyACM0
```

Adjust `--upload-port` for your host.

## Transport assumptions

The live firmware uses StackFlow over the internal UART:

- CoreS3 RX=G18.
- CoreS3 TX=G17.
- `Serial2`.
- 115200 baud.
- StackFlow `work_id: sentinel` and action `summary`.

CoreS3 HTTP/Wi-Fi polling fallback is not part of public v1.

## Fixture and development environments

`m5stack-cores3` is the live firmware.

`m5stack-cores3-ledcards-interface` embeds a visual fixture and is for layout checks only. Do not use it as the live release firmware.

`m5stack-cores3-als-probe` is a development-only ambient-light calibration probe and is not the public release firmware.

## Serial capture verification

For non-interactive checks, prefer the repository helper over `pio device monitor`:

```bash
python3 tools/core_s3_serial_capture.py --port /dev/ttyACM0 --duration 15
```

The helper does not reset the CoreS3 by default. Use it after the backend is running to look for StackFlow summary success/recovery breadcrumbs, transport diagnostics, JSON parse failures, StackFlow errors, stale-response reports, or crash/backtrace evidence.

## Not supported in public v1

- No OTA firmware updates.
- No Module LLM-mediated flashing.
- No CoreS3 setup/options page for installation.
- No HTTP/Wi-Fi polling fallback for live summaries.

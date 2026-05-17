# CoreS3 frontend decisions

Status: draft / approved for V0 exploration

## Goal

Build a physical autonomous display for the M5Stack Power Sentinel.

Primary endpoint:

```text
GET http://192.168.2.202:8088/api/v1/summary
```

The display must treat the M5Stack LLM Module API as the primary source, not Home Assistant.

## UI framework decision

Initial recommendation was manual M5GFX drawing for simplicity. After reviewing LVGL visuals and the user's preference for a polished appliance UI, V0 frontend should explore LVGL rather than rejecting it.

Preferred stack for V0 exploration:

```text
PlatformIO
Arduino framework
board = m5stack-cores3
M5Unified
M5GFX / LovyanGFX display/touch backend
LVGL
ArduinoJson
WiFi
HTTPClient
```

Rationale:

- LVGL gives much better visual polish for cards, bars, status badges, tabs, and typography.
- The UI is page/card based, which maps well to LVGL containers.
- The backend API is already simple, so frontend complexity can live in the UI layer.
- M5GFX remains useful as the low-level display/touch bridge.

Risk:

- LVGL has more setup complexity and memory pressure than direct M5GFX drawing.
- If LVGL proves unstable/heavy on CoreS3, fall back to manual M5GFX drawing with the same data model.

Decision gate:

- Build a static LVGL mock with four pages.
- If RAM/refresh/touch are acceptable on real hardware, continue with LVGL.
- If not, keep the visual design but reimplement with M5GFX sprites/manual draw.

## Firmware pages

1. UPS / Power
   - default page
   - works without Home Assistant
   - online/on battery/low battery states
   - battery %, runtime, load, input/output voltage

2. Home Assistant
   - HA API reachability
   - MQTT reachability
   - future critical HA services

3. Proxmox
   - reachability
   - shutdown state
   - future NUT client/read-only status

4. M5Stack/System
   - temperature
   - RAM
   - disk
   - OpenAI API
   - StackFlow API
   - chat smoke

5. Offline / stale fallback
   - backend unreachable
   - cached last-good payload age

## Upload / flashing strategy

### Path A: manual Windows upload for first flash

Most reliable first path.

Flow:

1. Hermes/Codex generates firmware project in GitHub repo.
2. User pulls/clones repo on Windows.
3. User opens `firmware/core-s3-display` in VS Code + PlatformIO.
4. User connects CoreS3 by USB.
5. User puts CoreS3 in download mode: hold reset about 2 seconds until internal green LED lights, then release.
6. User uploads via PlatformIO.
7. Firmware reports status via display/serial.

Pros:

- Avoids Proxmox/LXC USB passthrough friction.
- Best first-flash reliability.
- Works even if CoreS3 changes serial mode during bootloader upload.

Cons:

- User must perform upload manually.

### Path B: USB passthrough into Hermes LXC

Useful if we want fully agent-driven upload from Hermes.

Expected CoreS3 device node:

```text
/dev/ttyACM* or /dev/ttyUSB*
```

Proxmox host should first see the device with something like:

```bash
lsusb
ls -l /dev/ttyACM* /dev/ttyUSB*
dmesg -w
```

For LXC passthrough, likely requirements:

- pass the serial device into LXC 101;
- allow char device major 166 for ttyACM and/or 188 for ttyUSB;
- possibly account for device renumeration when entering download mode.

This is feasible but fiddly. Use only after first manual flash or if manual Windows upload is inconvenient.

### Path C: OTA after first flash

Best long-term workflow.

First firmware should include an OTA path if practical:

- ArduinoOTA, or
- web update endpoint, or
- PlatformIO `espota` upload.

Then future iterations can be uploaded over LAN without USB passthrough.

Preferred sequence:

```text
first flash: Windows/manual USB
future flashes: OTA from Hermes or Windows
fallback: USB passthrough to Hermes LXC
```

## Agent split

Supervisor:

- Hermes main session.
- Owns API contract, repository, safety gates, deploy decisions, and final review.

Frontend implementer:

- Prefer Codex CLI if used for coding-heavy C++/LVGL work.
- Hermes subagent is acceptable for design/review and smaller edits.

Recommended first frontend task:

```text
Create a PlatformIO CoreS3 LVGL static UI mock in firmware/core-s3-display.
No hardware upload required.
It should compile or at least be structurally ready, and include screens/components for UPS, HA, Proxmox, M5Stack, and offline states using sample JSON data.
```

Review gates:

1. Static project skeleton exists.
2. Dependencies and `platformio.ini` are plausible for `m5stack-cores3`.
3. JSON contract is represented in code.
4. UI supports all states from sample payloads.
5. No secrets committed.
6. Upload method documented before requiring hardware.

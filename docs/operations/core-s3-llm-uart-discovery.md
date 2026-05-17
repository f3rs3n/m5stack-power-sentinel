# CoreS3 <-> LLM Module internal UART discovery

Stack currently assembled by Martino, bottom to top:

```text
DIN BASE -> LLM MATE -> LLM MODULE -> CORE S3
```

No DIP/pin switch changes have been made by the user.

## What the M5Stack docs imply

Relevant official docs reviewed:

- Module LLM Kit: the kit uses serial communication; LLM Mate provides M5-Bus stacked power, CH340N USB-to-serial, a core serial port, and FPC-8P direct connection to Module LLM.
- Module LLM: UART serial communication is a first-class interface.
- Module LLM Pins Change: Module LLM can manually switch RX/TX pins to avoid M5-Bus conflicts.
- DIP Switch Pin Switching Instructions: M5-Bus is a 2x15 2.54mm stack bus; switch settings change physical signal routing and firmware pins must match hardware routing.
- DIN Base: CoreS3 PORT.C is mapped as G18/G17 RX/TX; on M5-Bus, PORT.C is pins 15/16.

CoreS3 pin options listed in the Module LLM pin-switching doc:

| Host | RXD options | TXD options |
| --- | --- | --- |
| CoreS3 | G18 / G7 / G14 / G10 | G17 / G13 / G6 / G0 |

Since the user has not changed switches, the first/default-looking CoreS3 pair is the best initial candidate:

```text
CoreS3 RXD = G18
CoreS3 TXD = G17
baud       = 115200 8N1
```

This also matches DIN Base PORT.C for CoreS3:

```text
PORT.C = G18/G17 = RX/TX
```

Important caveat: the docs include a confusing example sentence saying a successful CoreS3 + Module LLM switch ended with "G7 for TX and G17 for RX", while the CoreS3 table lists RXD candidates `G18/G7/G14/G10` and TXD candidates `G17/G13/G6/G0`. Do not hard-code only the example sentence; verify on the assembled stack.

## Linux side observed on the LLM Module

Observed over SSH on `192.168.2.202`:

```text
/dev/ttyS0  root:tty     console, active serial-getty, DO NOT use for probe
/dev/ttyS1  root:dialout candidate
/dev/ttyS2  root:dialout candidate
/dev/ttyS3  root:dialout candidate
/dev/ttyS4  root:dialout candidate
/dev/ttyS5  root:dialout candidate
```

Kernel command line:

```text
console=ttyS0,115200n8 earlycon=uart8250,mmio32,0x4880000
```

Only two UARTs appear in dmesg as MMIO devices:

```text
ttyS0 at 0x4880000  # console
ttyS1 at 0x4881000  # likely first non-console hardware UART
```

Therefore the first Linux-side candidate for CoreS3 internal serial is:

```text
/dev/ttyS1 @ 115200
```

Probe the others only if `/dev/ttyS1` does not receive traffic.

## Safe discovery sequence

1. Do not touch DIP switches yet.
2. Do not use `/dev/ttyS0`; it is the Linux console/getty.
3. Flash a minimal CoreS3 UART probe or add probe mode to the frontend firmware.
4. On CoreS3, configure:

```cpp
HardwareSerial LlmSerial(1);
LlmSerial.begin(115200, SERIAL_8N1, 18, 17); // RX, TX
```

5. Have CoreS3 send a line once per second:

```text
PS1 PING <millis>
```

6. On LLM Module, listen on `/dev/ttyS1` first and reply:

```text
PS1 PONG <same-token>
```

7. If no traffic appears, try `/dev/ttyS2`..`/dev/ttyS5` on Linux first; if still no traffic, inspect/switch the Module LLM pin selector and test alternate CoreS3 pairs:

```text
RX/TX G18/G17
RX/TX G7/G13
RX/TX G14/G6
RX/TX G10/G0   # last resort; G0 can affect bootstrapping
```

## Final transport target

The final Power Sentinel frontend should prefer internal UART over WiFi HTTP when the stack is physically assembled.

Recommended line protocol V1:

Request from CoreS3:

```text
PS1 GET summary
```

Response from LLM Module:

```text
PS1 OK <json-byte-length>
{...power-sentinel.summary.v1...}
```

WiFi HTTP remains useful for development and fallback:

```text
GET http://192.168.2.202:8088/api/v1/summary
```

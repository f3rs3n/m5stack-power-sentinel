# CoreS3 <-> LLM Module internal UART discovery

Stack currently assembled by Martino, bottom to top:

```text
DIN BASE -> LLM MATE -> LLM MODULE -> CORE S3
```

No DIP/pin switch changes were needed.

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

The confirmed default pair for this assembled stack is:

```text
CoreS3 RXD = G18
CoreS3 TXD = G17
baud       = 115200 8N1
```

`115200` is the StackFlow/default service speed used by the current Power Sentinel baseline, not a hardware ceiling. Official Module LLM examples start on `Serial2` at 115200 and some image/YOLO examples renegotiate the link to higher baud rates such as 1500000 for large camera payloads. Keep the baseline at 115200 unless the StackFlow service and CoreS3 firmware are deliberately changed together and validated.

This also matches DIN Base PORT.C for CoreS3:

```text
PORT.C = G18/G17 = RX/TX
```

Important caveat: the docs include a confusing example sentence saying a successful CoreS3 + Module LLM switch ended with "G7 for TX and G17 for RX", while the CoreS3 table lists RXD candidates `G18/G7/G14/G10` and TXD candidates `G17/G13/G6/G0`. Do not hard-code only the example sentence; verify on the assembled stack if hardware changes.

## Linux side observed on the LLM Module

Observed over SSH on `m5stack` (`192.168.2.202`):

```text
/dev/ttyS0  root:tty     console, active serial-getty, DO NOT use
/dev/ttyS1  root:dialout real CoreS3/StackFlow UART, owned by llm_sys
/dev/ttyS2  root:dialout dummy/nonfunctional on this device
/dev/ttyS3  root:dialout dummy/nonfunctional on this device
/dev/ttyS4  root:dialout dummy/nonfunctional on this device
/dev/ttyS5  root:dialout dummy/nonfunctional on this device
```

Kernel command line:

```text
console=ttyS0,115200n8 earlycon=uart8250,mmio32,0x4880000
```

Only two UARTs appear in dmesg as MMIO devices:

```text
ttyS0 at 0x4880000  # console
ttyS1 at 0x4881000  # first non-console hardware UART, used by llm_sys
```

`/dev/ttyS2` through `/dev/ttyS5` returned I/O errors during scan. `/dev/ttyS0` is the Linux console and must not be used.

## Discovery result

Historical probe result, before choosing the final StackFlow architecture:

```text
CoreS3 RX=G18 TX=G17 @ 115200
LLM Module /dev/ttyS1 @ 115200
Bidirectional traffic confirmed.
```

Observed scan output from that discovery phase:

```text
/dev/ttyS1: listening @115200
/dev/ttyS2: open failed: error(5, 'Input/output error')
/dev/ttyS3: open failed: error(5, 'Input/output error')
/dev/ttyS4: open failed: error(5, 'Input/output error')
/dev/ttyS5: open failed: error(5, 'Input/output error')
/dev/ttyS1 RX probe traffic from CoreS3
RESULT seen
```

The discovery scripts and the old direct probe/bridge protocol were removed from the repo after StackFlow was verified. Keep this file as hardware evidence, not as current runbook.

## Current transport target

The final Power Sentinel frontend uses the internal stacked UART when the stack is physically assembled. It speaks official StackFlow JSON through `llm_sys` rather than opening `/dev/ttyS1` from a parallel Linux process.

CoreS3 firmware uses the vendor-tested UART instance and pins:

```cpp
HardwareSerial LlmSerial(2);
LlmSerial.begin(115200, SERIAL_8N1, 18, 17); // RX, TX
```

Prefer `Serial2` / `HardwareSerial(2)` or the M5Unified `port_c_rxd` / `port_c_txd` pin helpers. Avoid treating `HardwareSerial(1)` success/failure as hardware evidence for this stack; the official CoreS3 Module LLM examples use `Serial2` on Port C.

Request from CoreS3:

```json
{"request_id":"ps-N","work_id":"sentinel","action":"summary","object":"None","data":"None"}
```

Response routed back by `llm_sys`:

```json
{"request_id":"ps-N","work_id":"sentinel","object":"power-sentinel.summary.v1","data":{...},"error":{"code":0,"message":""}}
```

Linux-side routing:

```text
llm_sys -> ipc:///tmp/rpc.sentinel -> power-sentinel-stackflow-unit
```

The custom unit calls:

```text
http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1
```

The direct HTTP endpoint can still be queried for backend/manual diagnostics, but it is not a CoreS3 firmware transport in the current baseline:

```text
GET http://192.168.2.202:8088/api/v1/summary
```

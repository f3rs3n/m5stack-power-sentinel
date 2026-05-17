#pragma once

// Copy this file to include/power_sentinel_config.h and edit locally.
// include/power_sentinel_config.h is intentionally ignored by git.

// Leave empty to run the static/demo UI without WiFi.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Backend endpoint. This is not a secret; it is the local Power Sentinel API URL.
#define POWER_SENTINEL_SUMMARY_URL "http://192.168.2.202:8088/api/v1/summary"

// Network/API polling interval for live data.
#define SUMMARY_POLL_MS 30000UL

// Data transport. The final stacked appliance should use the internal UART as
// primary transport and keep WiFi/HTTP as an optional development fallback.
// The Linux-side bridge listens on /dev/ttyS1 and serves this protocol:
//   CoreS3 -> LLM Module: PS1 GET summary
//   LLM Module -> CoreS3: PS1 OK <json-byte-length>
//   LLM Module -> CoreS3: <json>
#define POWER_SENTINEL_TRANSPORT_SERIAL 1
#define POWER_SENTINEL_HTTP_FALLBACK 1
#define POWER_SENTINEL_SERIAL_TIMEOUT_MS 3500UL
#define POWER_SENTINEL_SERIAL_MAX_JSON_BYTES 8192

// CoreS3 stacked 5V bus direction.
// 0 = external-input/sentinel mode: the CoreS3 accepts power from LLM Mate/Module/base,
//     but does not feed 5V back into the stack. This is the safest mode for the final
//     Power Sentinel appliance, where the stack/base is the primary power source.
// 1 = CoreS3-source mode: the CoreS3 enables 5V output to the M-Bus/stack, matching the
//     old behavior where USB-C on the CoreS3 can power the rest of the stack. Do not use
//     this mode when the stack/base is also feeding 5V into the CoreS3.
#define POWER_SENTINEL_STACK_POWER_OUT 0

// Internal CoreS3 <-> LLM Module UART discovery. Keep disabled for normal UI builds.
// Enable temporarily in local power_sentinel_config.h to send `PS1 PING <millis>` once
// per second on the stacked serial pins and print any replies to USB serial.
#define POWER_SENTINEL_UART_PROBE 0
#define POWER_SENTINEL_UART_RX_PIN 18
#define POWER_SENTINEL_UART_TX_PIN 17
#define POWER_SENTINEL_UART_BAUD 115200

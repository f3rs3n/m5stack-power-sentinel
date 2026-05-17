#pragma once

// Copy this file to include/power_sentinel_config.h and edit locally.
// include/power_sentinel_config.h is intentionally ignored by git.

// Leave empty if the stacked UART/StackFlow transport is used as primary data path.
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Backend endpoint. This is not a secret; it is the local Power Sentinel API URL.
#define POWER_SENTINEL_SUMMARY_URL "http://192.168.2.202:8088/api/v1/summary"

// Network/API polling interval for live data.
#define SUMMARY_POLL_MS 30000UL

// Data transport. The final stacked appliance uses the internal UART as
// primary transport, but through vendor StackFlow/llm_sys rather than a
// parallel /dev/ttyS1 bridge:
//   CoreS3 -> LLM Module: {"work_id":"sentinel","action":"summary",...}
//   LLM Module -> CoreS3: StackFlow JSON response with data=power-sentinel.summary.v1
// Keep WiFi/HTTP as an optional development fallback.
#define POWER_SENTINEL_TRANSPORT_SERIAL 1
#define POWER_SENTINEL_HTTP_FALLBACK 1
#define POWER_SENTINEL_SERIAL_TIMEOUT_MS 5000UL
#define POWER_SENTINEL_SERIAL_MAX_JSON_BYTES 8192
#define POWER_SENTINEL_SERIAL_RETRIES 3

// CoreS3 stacked 5V bus direction.
// 0 = external-input/sentinel mode: the CoreS3 accepts power from LLM Mate/Module/base,
//     but does not feed 5V back into the stack. This is the safest mode for the final
//     Power Sentinel appliance, where the stack/base is the primary power source.
// 1 = CoreS3-source mode: the CoreS3 enables 5V output to the M-Bus/stack, matching the
//     old behavior where USB-C on the CoreS3 can power the rest of the stack. Do not use
//     this mode when the stack/base is also feeding 5V into the CoreS3.
#define POWER_SENTINEL_STACK_POWER_OUT 0

#define POWER_SENTINEL_UART_RX_PIN 18
#define POWER_SENTINEL_UART_TX_PIN 17
#define POWER_SENTINEL_UART_BAUD 115200

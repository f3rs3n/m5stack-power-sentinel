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

// Display standby policy. Values are compile-time defaults and may be
// overridden by build flags for short hardware validation runs.
// Auto-standby fades to DIM and stays visible; OFF is reserved for manual
// long-press snooze or for a prolonged missing-payload condition.
#define POWER_SENTINEL_DISPLAY_STANDBY_MS 300000UL
#define POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS 900000UL
#define POWER_SENTINEL_DISPLAY_AWAKE_BRIGHTNESS 160
#define POWER_SENTINEL_DISPLAY_DIM_BRIGHTNESS 48
#define POWER_SENTINEL_DISPLAY_FADE_MS 400UL
#define POWER_SENTINEL_DISPLAY_SNOOZE_MS 1800000UL

// Reserved for future CoreS3 IMU tap/knock wake calibration; not a PIR/presence sensor.
#define POWER_SENTINEL_MOTION_WAKE 0

// Production builds clamp display timeouts shorter than the defaults above, so
// stale local accelerated-test configs cannot make the appliance black out.
// Set to 1 only for deliberate short hardware validation runs.
#define POWER_SENTINEL_ALLOW_SHORT_DISPLAY_TIMEOUTS 0

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

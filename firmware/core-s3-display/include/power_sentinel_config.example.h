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

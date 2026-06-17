#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <lvgl.h>
#include <string.h>

#include "m5_hal.h"
#include "core-s3-transport-diagnostics.h"
#include "ledcards-interface-page.h"
#include "proxmox-ambient-page-model.h"

#if __has_include("power_sentinel_config.h")
#include "power_sentinel_config.h"
#else
#include "power_sentinel_config.example.h"
#endif

#ifndef POWER_SENTINEL_FIRMWARE_BUILD
#define POWER_SENTINEL_FIRMWARE_BUILD "nut-monitor-clean-baseline-2026-06-08"
#endif
#ifndef POWER_SENTINEL_UART_RX_PIN
#define POWER_SENTINEL_UART_RX_PIN 18
#endif
#ifndef POWER_SENTINEL_UART_TX_PIN
#define POWER_SENTINEL_UART_TX_PIN 17
#endif
#ifndef POWER_SENTINEL_UART_BAUD
#define POWER_SENTINEL_UART_BAUD 115200
#endif
#ifndef POWER_SENTINEL_TRANSPORT_SERIAL
#define POWER_SENTINEL_TRANSPORT_SERIAL 1
#endif
#ifndef POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY
#define POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY 0
#endif
#ifndef POWER_SENTINEL_WIFI_SSID
#define POWER_SENTINEL_WIFI_SSID ""
#endif
#ifndef POWER_SENTINEL_WIFI_PASSWORD
#define POWER_SENTINEL_WIFI_PASSWORD ""
#endif
#ifndef POWER_SENTINEL_SERIAL_TIMEOUT_MS
#define POWER_SENTINEL_SERIAL_TIMEOUT_MS 3500UL
#endif
#ifndef POWER_SENTINEL_SERIAL_RETRIES
#define POWER_SENTINEL_SERIAL_RETRIES 3
#endif
#ifndef POWER_SENTINEL_SERIAL_MAX_JSON_BYTES
#define POWER_SENTINEL_SERIAL_MAX_JSON_BYTES 8192
#endif
#ifndef POWER_SENTINEL_DISPLAY_STANDBY_MS
#define POWER_SENTINEL_DISPLAY_STANDBY_MS 300000UL
#endif
#ifndef POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS
#define POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS 900000UL
#endif
#ifndef POWER_SENTINEL_DISPLAY_AWAKE_BRIGHTNESS
#define POWER_SENTINEL_DISPLAY_AWAKE_BRIGHTNESS 120
#endif
#ifndef POWER_SENTINEL_DISPLAY_DIM_BRIGHTNESS
#define POWER_SENTINEL_DISPLAY_DIM_BRIGHTNESS 36
#endif
#ifndef POWER_SENTINEL_DISPLAY_FADE_MS
#define POWER_SENTINEL_DISPLAY_FADE_MS 400UL
#endif
#ifndef POWER_SENTINEL_DISPLAY_SNOOZE_MS
#define POWER_SENTINEL_DISPLAY_SNOOZE_MS 1800000UL
#endif
#ifndef POWER_SENTINEL_MOTION_WAKE
#define POWER_SENTINEL_MOTION_WAKE 0
#endif
#ifndef POWER_SENTINEL_ALS_SENSOR_ENABLED
#define POWER_SENTINEL_ALS_SENSOR_ENABLED 1
#endif
#ifndef POWER_SENTINEL_ADAPTIVE_BRIGHTNESS_ENABLED
#define POWER_SENTINEL_ADAPTIVE_BRIGHTNESS_ENABLED 1
#endif
#ifndef POWER_SENTINEL_ALS_POLL_MS
#define POWER_SENTINEL_ALS_POLL_MS 100UL
#endif
#ifndef POWER_SENTINEL_ALS_WARMUP_SAMPLES
#define POWER_SENTINEL_ALS_WARMUP_SAMPLES 4
#endif
#ifndef POWER_SENTINEL_ALS_EMA_ALPHA_NUM
#define POWER_SENTINEL_ALS_EMA_ALPHA_NUM 2
#endif
#ifndef POWER_SENTINEL_ALS_EMA_ALPHA_DEN
#define POWER_SENTINEL_ALS_EMA_ALPHA_DEN 3
#endif
#ifndef POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_STEP
#define POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_STEP 1
#endif
#ifndef POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_HIGH_MS
#define POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_HIGH_MS 96UL
#endif
#ifndef POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_MID_HIGH_MS
#define POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_MID_HIGH_MS 136UL
#endif
#ifndef POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_MID_LOW_MS
#define POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_MID_LOW_MS 160UL
#endif
#ifndef POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_LOW_MS
#define POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_LOW_MS 192UL
#endif
#ifndef POWER_SENTINEL_ALS_BRIGHTENING_DEBOUNCE_MS
#define POWER_SENTINEL_ALS_BRIGHTENING_DEBOUNCE_MS 300UL
#endif
#ifndef POWER_SENTINEL_ALS_DARKENING_DEBOUNCE_MS
#define POWER_SENTINEL_ALS_DARKENING_DEBOUNCE_MS 1000UL
#endif
#ifndef POWER_SENTINEL_ALS_TARGET_FILTER_SHIFT
#define POWER_SENTINEL_ALS_TARGET_FILTER_SHIFT 0
#endif
#ifndef POWER_SENTINEL_ALS_TARGET_DEADBAND
#define POWER_SENTINEL_ALS_TARGET_DEADBAND 2
#endif
#ifndef POWER_SENTINEL_ALS_DEBUG_SERIAL
#define POWER_SENTINEL_ALS_DEBUG_SERIAL 0
#endif
#ifndef POWER_SENTINEL_ALS_DEBUG_OVERLAY
#define POWER_SENTINEL_ALS_DEBUG_OVERLAY 0
#endif
#ifndef POWER_SENTINEL_ALS_WAKE_ON_LIGHT
#define POWER_SENTINEL_ALS_WAKE_ON_LIGHT 0
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_RAW_1
#define POWER_SENTINEL_ALS_INTERPOLATION_RAW_1 12
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_RAW_2
#define POWER_SENTINEL_ALS_INTERPOLATION_RAW_2 45
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_RAW_3
#define POWER_SENTINEL_ALS_INTERPOLATION_RAW_3 70
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_RAW_4
#define POWER_SENTINEL_ALS_INTERPOLATION_RAW_4 95
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_MAX_RAW
#define POWER_SENTINEL_ALS_INTERPOLATION_MAX_RAW 135
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_0
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_0 20
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_1
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_1 20
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_2
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_2 21
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_3
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_3 21
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_4
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_4 22
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_5
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_5 22
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_0
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_0 21
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_1
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_1 22
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_2
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_2 23
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_3
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_3 24
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_4
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_4 25
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_5
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_5 25
#endif

#include "display-brightness-policy.h"

#if POWER_SENTINEL_ALS_EMA_ALPHA_DEN == 0
#error "POWER_SENTINEL_ALS_EMA_ALPHA_DEN must be greater than 0"
#endif
#if POWER_SENTINEL_ALS_EMA_ALPHA_NUM > POWER_SENTINEL_ALS_EMA_ALPHA_DEN
#error "POWER_SENTINEL_ALS_EMA_ALPHA_NUM must be <= POWER_SENTINEL_ALS_EMA_ALPHA_DEN"
#endif

#if POWER_SENTINEL_DISPLAY_STANDBY_MS < 60000UL
#warning "POWER_SENTINEL_DISPLAY_STANDBY_MS is under 60s; check local power_sentinel_config.h/build flags if this is not an accelerated test build."
#endif
#if POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS < 60000UL
#warning "POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS is under 60s; missing payload can turn the display off quickly."
#endif
#if POWER_SENTINEL_DISPLAY_DIM_BRIGHTNESS == 0
#warning "POWER_SENTINEL_DISPLAY_DIM_BRIGHTNESS is 0; DIM will look like OFF."
#endif

namespace {
constexpr uint16_t kScreenW = 320;
constexpr uint16_t kScreenH = 240;
constexpr uint32_t kSummaryPollMs = SUMMARY_POLL_MS;
constexpr uint32_t kDisplayWakeCooldownMs = 1000;
constexpr uint32_t kLedcardsSleepLongPressMs = 3000;
constexpr uint32_t kDisplayStandbyMs = POWER_SENTINEL_DISPLAY_STANDBY_MS;
constexpr uint32_t kDisplayNoPayloadOffMs = POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS;
constexpr uint8_t kDisplayAwakeBrightness = POWER_SENTINEL_DISPLAY_AWAKE_BRIGHTNESS;
constexpr uint8_t kDisplayDimBrightness = POWER_SENTINEL_DISPLAY_DIM_BRIGHTNESS;
constexpr uint32_t kDisplayFadeMs = POWER_SENTINEL_DISPLAY_FADE_MS;
constexpr uint32_t kDisplaySnoozeMs = POWER_SENTINEL_DISPLAY_SNOOZE_MS;
constexpr uint32_t kLinkStatusTimeoutMs = 10000;
constexpr uint32_t kAlsPollMs = POWER_SENTINEL_ALS_POLL_MS;
constexpr uint8_t kAlsWarmupSamples = POWER_SENTINEL_ALS_WARMUP_SAMPLES;
constexpr uint8_t kAlsBrightnessSlewStep = POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_STEP;
constexpr uint32_t kAlsBrightnessSlewHighMs = POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_HIGH_MS;
constexpr uint32_t kAlsBrightnessSlewMidHighMs = POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_MID_HIGH_MS;
constexpr uint32_t kAlsBrightnessSlewMidLowMs = POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_MID_LOW_MS;
constexpr uint32_t kAlsBrightnessSlewLowMs = POWER_SENTINEL_ALS_BRIGHTNESS_SLEW_LOW_MS;
constexpr uint32_t kAlsBrighteningDebounceMs = POWER_SENTINEL_ALS_BRIGHTENING_DEBOUNCE_MS;
constexpr uint32_t kAlsDarkeningDebounceMs = POWER_SENTINEL_ALS_DARKENING_DEBOUNCE_MS;
constexpr uint8_t kAlsTargetFilterShift = POWER_SENTINEL_ALS_TARGET_FILTER_SHIFT;
constexpr uint8_t kAlsTargetDeadband = POWER_SENTINEL_ALS_TARGET_DEADBAND;

struct UpsState {
  bool available = false;
  bool onBattery = false;
  bool lowBattery = false;
  bool charging = false;
  bool stale = true;
  char status[24] = "unknown";
  int batteryPercent = -1;
  int runtimeSeconds = -1;
  int loadPercent = -1;
  float inputVoltage = 0.0f;
  int ageSeconds = -1;
};

struct NutState {
  int clientCount = -1;
  bool wouldShutdown = false;
};

struct ProxmoxState {
  bool enabled = false;
  bool implemented = false;
  bool hasLiveData = false;
  char condition[16] = "unavailable";
  char status[24] = "not_observed";
  char signalKind[24] = "";
  char signalSummary[48] = "";
  char signalContext[32] = "";
  int nodeCount = -1;
  int onlineNodeCount = -1;
  int guestTotalCount = -1;
  int guestRunningCount = -1;
  int storageCount = -1;
  int maxStorageUsedPercent = -1;
  int cpuPercent = -1;
  char cpuCondition[16] = "healthy";
  int ramPercent = -1;
  char ramCondition[16] = "healthy";
  int guestRunning = -1;
  int guestTotal = -1;
  char guestCondition[16] = "healthy";
  int storagePercent = -1;
  char storageCondition[16] = "healthy";
  int networkPercent = -1;
  char networkCondition[16] = "healthy";
};

struct SummaryState {
  char schema[32] = "power-sentinel.summary.v1";
  char severity[12] = "unknown";
  char source[16] = "boot";
  char transportStatus[96] = "Waiting for StackFlow summary";
  bool moduleLanConnected = false;
  char moduleTimeHhmm[6] = "--:--";
  bool offline = true;
  uint32_t lastGoodMillis = 0;
  UpsState ups;
  NutState nut;
  ProxmoxState proxmox;
};

enum class DisplayMode : uint8_t { Awake, Dim, Off };

SummaryState state;
uint32_t lastFetchMs = 0;
uint32_t lastLvTickMs = 0;
uint32_t stackflowRequestId = 0;
uint32_t fetchAttemptCount = 0;
uint32_t fetchOkCount = 0;
uint32_t fetchFailCount = 0;
CoreS3TransportDiagnostics transportDiagnostics{};
CoreS3TransportFailureKind pendingTransportFailureKind = CORE_S3_TRANSPORT_FAILURE_TIMEOUT;
bool displayAsleep = false;
bool displaySleepWakeArmed = false;
bool ledcardsSleepTouchActive = false;
bool ledcardsSleepLongPressFired = false;
bool standbyEligible = false;
bool stateSignatureValid = false;
bool manualDisplaySnoozeActive = false;
bool missingPayloadDisplayOffActive = false;
bool displayFadeActive = false;
bool linkStatusRenderedValid = false;
bool lastRenderedLinkOk = false;
uint8_t currentPageIndex = 0;
bool topBarPageTouchActive = false;
DisplayMode displayMode = DisplayMode::Awake;
DisplayMode displayFadeTargetMode = DisplayMode::Awake;
uint8_t displayBrightnessCurrent = kDisplayAwakeBrightness;
uint8_t displayBrightnessStart = kDisplayAwakeBrightness;
uint8_t displayBrightnessTarget = kDisplayAwakeBrightness;
uint32_t displayFadeStartMs = 0;
uint32_t displaySleepEnteredMs = 0;
uint32_t displayDimEnteredMs = 0;
bool alsSensorReady = false;
bool alsEmaValid = false;
bool alsAdaptiveReady = false;
uint16_t alsRawCurrent = 0;
uint16_t alsEmaCurrent = 0;
uint8_t alsValidSamples = 0;
bool alsBrightnessFilterValid = false;
DisplayMode alsBrightnessFilterMode = DisplayMode::Awake;
int32_t alsBrightnessAcceptedTargetQ8 = static_cast<int32_t>(kDisplayAwakeBrightness) << 8;
int32_t alsBrightnessFilteredQ8 = static_cast<int32_t>(kDisplayAwakeBrightness) << 8;
int32_t alsBrightnessPendingTargetQ8 = static_cast<int32_t>(kDisplayAwakeBrightness) << 8;
uint32_t lastAlsPollMs = 0;
uint32_t lastAlsTargetFilterMs = 0;
uint32_t lastAlsBrightnessSlewMs = 0;
uint32_t alsBrightnessPendingSinceMs = 0;
uint32_t lastDisplayActivityMs = 0;
uint32_t manualDisplaySnoozeUntilMs = 0;
uint32_t ledcardsSleepPressStartMs = 0;
char lastStateSignature[192] = "";
char snoozedStateSignature[192] = "";

lv_color_t buf1[kScreenW * 24];
lv_color_t buf2[kScreenW * 24];
lv_display_t *display = nullptr;
lv_indev_t *indev = nullptr;

void safeCopy(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

int jsonInt(JsonVariantConst v, int fallback) {
  if (v.isNull()) return fallback;
  if (v.is<int>()) return v.as<int>();
  if (v.is<float>()) return static_cast<int>(v.as<float>());
  if (v.is<const char *>()) return atoi(v.as<const char *>());
  return fallback;
}

float jsonFloat(JsonVariantConst v, float fallback) {
  if (v.isNull()) return fallback;
  if (v.is<float>() || v.is<int>()) return v.as<float>();
  if (v.is<const char *>()) return atof(v.as<const char *>());
  return fallback;
}

bool jsonBool(JsonVariantConst v, bool fallback = false) {
  if (v.isNull()) return fallback;
  if (v.is<bool>()) return v.as<bool>();
  if (v.is<int>()) return v.as<int>() != 0;
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    return strcmp(s, "1") == 0 || strcmp(s, "true") == 0 || strcmp(s, "yes") == 0;
  }
  return fallback;
}

bool validHhmm(const char *s) {
  if (!s) return false;
  return s[0] >= '0' && s[0] <= '2' &&
         s[1] >= '0' && s[1] <= '9' &&
         s[2] == ':' &&
         s[3] >= '0' && s[3] <= '5' &&
         s[4] >= '0' && s[4] <= '9' &&
         s[5] == '\0' &&
         !(s[0] == '2' && s[1] > '3');
}

bool linkOkAt(uint32_t now) {
  return state.lastGoodMillis != 0 && now - state.lastGoodMillis <= kLinkStatusTimeoutMs;
}

bool proxmoxPageAvailable() {
  return state.proxmox.enabled && state.proxmox.implemented;
}

uint8_t ambientPageCount() {
  return proxmoxPageAvailable() ? 2 : 1;
}

void clampCurrentPageIndex() {
  uint8_t pageCount = ambientPageCount();
  if (currentPageIndex >= pageCount) currentPageIndex = 0;
}

bool parseSummary(const String &json, const char *source) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("NUT summary JSON parse error: %s\n", err.c_str());
    snprintf(state.transportStatus, sizeof(state.transportStatus), "JSON parse failed: %s", err.c_str());
    pendingTransportFailureKind = CORE_S3_TRANSPORT_FAILURE_JSON_PARSE;
    return false;
  }
  safeCopy(state.schema, sizeof(state.schema), doc["schema"] | "power-sentinel.summary.v1");
  safeCopy(state.severity, sizeof(state.severity), doc["severity"] | "unknown");
  safeCopy(state.source, sizeof(state.source), source);
  JsonObjectConst ups = doc["ups"].as<JsonObjectConst>();
  state.ups.available = jsonBool(ups["available"], false);
  state.ups.stale = jsonBool(ups["stale"], !state.ups.available);
  state.ups.onBattery = jsonBool(ups["on_battery"], false);
  state.ups.lowBattery = jsonBool(ups["low_battery"], false);
  state.ups.charging = jsonBool(ups["charging"], false);
  safeCopy(state.ups.status, sizeof(state.ups.status), ups["status"] | ups["status_label"] | "unknown");
  state.ups.batteryPercent = jsonInt(ups["battery_percent"], -1);
  state.ups.runtimeSeconds = jsonInt(ups["runtime_seconds"], -1);
  state.ups.loadPercent = jsonInt(ups["load_percent"], -1);
  state.ups.inputVoltage = jsonFloat(ups["input_voltage"], 0.0f);
  state.ups.ageSeconds = jsonInt(ups["age_seconds"], -1);
  JsonObjectConst nut = doc["nut"].as<JsonObjectConst>();
  state.nut.clientCount = jsonInt(nut["client_count"], -1);
  state.nut.wouldShutdown = jsonBool(nut["would_shutdown"], false);
  JsonObjectConst module = doc["module"].as<JsonObjectConst>();
  state.moduleLanConnected = jsonBool(module["lan_connected"], false);
  const char *timeHhmm = module["time_hhmm"] | "--:--";
  safeCopy(state.moduleTimeHhmm, sizeof(state.moduleTimeHhmm), validHhmm(timeHhmm) ? timeHhmm : "--:--");
  JsonObjectConst proxmox = doc["modules"]["proxmox"].as<JsonObjectConst>();
  state.proxmox.enabled = jsonBool(proxmox["enabled"], false);
  state.proxmox.implemented = jsonBool(proxmox["implemented"], false);
  safeCopy(state.proxmox.condition, sizeof(state.proxmox.condition), proxmox["condition"] | "unavailable");
  safeCopy(state.proxmox.status, sizeof(state.proxmox.status), proxmox["status"] | "not_observed");
  JsonObjectConst proxmoxCpuCard = proxmox["cards"]["cpu"].as<JsonObjectConst>();
  state.proxmox.cpuPercent = jsonInt(proxmoxCpuCard["value_percent"], -1);
  safeCopy(state.proxmox.cpuCondition, sizeof(state.proxmox.cpuCondition), proxmoxCpuCard["condition"] | "healthy");
  JsonObjectConst proxmoxRamCard = proxmox["cards"]["ram"].as<JsonObjectConst>();
  state.proxmox.ramPercent = jsonInt(proxmoxRamCard["value_percent"], -1);
  safeCopy(state.proxmox.ramCondition, sizeof(state.proxmox.ramCondition), proxmoxRamCard["condition"] | "healthy");
  JsonObjectConst proxmoxGuestCard = proxmox["cards"]["guests"].as<JsonObjectConst>();
  state.proxmox.guestRunning = jsonInt(proxmoxGuestCard["running"], -1);
  state.proxmox.guestTotal = jsonInt(proxmoxGuestCard["total"], -1);
  safeCopy(state.proxmox.guestCondition, sizeof(state.proxmox.guestCondition), proxmoxGuestCard["condition"] | "healthy");
  JsonObjectConst proxmoxStorageCard = proxmox["cards"]["storage"].as<JsonObjectConst>();
  state.proxmox.storagePercent = jsonInt(proxmoxStorageCard["value_percent"], -1);
  safeCopy(state.proxmox.storageCondition, sizeof(state.proxmox.storageCondition), proxmoxStorageCard["condition"] | "healthy");
  JsonObjectConst proxmoxNetworkCard = proxmox["cards"]["network"].as<JsonObjectConst>();
  state.proxmox.networkPercent = jsonInt(proxmoxNetworkCard["value_percent"], -1);
  safeCopy(state.proxmox.networkCondition, sizeof(state.proxmox.networkCondition), proxmoxNetworkCard["condition"] | "healthy");
  JsonObjectConst proxmoxEnvironment = proxmox["environment"].as<JsonObjectConst>();
  state.proxmox.nodeCount = jsonInt(proxmoxEnvironment["node_count"], -1);
  state.proxmox.onlineNodeCount = jsonInt(proxmoxEnvironment["online_node_count"], -1);
  state.proxmox.guestTotalCount = jsonInt(proxmoxEnvironment["guest_total_count"], -1);
  state.proxmox.guestRunningCount = jsonInt(proxmoxEnvironment["guest_running_count"], -1);
  state.proxmox.storageCount = jsonInt(proxmoxEnvironment["storage_count"], -1);
  state.proxmox.maxStorageUsedPercent = -1;
  JsonArrayConst proxmoxStorage = proxmox["storage"].as<JsonArrayConst>();
  for (JsonObjectConst item : proxmoxStorage) {
    int used = jsonInt(item["used_percent"], -1);
    if (used > state.proxmox.maxStorageUsedPercent) state.proxmox.maxStorageUsedPercent = used;
  }
  JsonArrayConst proxmoxSignals = proxmox["signals"].as<JsonArrayConst>();
  JsonObjectConst selectedProxmoxSignal;
  for (JsonObjectConst signal : proxmoxSignals) {
    if (selectedProxmoxSignal.isNull()) {
      selectedProxmoxSignal = signal;
      continue;
    }
    const char *selectedKind = selectedProxmoxSignal["kind"] | "";
    const char *kind = signal["kind"] | "";
    if (strcmp(selectedKind, "storage_warning") == 0 && strcmp(kind, "storage_critical") == 0) {
      selectedProxmoxSignal = signal;
    }
  }
  safeCopy(state.proxmox.signalKind, sizeof(state.proxmox.signalKind), selectedProxmoxSignal["kind"] | "");
  safeCopy(state.proxmox.signalSummary, sizeof(state.proxmox.signalSummary), selectedProxmoxSignal["summary"] | "");
  JsonObjectConst selectedProxmoxSignalContext = selectedProxmoxSignal["context"].as<JsonObjectConst>();
  const char *signalNode = selectedProxmoxSignalContext["node"] | "";
  const char *signalStorage = selectedProxmoxSignalContext["storage"] | "";
  const char *signalName = selectedProxmoxSignalContext["name"] | "";
  int signalUsedPercent = jsonInt(selectedProxmoxSignalContext["used_percent"], -1);
  int signalVmid = jsonInt(selectedProxmoxSignalContext["vmid"], -1);
  if (signalNode[0] != '\0') {
    if (signalStorage[0] != '\0' && signalUsedPercent >= 0) {
      snprintf(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "%s/%s %d%%", signalNode, signalStorage, signalUsedPercent);
    } else {
      safeCopy(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), signalNode);
    }
  } else if (signalStorage[0] != '\0' && signalUsedPercent >= 0) {
    snprintf(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "%s %d%%", signalStorage, signalUsedPercent);
  } else if (signalName[0] != '\0' && signalVmid >= 0) {
    snprintf(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "%s %d", signalName, signalVmid);
  } else if (signalVmid >= 0) {
    snprintf(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "VMID %d", signalVmid);
  } else {
    safeCopy(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "");
  }
  state.proxmox.hasLiveData = state.proxmox.enabled &&
                              state.proxmox.implemented &&
                              strcmp(state.proxmox.status, "observed") == 0 &&
                              state.proxmox.nodeCount >= 0;
  state.offline = false;
  state.lastGoodMillis = millis();
  snprintf(state.transportStatus, sizeof(state.transportStatus), "%s summary OK", source);
  return true;
}

#if POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY
static const char kLedcardsInterfaceVisualFixtureJson[] = R"JSON({
  "schema": "power-sentinel.summary.v1",
  "severity": "ok",
  "module": {"lan_connected": true, "time_hhmm": "14:32"},
  "ups": {
    "available": true,
    "stale": false,
    "status": "OL CHRG",
    "battery_percent": 96,
    "runtime_seconds": 5400,
    "load_percent": 18,
    "input_voltage": 232.1,
    "age_seconds": 3
  },
  "nut": {"client_count": 2, "would_shutdown": false},
  "modules": {
    "proxmox": {
      "enabled": true,
      "implemented": true,
      "condition": "warning",
      "status": "observed",
      "environment": {
        "node_count": 1,
        "online_node_count": 1,
        "guest_running_count": 5,
        "guest_total_count": 6,
        "storage_count": 3
      },
      "cards": {
        "cpu": {"value_percent": 38, "condition": "healthy"},
        "ram": {"value_percent": 64, "condition": "healthy"},
        "guests": {"running": 5, "total": 6, "condition": "warning"},
        "storage": {"value_percent": 72, "condition": "healthy"},
        "network": {"value_percent": 41, "condition": "healthy"}
      },
      "signals": [
        {"kind": "guest_down", "condition": "warning", "summary": "Proxmox guests running 5/6", "context": {"running": 5, "total": 6, "stopped": 1}}
      ]
    }
  }
})JSON";

void applyLedcardsInterfaceVisualFixture() {
  parseSummary(kLedcardsInterfaceVisualFixtureJson, "fixture");
}
#endif

LedcardsInterfaceNutView makeLedcardsInterfaceNutView() {
  LedcardsInterfaceNutView view{};
  view.offline = state.offline;
  view.upsAvailable = state.ups.available;
  view.upsStale = state.ups.stale;
  view.onBattery = state.ups.onBattery;
  view.lowBattery = state.ups.lowBattery;
  view.charging = state.ups.charging;
  view.batteryPercent = state.ups.batteryPercent;
  view.runtimeSeconds = state.ups.runtimeSeconds;
  view.loadPercent = state.ups.loadPercent;
  view.inputVoltage = state.ups.inputVoltage;
  view.nutClientCount = state.nut.clientCount;
  view.ageSeconds = state.ups.ageSeconds;
  uint32_t now = millis();
  view.moduleLanConnected = state.moduleLanConnected;
  view.wifiConnected = WiFi.status() == WL_CONNECTED;
  view.linkOk = linkOkAt(now);
  safeCopy(view.transportStatus, sizeof(view.transportStatus), state.transportStatus);
  safeCopy(view.moduleTimeHhmm, sizeof(view.moduleTimeHhmm), validHhmm(state.moduleTimeHhmm) ? state.moduleTimeHhmm : "--:--");
  view.localBatteryPercent = psBatteryLevel();
  view.localBatteryCharging = psBatteryCharging();
  view.localBatteryKnown = view.localBatteryPercent >= 0;
  clampCurrentPageIndex();
  view.pageCount = proxmoxPageAvailable() ? 2 : 1;
  view.pageIndex = currentPageIndex;
  view.nowMillis = now;
  return view;
}

ProxmoxAmbientView makeProxmoxAmbientView() {
  ProxmoxAmbientView view{};
  view.enabled = state.proxmox.enabled;
  view.implemented = state.proxmox.implemented;
  view.hasLiveData = state.proxmox.hasLiveData;
  safeCopy(view.condition, sizeof(view.condition), state.proxmox.condition);
  safeCopy(view.status, sizeof(view.status), state.proxmox.status);
  view.cpuPercent = state.proxmox.cpuPercent;
  safeCopy(view.cpuCondition, sizeof(view.cpuCondition), state.proxmox.cpuCondition);
  view.ramPercent = state.proxmox.ramPercent;
  safeCopy(view.ramCondition, sizeof(view.ramCondition), state.proxmox.ramCondition);
  view.guestRunning = state.proxmox.guestRunning;
  view.guestTotal = state.proxmox.guestTotal;
  safeCopy(view.guestCondition, sizeof(view.guestCondition), state.proxmox.guestCondition);
  view.storagePercent = state.proxmox.storagePercent;
  safeCopy(view.storageCondition, sizeof(view.storageCondition), state.proxmox.storageCondition);
  view.networkPercent = state.proxmox.networkPercent;
  safeCopy(view.networkCondition, sizeof(view.networkCondition), state.proxmox.networkCondition);
  return view;
}

void myDispFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap) {
  uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  psDisplayWritePixels(area->x1, area->y1, w, h, reinterpret_cast<uint16_t *>(pxMap), true);
  lv_display_flush_ready(disp);
}

bool isCriticalSeverity() {
  return strcmp(state.severity, "critical") == 0;
}

bool isAlertSeverity() {
  return strcmp(state.severity, "warn") == 0 || isCriticalSeverity();
}

void startDisplayFade(DisplayMode targetMode, uint8_t targetBrightness, uint32_t now);
void applyDisplayBrightness(uint8_t brightness);

void buildStateSignature(char *dst, size_t dstSize) {
  snprintf(dst, dstSize, "%s|a%d|s%d|ob%d|lb%d|ch%d|st:%s|clients:%d",
           state.severity,
           state.ups.available ? 1 : 0,
           state.ups.stale ? 1 : 0,
           state.ups.onBattery ? 1 : 0,
           state.ups.lowBattery ? 1 : 0,
           state.ups.charging ? 1 : 0,
           state.ups.status,
           state.nut.clientCount);
}

bool adaptiveBrightnessActive() {
#if POWER_SENTINEL_ADAPTIVE_BRIGHTNESS_ENABLED
  return alsSensorReady && alsAdaptiveReady;
#else
  return false;
#endif
}

uint8_t staticBrightnessForMode(DisplayMode mode) {
  if (mode == DisplayMode::Dim) return kDisplayDimBrightness;
  if (mode == DisplayMode::Awake) return kDisplayAwakeBrightness;
  return 0;
}

uint8_t adaptiveBrightnessForMode(DisplayMode mode) {
  uint16_t raw = alsEmaValid ? alsEmaCurrent : alsRawCurrent;
  if (mode == DisplayMode::Dim) return psBrightnessForRaw(raw, PsBrightnessMode::Dim);
  if (mode == DisplayMode::Awake) return psBrightnessForRaw(raw, PsBrightnessMode::Awake);
  return 0;
}

bool adaptiveBrightnessIsEndpointTarget(DisplayMode mode, uint8_t target) {
  uint8_t level = psCoreS3BacklightLevelForBrightness(target);
  if (mode == DisplayMode::Dim) return level <= psMinBacklightLevelForMode(PsBrightnessMode::Dim) || level >= psMaxBacklightLevelForMode(PsBrightnessMode::Dim);
  if (mode == DisplayMode::Awake) return level <= psMinBacklightLevelForMode(PsBrightnessMode::Awake) || level >= psMaxBacklightLevelForMode(PsBrightnessMode::Awake);
  return target == 0;
}

uint32_t adaptiveBrightnessSlewIntervalMs(uint8_t current, uint8_t target) {
  uint8_t lower = current < target ? current : target;
  if (lower < 48) return kAlsBrightnessSlewLowMs;
  if (lower < 64) return kAlsBrightnessSlewMidLowMs;
  if (lower < 80) return kAlsBrightnessSlewMidHighMs;
  return kAlsBrightnessSlewHighMs;
}

uint8_t filteredAdaptiveBrightnessForMode(DisplayMode mode) {
  if (!alsBrightnessFilterValid || alsBrightnessFilterMode != mode) return adaptiveBrightnessForMode(mode);
  int32_t value = (alsBrightnessFilteredQ8 + 128) >> 8;
  if (value < 0) return 0;
  if (value > 255) return 255;
  return static_cast<uint8_t>(value);
}

uint8_t brightnessForMode(DisplayMode mode) {
  if (!adaptiveBrightnessActive()) return staticBrightnessForMode(mode);
  return filteredAdaptiveBrightnessForMode(mode);
}

DisplayMode currentDisplayBrightnessMode() {
  return displayFadeActive ? displayFadeTargetMode : displayMode;
}

void updateAmbientLight(uint32_t now) {
#if POWER_SENTINEL_ALS_SENSOR_ENABLED
  if (!alsSensorReady || kAlsPollMs == 0) return;
  if (lastAlsPollMs != 0 && now - lastAlsPollMs < kAlsPollMs) return;
  lastAlsPollMs = now;

  uint16_t raw = 0;
  if (!psAmbientLightRead(&raw)) return;
  alsRawCurrent = raw;
  if (!alsEmaValid) {
    alsEmaCurrent = raw;
    alsEmaValid = true;
  } else {
    uint32_t weighted = static_cast<uint32_t>(alsEmaCurrent) * (POWER_SENTINEL_ALS_EMA_ALPHA_DEN - POWER_SENTINEL_ALS_EMA_ALPHA_NUM) +
                        static_cast<uint32_t>(raw) * POWER_SENTINEL_ALS_EMA_ALPHA_NUM +
                        (POWER_SENTINEL_ALS_EMA_ALPHA_DEN / 2);
    alsEmaCurrent = static_cast<uint16_t>(weighted / POWER_SENTINEL_ALS_EMA_ALPHA_DEN);
  }
  if (alsValidSamples < 255) ++alsValidSamples;
  if (!alsAdaptiveReady) {
    alsAdaptiveReady = alsValidSamples >= kAlsWarmupSamples;
  }

#if POWER_SENTINEL_ALS_DEBUG_SERIAL
  Serial.printf("ALS raw=%u ema=%u adaptive=%u mode=%u fade=%u instant=%u filtered=%u current=%u\n",
                static_cast<unsigned>(alsRawCurrent),
                static_cast<unsigned>(alsEmaCurrent),
                adaptiveBrightnessActive() ? 1U : 0U,
                static_cast<unsigned>(currentDisplayBrightnessMode()),
                displayFadeActive ? 1U : 0U,
                static_cast<unsigned>(adaptiveBrightnessForMode(currentDisplayBrightnessMode())),
                static_cast<unsigned>(brightnessForMode(currentDisplayBrightnessMode())),
                static_cast<unsigned>(displayBrightnessCurrent));
#endif
#else
  (void)now;
#endif
}

void updateAdaptiveBrightnessTargetFilter(uint32_t now) {
  if (!adaptiveBrightnessActive()) {
    alsBrightnessFilterValid = false;
    alsBrightnessPendingSinceMs = 0;
    return;
  }
  DisplayMode mode = currentDisplayBrightnessMode();
  if (mode == DisplayMode::Off) {
    alsBrightnessFilterValid = false;
    alsBrightnessPendingSinceMs = 0;
    return;
  }

  if (lastAlsTargetFilterMs != 0 && now - lastAlsTargetFilterMs < kAlsPollMs) return;
  lastAlsTargetFilterMs = now;

  uint8_t instantTarget = adaptiveBrightnessForMode(mode);
  int32_t targetQ8 = static_cast<int32_t>(instantTarget) << 8;
  if (!alsBrightnessFilterValid || alsBrightnessFilterMode != mode) {
    alsBrightnessAcceptedTargetQ8 = targetQ8;
    alsBrightnessFilteredQ8 = targetQ8;
    alsBrightnessPendingTargetQ8 = targetQ8;
    alsBrightnessPendingSinceMs = 0;
    alsBrightnessFilterMode = mode;
    alsBrightnessFilterValid = true;
    return;
  }

  int32_t deadbandQ8 = static_cast<int32_t>(kAlsTargetDeadband) << 8;
  int32_t targetDelta = targetQ8 - alsBrightnessAcceptedTargetQ8;
  bool significantTarget = targetDelta != 0 &&
                           (adaptiveBrightnessIsEndpointTarget(mode, instantTarget) ||
                            targetDelta < -deadbandQ8 || targetDelta > deadbandQ8);
  if (!significantTarget) {
    alsBrightnessPendingSinceMs = 0;
  } else {
    uint32_t debounceMs = targetDelta > 0 ? kAlsBrighteningDebounceMs : kAlsDarkeningDebounceMs;
    bool pendingDirectionChanged =
        alsBrightnessPendingSinceMs == 0 ||
        (targetQ8 > alsBrightnessAcceptedTargetQ8) !=
            (alsBrightnessPendingTargetQ8 > alsBrightnessAcceptedTargetQ8);
    if (debounceMs == 0) {
      alsBrightnessAcceptedTargetQ8 = targetQ8;
      alsBrightnessPendingSinceMs = 0;
    } else if (pendingDirectionChanged) {
      alsBrightnessPendingTargetQ8 = targetQ8;
      alsBrightnessPendingSinceMs = now;
    } else {
      alsBrightnessPendingTargetQ8 = targetQ8;
      if (now - alsBrightnessPendingSinceMs >= debounceMs) {
        alsBrightnessAcceptedTargetQ8 = alsBrightnessPendingTargetQ8;
        alsBrightnessPendingSinceMs = 0;
      }
    }
  }

  int32_t delta = alsBrightnessAcceptedTargetQ8 - alsBrightnessFilteredQ8;
  if (delta == 0) return;
  if (kAlsTargetFilterShift == 0) {
    alsBrightnessFilteredQ8 = alsBrightnessAcceptedTargetQ8;
    return;
  }
  int32_t step = delta >> kAlsTargetFilterShift;
  if (step == 0) step = delta > 0 ? 1 : -1;
  alsBrightnessFilteredQ8 += step;
}

void updateAdaptiveBrightnessSlew(uint32_t now) {
  if (!adaptiveBrightnessActive() || displayFadeActive) return;
  DisplayMode mode = currentDisplayBrightnessMode();
  if (mode == DisplayMode::Off) return;

  uint8_t target = brightnessForMode(mode);
  if (displayBrightnessCurrent == target) return;

  uint32_t intervalMs = adaptiveBrightnessSlewIntervalMs(displayBrightnessCurrent, target);
  if (intervalMs == 0) return;
  if (lastAlsBrightnessSlewMs != 0 && now - lastAlsBrightnessSlewMs < intervalMs) return;
  lastAlsBrightnessSlewMs = now;

  uint8_t currentLevel = psCoreS3BacklightLevelForBrightness(displayBrightnessCurrent);
  uint8_t targetLevel = psCoreS3BacklightLevelForBrightness(target);
  if (currentLevel == targetLevel) {
    applyDisplayBrightness(target);
    return;
  }

  uint8_t levelStep = kAlsBrightnessSlewStep == 0 ? 1 : kAlsBrightnessSlewStep;
  uint8_t nextLevel = currentLevel;
  if (currentLevel < targetLevel) {
    uint16_t raised = static_cast<uint16_t>(currentLevel) + levelStep;
    nextLevel = raised >= targetLevel ? targetLevel : static_cast<uint8_t>(raised);
  } else {
    nextLevel = currentLevel - targetLevel <= levelStep ? targetLevel : static_cast<uint8_t>(currentLevel - levelStep);
  }
  uint8_t next = nextLevel == targetLevel ? target : psBrightnessForCoreS3BacklightLevel(nextLevel);
  applyDisplayBrightness(next);
}

void applyDisplayBrightness(uint8_t brightness) {
  displayBrightnessCurrent = brightness;
  psDisplaySetBrightness(brightness);
}

void startDisplayFade(DisplayMode targetMode, uint8_t targetBrightness, uint32_t now) {
  displayFadeActive = true;
  displayFadeTargetMode = targetMode;
  displayBrightnessStart = displayBrightnessCurrent;
  displayBrightnessTarget = targetBrightness;
  displayFadeStartMs = now;
  if (targetMode != DisplayMode::Off) displayAsleep = false;
  if (targetMode == DisplayMode::Dim) displayDimEnteredMs = now;
}

void finishDisplayFade() {
  displayFadeActive = false;
  applyDisplayBrightness(displayBrightnessTarget);
  displayMode = displayFadeTargetMode;
  displayAsleep = displayMode == DisplayMode::Off;
  if (displayAsleep) {
    displaySleepEnteredMs = millis();
    displaySleepWakeArmed = false;
  }
}

void updateDisplayFade(uint32_t now) {
  if (!displayFadeActive) return;
  uint32_t elapsed = now - displayFadeStartMs;
  if (elapsed >= kDisplayFadeMs || kDisplayFadeMs == 0) {
    finishDisplayFade();
    return;
  }
  int32_t delta = static_cast<int32_t>(displayBrightnessTarget) - static_cast<int32_t>(displayBrightnessStart);
  uint8_t next = static_cast<uint8_t>(static_cast<int32_t>(displayBrightnessStart) + (delta * static_cast<int32_t>(elapsed)) / static_cast<int32_t>(kDisplayFadeMs));
  if (next != displayBrightnessCurrent) applyDisplayBrightness(next);
}

void refreshLedcardsUi() {
  LedcardsInterfaceNutView view = makeLedcardsInterfaceNutView();
  if (currentPageIndex == 1 && proxmoxPageAvailable()) {
    renderProxmoxAmbientUi(makeProxmoxAmbientView(), view);
  } else {
    updateLedcardsInterfaceUi(view);
  }
  lastRenderedLinkOk = view.linkOk;
  linkStatusRenderedValid = true;
}

bool handleTopBarPageTap(int32_t x, int32_t y) {
  uint8_t pageCount = ambientPageCount();
  if (pageCount <= 1 || y > 24) return false;
  if (!topBarPageTouchActive) {
    uint8_t previousPageIndex = currentPageIndex;
    uint8_t targetPageIndex = x < 160 ? 0 : 1;
    currentPageIndex = targetPageIndex;
    clampCurrentPageIndex();
    LedcardsInterfaceNutView view = makeLedcardsInterfaceNutView();
    if (!transitionLedcardsInterfacePageUi(previousPageIndex, currentPageIndex, view, makeProxmoxAmbientView())) {
      refreshLedcardsUi();
    }
    topBarPageTouchActive = true;
  }
  return true;
}

void enterDisplayDim(uint32_t now) {
  if ((displayMode == DisplayMode::Dim && !displayFadeActive) ||
      (displayFadeActive && displayFadeTargetMode == DisplayMode::Dim)) return;
  if (displayMode == DisplayMode::Off) refreshLedcardsUi();
  startDisplayFade(DisplayMode::Dim, brightnessForMode(DisplayMode::Dim), now);
}

void enterDisplaySleep() {
  uint32_t now = millis();
  if ((displayMode == DisplayMode::Off && !displayFadeActive) ||
      (displayFadeActive && displayFadeTargetMode == DisplayMode::Off)) return;
  startDisplayFade(DisplayMode::Off, 0, now);
  // Static guard marker: off is implemented via hardware brightness only.
  if (false) psDisplaySetBrightness(0);
}

void wakeDisplay() {
  uint32_t now = millis();
  if ((displayMode == DisplayMode::Awake && !displayFadeActive) ||
      (displayFadeActive && displayFadeTargetMode == DisplayMode::Awake)) return;
  if (displayMode == DisplayMode::Off) refreshLedcardsUi();
  displayAsleep = false;
  displaySleepWakeArmed = true;
  startDisplayFade(DisplayMode::Awake, brightnessForMode(DisplayMode::Awake), now);
}

void recordDisplayActivity(uint32_t now) {
  lastDisplayActivityMs = now;
  if (manualDisplaySnoozeActive) manualDisplaySnoozeActive = false;
}

void enterManualDisplaySnooze(uint32_t now) {
  buildStateSignature(snoozedStateSignature, sizeof(snoozedStateSignature));
  manualDisplaySnoozeActive = true;
  manualDisplaySnoozeUntilMs = now + kDisplaySnoozeMs;
  enterDisplaySleep();
}

bool motionWakeDetected(uint32_t) {
#if POWER_SENTINEL_MOTION_WAKE
  // Reserved for future CoreS3 IMU tap/knock wake calibration. This is not a PIR/presence sensor.
  return false;
#else
  return false;
#endif
}

void handleStateSignatureAfterFetch(uint32_t now) {
  char signature[sizeof(lastStateSignature)];
  buildStateSignature(signature, sizeof(signature));
  bool changed = !stateSignatureValid || strcmp(signature, lastStateSignature) != 0;
  bool wasMissingPayloadOff = missingPayloadDisplayOffActive;
  missingPayloadDisplayOffActive = false;
  if (!stateSignatureValid) {
    stateSignatureValid = true;
    standbyEligible = true;
    recordDisplayActivity(now);
  } else if (changed) {
    lastDisplayActivityMs = now;
    if (manualDisplaySnoozeActive && strcmp(signature, snoozedStateSignature) != 0) manualDisplaySnoozeActive = false;
  }
  if (changed && isAlertSeverity()) {
    wakeDisplay();
  } else if (wasMissingPayloadOff) {
    enterDisplayDim(now);
  }
  safeCopy(lastStateSignature, sizeof(lastStateSignature), signature);
}

void updateAutoStandby(uint32_t now) {
  uint32_t lastPayloadAgeMs = 0;
  if (state.lastGoodMillis == 0) {
    lastPayloadAgeMs = now;
  } else if (now >= state.lastGoodMillis) {
    lastPayloadAgeMs = now - state.lastGoodMillis;
  }
  if (kDisplayNoPayloadOffMs > 0 && lastPayloadAgeMs >= kDisplayNoPayloadOffMs) {
    missingPayloadDisplayOffActive = true;
    enterDisplaySleep();
    return;
  }

  if (!standbyEligible) return;

  if (manualDisplaySnoozeActive) {
    if ((int32_t)(manualDisplaySnoozeUntilMs - now) > 0) return;
    manualDisplaySnoozeActive = false;
    if (isCriticalSeverity()) {
      enterDisplayDim(now);
      return;
    }
  }

  if (displayMode == DisplayMode::Awake && now - lastDisplayActivityMs >= kDisplayStandbyMs) {
    enterDisplayDim(now);
    return;
  }
  // Automatic standby stops at DIM for all valid-payload states. OFF is reserved
  // for deliberate long-press snooze or a prolonged missing-payload condition.
}

void myTouchRead(lv_indev_t *, lv_indev_data_t *data) {
  int32_t x = 0;
  int32_t y = 0;
  uint32_t now = millis();
  bool pressed = psTouchPressed(&x, &y);

  if (!pressed) topBarPageTouchActive = false;

  if (pressed && (displayMode == DisplayMode::Dim || displayMode == DisplayMode::Off)) {
    recordDisplayActivity(now);
    if (displayMode == DisplayMode::Off && now - displaySleepEnteredMs <= kDisplayWakeCooldownMs) {
      data->state = LV_INDEV_STATE_RELEASED;
      return;
    }
    wakeDisplay();
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (pressed && handleTopBarPageTap(x, y)) {
    recordDisplayActivity(now);
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (displaySleepWakeArmed) {
    data->state = LV_INDEV_STATE_RELEASED;
    if (!pressed) displaySleepWakeArmed = false;
    return;
  }
  if (pressed && ledcardsSleepLongPressFired) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }

  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (pressed) {
    recordDisplayActivity(now);
    data->point.x = x;
    data->point.y = y;
    if (!ledcardsSleepTouchActive) {
      ledcardsSleepTouchActive = true;
      ledcardsSleepLongPressFired = false;
      ledcardsSleepPressStartMs = now;
    } else if (!ledcardsSleepLongPressFired && now - ledcardsSleepPressStartMs >= kLedcardsSleepLongPressMs) {
      ledcardsSleepLongPressFired = true;
      enterManualDisplaySnooze(now);
      data->state = LV_INDEV_STATE_RELEASED;
    }
  } else {
    ledcardsSleepTouchActive = false;
    ledcardsSleepLongPressFired = false;
  }
}

void initSerialTransport() {
#if POWER_SENTINEL_TRANSPORT_SERIAL
  Serial2.begin(POWER_SENTINEL_UART_BAUD, SERIAL_8N1, POWER_SENTINEL_UART_RX_PIN, POWER_SENTINEL_UART_TX_PIN);
#endif
}

void initStatusWiFi() {
  if (strlen(POWER_SENTINEL_WIFI_SSID) == 0) return;
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(POWER_SENTINEL_WIFI_SSID, POWER_SENTINEL_WIFI_PASSWORD);
}

bool readSerialLine(String &line, uint32_t timeoutMs) {
#if POWER_SENTINEL_TRANSPORT_SERIAL
  line = "";
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (Serial2.available()) {
      char c = static_cast<char>(Serial2.read());
      if (c == '\n') return line.length() > 0;
      if (c != '\r' && line.length() < POWER_SENTINEL_SERIAL_MAX_JSON_BYTES) line += c;
    }
    delay(2);
  }
#endif
  return false;
}

bool parseStackFlowSummaryResponse(const String &line, const String &requestId, uint8_t attempt) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) {
    snprintf(state.transportStatus, sizeof(state.transportStatus), "StackFlow JSON fail try %u", static_cast<unsigned>(attempt));
    pendingTransportFailureKind = CORE_S3_TRANSPORT_FAILURE_JSON_PARSE;
    return false;
  }
  const char *rid = doc["request_id"] | "";
  if (strcmp(rid, requestId.c_str()) != 0) {
    pendingTransportFailureKind = CORE_S3_TRANSPORT_FAILURE_STALE_RESPONSE;
    return false;
  }
  JsonObjectConst error = doc["error"].as<JsonObjectConst>();
  int code = error["code"] | 0;
  if (code != 0) {
    snprintf(state.transportStatus, sizeof(state.transportStatus), "StackFlow error %d", code);
    pendingTransportFailureKind = CORE_S3_TRANSPORT_FAILURE_STACKFLOW_ERROR;
    return false;
  }
  String body;
  serializeJson(doc["data"], body);
  return parseSummary(body, "stackflow");
}

bool fetchSerialSummary() {
#if POWER_SENTINEL_TRANSPORT_SERIAL
  pendingTransportFailureKind = CORE_S3_TRANSPORT_FAILURE_TIMEOUT;
  for (uint8_t attempt = 1; attempt <= POWER_SENTINEL_SERIAL_RETRIES; ++attempt) {
    String requestId = "ps-nut-" + String(++stackflowRequestId);
    JsonDocument req;
    req["request_id"] = requestId;
    req["work_id"] = "sentinel";
    req["action"] = "summary";
    serializeJson(req, Serial2);
    Serial2.print('\n');
    Serial2.flush();
    String line;
    while (readSerialLine(line, POWER_SENTINEL_SERIAL_TIMEOUT_MS)) {
      line.trim();
      if (line.length() == 0 || line[0] != '{') continue;
      if (parseStackFlowSummaryResponse(line, requestId, attempt)) return true;
    }
  }
#endif
  return false;
}

void formatTransportStatusFromDiagnostics() {
  coreS3TransportFormatStatus(transportDiagnostics, state.transportStatus, sizeof(state.transportStatus));
}

void logTransportFailureIfNeeded() {
  if (!coreS3TransportShouldLogFailure(transportDiagnostics, 3)) return;
  char status[96]{};
  coreS3TransportFormatStatus(transportDiagnostics, status, sizeof(status));
  Serial.printf("Transport diagnostic: %s attempts=%lu ok=%lu fail=%lu last_good_ms=%lu\n",
                status,
                static_cast<unsigned long>(transportDiagnostics.attemptCount),
                static_cast<unsigned long>(transportDiagnostics.okCount),
                static_cast<unsigned long>(transportDiagnostics.failCount),
                static_cast<unsigned long>(transportDiagnostics.lastGoodMillis));
}

void logTransportSuccessIfNeeded() {
  if (!coreS3TransportShouldLogSuccess(transportDiagnostics)) return;
  char status[96]{};
  coreS3TransportFormatStatus(transportDiagnostics, status, sizeof(status));
  Serial.printf("Transport diagnostic: %s attempts=%lu ok=%lu fail=%lu\n",
                status,
                static_cast<unsigned long>(transportDiagnostics.attemptCount),
                static_cast<unsigned long>(transportDiagnostics.okCount),
                static_cast<unsigned long>(transportDiagnostics.failCount));
}

bool fetchLiveSummary() {
  if (fetchSerialSummary()) {
    coreS3TransportRecordSuccess(transportDiagnostics, CORE_S3_TRANSPORT_STACKFLOW_SERIAL, millis());
    fetchAttemptCount = transportDiagnostics.attemptCount;
    ++fetchOkCount;
    formatTransportStatusFromDiagnostics();
    logTransportSuccessIfNeeded();
    handleStateSignatureAfterFetch(millis());
    return true;
  }
  coreS3TransportRecordFailure(transportDiagnostics, pendingTransportFailureKind, millis(), POWER_SENTINEL_SERIAL_RETRIES);
  fetchAttemptCount = transportDiagnostics.attemptCount;
  ++fetchFailCount;
  formatTransportStatusFromDiagnostics();
  logTransportFailureIfNeeded();
  state.offline = true;
  return false;
}

void initLvgl() {
  lv_init();
  display = lv_display_create(kScreenW, kScreenH);
  lv_display_set_buffers(display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(display, myDispFlush);
  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, myTouchRead);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  psM5Begin(POWER_SENTINEL_STACK_POWER_OUT != 0);
  psDisplaySetRotation(1);
  applyDisplayBrightness(kDisplayAwakeBrightness);
  initLvgl();
#if POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY
  applyLedcardsInterfaceVisualFixture();
#endif
  createLedcardsInterfaceUi(makeLedcardsInterfaceNutView());
  alsSensorReady = psAmbientLightBegin();
  Serial.printf("ALS sensor: %s adaptive=%u poll=%lu ms\n",
                alsSensorReady ? "ready" : "unavailable",
                POWER_SENTINEL_ADAPTIVE_BRIGHTNESS_ENABLED ? 1U : 0U,
                static_cast<unsigned long>(kAlsPollMs));
  initStatusWiFi();
  initSerialTransport();
  Serial.printf("Power Sentinel %s clean NUT monitor baseline\n", POWER_SENTINEL_FIRMWARE_BUILD);
  Serial.printf("Transport: StackFlow serial rx=%u tx=%u baud=%lu retries=%u timeout=%lu max_json=%u\n",
                static_cast<unsigned>(POWER_SENTINEL_UART_RX_PIN),
                static_cast<unsigned>(POWER_SENTINEL_UART_TX_PIN),
                static_cast<unsigned long>(POWER_SENTINEL_UART_BAUD),
                static_cast<unsigned>(POWER_SENTINEL_SERIAL_RETRIES),
                static_cast<unsigned long>(POWER_SENTINEL_SERIAL_TIMEOUT_MS),
                static_cast<unsigned>(POWER_SENTINEL_SERIAL_MAX_JSON_BYTES));
  Serial.printf("Display policy: standby=%lu no_payload_off=%lu awake=%u dim=%u\n",
                static_cast<unsigned long>(kDisplayStandbyMs),
                static_cast<unsigned long>(kDisplayNoPayloadOffMs),
                static_cast<unsigned>(kDisplayAwakeBrightness),
                static_cast<unsigned>(kDisplayDimBrightness));
  Serial.printf("ALS brightness slew: high=%lu mid_high=%lu mid_low=%lu low=%lu step=%u\n",
                static_cast<unsigned long>(kAlsBrightnessSlewHighMs),
                static_cast<unsigned long>(kAlsBrightnessSlewMidHighMs),
                static_cast<unsigned long>(kAlsBrightnessSlewMidLowMs),
                static_cast<unsigned long>(kAlsBrightnessSlewLowMs),
                static_cast<unsigned>(kAlsBrightnessSlewStep));
  Serial.printf("ALS backlight levels: raw=%u,%u,%u,%u,%u,%u dim=%u,%u,%u,%u,%u,%u awake=%u,%u,%u,%u,%u,%u\n",
                static_cast<unsigned>(kPsAlsInterpolationRaw[0]),
                static_cast<unsigned>(kPsAlsInterpolationRaw[1]),
                static_cast<unsigned>(kPsAlsInterpolationRaw[2]),
                static_cast<unsigned>(kPsAlsInterpolationRaw[3]),
                static_cast<unsigned>(kPsAlsInterpolationRaw[4]),
                static_cast<unsigned>(kPsAlsInterpolationRaw[5]),
                static_cast<unsigned>(kPsDimBacklightLevels[0]),
                static_cast<unsigned>(kPsDimBacklightLevels[1]),
                static_cast<unsigned>(kPsDimBacklightLevels[2]),
                static_cast<unsigned>(kPsDimBacklightLevels[3]),
                static_cast<unsigned>(kPsDimBacklightLevels[4]),
                static_cast<unsigned>(kPsDimBacklightLevels[5]),
                static_cast<unsigned>(kPsAwakeBacklightLevels[0]),
                static_cast<unsigned>(kPsAwakeBacklightLevels[1]),
                static_cast<unsigned>(kPsAwakeBacklightLevels[2]),
                static_cast<unsigned>(kPsAwakeBacklightLevels[3]),
                static_cast<unsigned>(kPsAwakeBacklightLevels[4]),
                static_cast<unsigned>(kPsAwakeBacklightLevels[5]));
}

void loop() {
  psM5Update();
  uint32_t now = millis();
  uint32_t delta = now - lastLvTickMs;
  lastLvTickMs = now;
  lv_tick_inc(delta ? delta : 1);
  lv_timer_handler();
  updateDisplayFade(now);
  updateAmbientLight(now);
  updateAdaptiveBrightnessTargetFilter(now);
  updateAdaptiveBrightnessSlew(now);
  if ((displayMode == DisplayMode::Dim || displayMode == DisplayMode::Off) && motionWakeDetected(now)) {
    recordDisplayActivity(now);
    wakeDisplay();
  }
  if (now - lastFetchMs >= kSummaryPollMs || lastFetchMs == 0) {
    lastFetchMs = now;
#if !POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY
    fetchLiveSummary();
#endif
    now = millis();
    if (displayMode != DisplayMode::Off) refreshLedcardsUi();
  }
  if (displayMode != DisplayMode::Off) {
    bool currentLinkOk = linkOkAt(now);
    if (!linkStatusRenderedValid || currentLinkOk != lastRenderedLinkOk) refreshLedcardsUi();
  }
  updateAutoStandby(now);
  delay(5);
}

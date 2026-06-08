#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <lvgl.h>
#include <string.h>

#include "m5_hal.h"
#include "ledcards-interface-page.h"

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
#ifndef POWER_SENTINEL_HTTP_FALLBACK
#define POWER_SENTINEL_HTTP_FALLBACK 1
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

namespace {
constexpr uint16_t kScreenW = 320;
constexpr uint16_t kScreenH = 240;
constexpr uint32_t kSummaryPollMs = SUMMARY_POLL_MS;
constexpr uint32_t kDisplayWakeCooldownMs = 1000;
constexpr uint32_t kLedcardsSleepLongPressMs = 3000;
constexpr uint8_t kDisplayAwakeBrightness = 160;

struct UpsState {
  bool available = false;
  bool onBattery = false;
  bool lowBattery = false;
  bool charging = false;
  bool stale = true;
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

struct SummaryState {
  char schema[32] = "power-sentinel.summary.v1";
  char severity[12] = "unknown";
  char source[16] = "boot";
  char transportStatus[96] = "Waiting for StackFlow summary";
  bool offline = true;
  uint32_t lastGoodMillis = 0;
  UpsState ups;
  NutState nut;
};

SummaryState state;
uint32_t lastFetchMs = 0;
uint32_t lastLvTickMs = 0;
uint32_t stackflowRequestId = 0;
uint32_t fetchAttemptCount = 0;
uint32_t fetchOkCount = 0;
uint32_t fetchFailCount = 0;
bool displayAsleep = false;
bool displaySleepWakeArmed = false;
bool ledcardsSleepTouchActive = false;
bool ledcardsSleepLongPressFired = false;
uint32_t displaySleepEnteredMs = 0;
uint32_t ledcardsSleepPressStartMs = 0;

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

void parseSummary(const String &json, const char *source) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("NUT summary JSON parse error: %s\n", err.c_str());
    snprintf(state.transportStatus, sizeof(state.transportStatus), "JSON parse failed: %s", err.c_str());
    return;
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
  state.ups.batteryPercent = jsonInt(ups["battery_percent"], -1);
  state.ups.runtimeSeconds = jsonInt(ups["runtime_seconds"], -1);
  state.ups.loadPercent = jsonInt(ups["load_percent"], -1);
  state.ups.inputVoltage = jsonFloat(ups["input_voltage"], 0.0f);
  state.ups.ageSeconds = jsonInt(ups["age_seconds"], -1);
  JsonObjectConst nut = doc["nut"].as<JsonObjectConst>();
  state.nut.clientCount = jsonInt(nut["client_count"], -1);
  state.nut.wouldShutdown = jsonBool(nut["would_shutdown"], false);
  state.offline = false;
  state.lastGoodMillis = millis();
  snprintf(state.transportStatus, sizeof(state.transportStatus), "%s summary OK", source);
}

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
  view.nowMillis = millis();
  return view;
}

void myDispFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap) {
  uint32_t w = static_cast<uint32_t>(area->x2 - area->x1 + 1);
  uint32_t h = static_cast<uint32_t>(area->y2 - area->y1 + 1);
  psDisplayWritePixels(area->x1, area->y1, w, h, reinterpret_cast<uint16_t *>(pxMap), true);
  lv_display_flush_ready(disp);
}

void enterDisplaySleep() {
  if (displayAsleep) return;
  psDisplaySetBrightness(0);
  displayAsleep = true;
  displaySleepWakeArmed = false;
  displaySleepEnteredMs = millis();
}

void wakeDisplay() {
  if (!displayAsleep) return;
  psDisplaySetBrightness(kDisplayAwakeBrightness);
  displayAsleep = false;
  displaySleepWakeArmed = true;
}

void myTouchRead(lv_indev_t *, lv_indev_data_t *data) {
  int32_t x = 0;
  int32_t y = 0;
  bool pressed = psTouchPressed(&x, &y);
  if (displayAsleep) {
    if (pressed && millis() - displaySleepEnteredMs > kDisplayWakeCooldownMs) wakeDisplay();
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  if (displaySleepWakeArmed) {
    data->state = LV_INDEV_STATE_RELEASED;
    if (!pressed) displaySleepWakeArmed = false;
    return;
  }
  data->state = pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
  if (pressed) {
    data->point.x = x;
    data->point.y = y;
    if (!ledcardsSleepTouchActive) {
      ledcardsSleepTouchActive = true;
      ledcardsSleepLongPressFired = false;
      ledcardsSleepPressStartMs = millis();
    } else if (!ledcardsSleepLongPressFired && millis() - ledcardsSleepPressStartMs >= kLedcardsSleepLongPressMs) {
      ledcardsSleepLongPressFired = true;
      enterDisplaySleep();
      data->state = LV_INDEV_STATE_RELEASED;
    }
  } else {
    ledcardsSleepTouchActive = false;
    ledcardsSleepLongPressFired = false;
  }
}

bool wifiConfigured() {
  return strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "change-me") != 0;
}

void connectWiFi() {
#if POWER_SENTINEL_HTTP_FALLBACK
  if (!wifiConfigured()) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) delay(250);
#endif
}

bool fetchHttpSummary() {
#if POWER_SENTINEL_HTTP_FALLBACK
  if (!wifiConfigured() || WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.setTimeout(3500);
  if (!http.begin(POWER_SENTINEL_SUMMARY_URL)) return false;
  int code = http.GET();
  if (code != 200) {
    http.end();
    snprintf(state.transportStatus, sizeof(state.transportStatus), "HTTP %d", code);
    return false;
  }
  String body = http.getString();
  http.end();
  parseSummary(body, "http");
  return !state.offline;
#else
  return false;
#endif
}

void initSerialTransport() {
#if POWER_SENTINEL_TRANSPORT_SERIAL
  Serial2.begin(POWER_SENTINEL_UART_BAUD, SERIAL_8N1, POWER_SENTINEL_UART_RX_PIN, POWER_SENTINEL_UART_TX_PIN);
#endif
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
    return false;
  }
  const char *rid = doc["request_id"] | "";
  if (strcmp(rid, requestId.c_str()) != 0) return false;
  JsonObjectConst error = doc["error"].as<JsonObjectConst>();
  int code = error["code"] | 0;
  if (code != 0) {
    snprintf(state.transportStatus, sizeof(state.transportStatus), "StackFlow error %d", code);
    return false;
  }
  String body;
  serializeJson(doc["data"], body);
  parseSummary(body, "stackflow");
  return !state.offline;
}

bool fetchSerialSummary() {
#if POWER_SENTINEL_TRANSPORT_SERIAL
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

bool fetchLiveSummary() {
  ++fetchAttemptCount;
  if (fetchSerialSummary()) {
    ++fetchOkCount;
    return true;
  }
  if (fetchHttpSummary()) {
    ++fetchOkCount;
    return true;
  }
  ++fetchFailCount;
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
  psDisplaySetBrightness(kDisplayAwakeBrightness);
  initLvgl();
  createLedcardsInterfaceUi(makeLedcardsInterfaceNutView());
  initSerialTransport();
  connectWiFi();
  Serial.printf("Power Sentinel %s clean NUT monitor baseline\n", POWER_SENTINEL_FIRMWARE_BUILD);
}

void loop() {
  psM5Update();
  uint32_t now = millis();
  uint32_t delta = now - lastLvTickMs;
  lastLvTickMs = now;
  lv_tick_inc(delta ? delta : 1);
  lv_timer_handler();
  if (now - lastFetchMs >= kSummaryPollMs || lastFetchMs == 0) {
    lastFetchMs = now;
    fetchLiveSummary();
    updateLedcardsInterfaceUi(makeLedcardsInterfaceNutView());
  }
  delay(5);
}

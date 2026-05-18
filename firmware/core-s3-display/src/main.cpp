#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include <WiFi.h>

#include "m5_hal.h"

#if __has_include("power_sentinel_config.h")
#include "power_sentinel_config.h"
#else
#include "power_sentinel_config.example.h"
#endif

#ifndef POWER_SENTINEL_FIRMWARE_BUILD
#define POWER_SENTINEL_FIRMWARE_BUILD "stackflow-2026-05-18"
#endif
#ifndef POWER_SENTINEL_UART_RX_PIN
#define POWER_SENTINEL_UART_RX_PIN 18
#endif
#ifndef POWER_SENTINEL_UART_TX_PIN
#define POWER_SENTINEL_UART_TX_PIN 17
#endif
#ifndef POWER_SENTINEL_UART_BAUD
// Keep the vendor StackFlow/llm_sys UART default: 115200 8N1.
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
#ifndef POWER_SENTINEL_SERIAL_MAX_JSON_BYTES
#define POWER_SENTINEL_SERIAL_MAX_JSON_BYTES 8192
#endif
#ifndef POWER_SENTINEL_SERIAL_RETRIES
#define POWER_SENTINEL_SERIAL_RETRIES 3
#endif
#ifndef POWER_SENTINEL_STALE_GRACE_MS
#define POWER_SENTINEL_STALE_GRACE_MS (SUMMARY_POLL_MS * 3UL)
#endif

namespace {
constexpr uint16_t kScreenW = 320;
constexpr uint16_t kScreenH = 240;

struct UpsState {
  bool available = false;
  bool onBattery = false;
  bool lowBattery = false;
  bool stale = true;
  char status[24] = "UNKNOWN";
  char statusLabel[32] = "Waiting for UPS";
  char model[48] = "unknown";
  char beeperStatus[24] = "unknown";
  char transferReason[48] = "unknown";
  char driver[24] = "unknown";
  int batteryPercent = -1;
  int runtimeSeconds = -1;
  int loadPercent = -1;
  int realpowerNominalW = -1;
  int loadW = -1;
  float inputVoltage = 0.0f;
  float outputVoltage = 0.0f;
  float batteryVoltage = 0.0f;
  int ageSeconds = -1;
};

struct NutState {
  bool serverActive = false;
  bool driverActive = false;
  bool monitorActive = false;
  int clientCount = -1;
  char mode[20] = "unknown";
  char shutdownState[20] = "unknown";
  char clients[96] = "none";
};

struct ServiceState {
  bool available = false;
  bool mqtt = false;
  bool stackflowOk = false;
  bool openaiOk = false;
  bool chatSmokeOk = false;
  char severity[12] = "unknown";
  char shutdownState[20] = "unknown";
  float temperatureC = 0.0f;
  int ramAvailableMb = -1;
  float diskFreeGb = 0.0f;
};

struct SummaryState {
  char schema[32] = "power-sentinel.summary.v1";
  char timestamp[32] = "waiting";
  char severity[12] = "unknown";
  bool offline = true;
  uint32_t lastGoodMillis = 0;
  char source[16] = "boot";
  char transportStatus[96] = "Waiting for StackFlow summary";
  UpsState ups;
  NutState nut;
  ServiceState ha;
  ServiceState proxmox;
  ServiceState m5stack;
  char problems[192] = "No active problems";
};

SummaryState state;
uint32_t lastFetchMs = 0;
uint32_t lastLvTickMs = 0;
uint32_t fetchAttemptCount = 0;
uint32_t fetchOkCount = 0;
uint32_t fetchFailCount = 0;
uint32_t lastFetchDurationMs = 0;
uint32_t stackflowRequestId = 0;

lv_color_t buf1[kScreenW * 24];
lv_color_t buf2[kScreenW * 24];
lv_display_t *display = nullptr;
lv_indev_t *indev = nullptr;

lv_obj_t *tabview = nullptr;
lv_obj_t *homeTab = nullptr;
lv_obj_t *nutTab = nullptr;
lv_obj_t *proxmoxTab = nullptr;
lv_obj_t *haTab = nullptr;
lv_obj_t *m5sTab = nullptr;

// Official M5Module-LLM Arduino examples use Serial2 for the CoreS3 stacked
// module UART. ESP32-S3 can route either UART to these pins, but matching the
// vendor-tested UART instance avoids subtle driver/resource differences.
HardwareSerial LlmSerial(2);


const lv_color_t severityColor(const char *severity) {
  if (strcmp(severity, "critical") == 0) return lv_palette_main(LV_PALETTE_RED);
  if (strcmp(severity, "warn") == 0) return lv_palette_main(LV_PALETTE_ORANGE);
  return lv_palette_main(LV_PALETTE_GREEN);
}

void safeCopy(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  snprintf(dst, dstSize, "%s", src ? src : "");
}

const char *intOrUnknown(int value, char *buf, size_t bufSize, const char *suffix = "") {
  if (value < 0) return "unknown";
  snprintf(buf, bufSize, "%d%s", value, suffix);
  return buf;
}

const char *floatOrUnknown(float value, char *buf, size_t bufSize, const char *suffix = "") {
  if (value <= 0.0f) return "unknown";
  snprintf(buf, bufSize, "%.1f%s", value, suffix);
  return buf;
}

int jsonInt(JsonVariantConst v, int fallback) {
  return v.isNull() ? fallback : v.as<int>();
}

float jsonFloat(JsonVariantConst v, float fallback) {
  return v.isNull() ? fallback : v.as<float>();
}

void parseSummary(const String &json, bool fromNetwork) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("JSON parse error: %s\n", err.c_str());
    return;
  }

  safeCopy(state.schema, sizeof(state.schema), doc["schema"] | "unknown");
  safeCopy(state.timestamp, sizeof(state.timestamp), doc["timestamp"] | "unknown");
  safeCopy(state.severity, sizeof(state.severity), doc["severity"] | "warn");
  state.offline = !fromNetwork;
  safeCopy(state.source, sizeof(state.source), fromNetwork ? "live" : "offline");
  if (fromNetwork) state.lastGoodMillis = millis();

  JsonObjectConst ups = doc["ups"].as<JsonObjectConst>();
  state.ups.available = ups["available"] | false;
  state.ups.onBattery = ups["on_battery"] | false;
  state.ups.lowBattery = ups["low_battery"] | false;
  state.ups.stale = ups["stale"] | false;
  safeCopy(state.ups.status, sizeof(state.ups.status), ups["status"] | "UNKNOWN");
  safeCopy(state.ups.statusLabel, sizeof(state.ups.statusLabel), ups["status_label"] | "Unknown");
  state.ups.batteryPercent = jsonInt(ups["battery_percent"], -1);
  state.ups.runtimeSeconds = jsonInt(ups["runtime_seconds"], -1);
  state.ups.loadPercent = jsonInt(ups["load_percent"], -1);
  state.ups.inputVoltage = jsonFloat(ups["input_voltage"], 0);
  state.ups.outputVoltage = jsonFloat(ups["output_voltage"], 0);
  state.ups.batteryVoltage = jsonFloat(ups["battery_voltage"], 0);
  state.ups.realpowerNominalW = jsonInt(ups["realpower_nominal_w"], -1);
  state.ups.loadW = jsonInt(ups["load_w"], -1);
  safeCopy(state.ups.model, sizeof(state.ups.model), ups["model"] | "unknown");
  safeCopy(state.ups.beeperStatus, sizeof(state.ups.beeperStatus), ups["beeper_status"] | "unknown");
  safeCopy(state.ups.transferReason, sizeof(state.ups.transferReason), ups["transfer_reason"] | "unknown");
  safeCopy(state.ups.driver, sizeof(state.ups.driver), ups["driver"] | "unknown");
  state.ups.ageSeconds = jsonInt(ups["age_seconds"], -1);

  JsonObjectConst nut = doc["nut"].as<JsonObjectConst>();
  state.nut.serverActive = nut["server_active"] | false;
  state.nut.driverActive = nut["driver_active"] | false;
  state.nut.monitorActive = nut["monitor_active"] | false;
  state.nut.clientCount = jsonInt(nut["client_count"], -1);
  safeCopy(state.nut.mode, sizeof(state.nut.mode), nut["mode"] | "unknown");
  safeCopy(state.nut.shutdownState, sizeof(state.nut.shutdownState), nut["shutdown_state"] | "unknown");
  JsonArrayConst clients = nut["clients"].as<JsonArrayConst>();
  if (clients.isNull() || clients.size() == 0) {
    safeCopy(state.nut.clients, sizeof(state.nut.clients), "none");
  } else {
    String joined;
    for (JsonVariantConst client : clients) {
      if (joined.length() > 0) joined += ", ";
      joined += client.as<const char *>();
    }
    safeCopy(state.nut.clients, sizeof(state.nut.clients), joined.c_str());
  }

  JsonObjectConst ha = doc["homeassistant"].as<JsonObjectConst>();
  state.ha.available = ha["available"] | false;
  state.ha.mqtt = ha["mqtt"] | false;
  safeCopy(state.ha.severity, sizeof(state.ha.severity), ha["severity"] | "warn");

  JsonObjectConst px = doc["proxmox"].as<JsonObjectConst>();
  state.proxmox.available = px["available"] | false;
  safeCopy(state.proxmox.severity, sizeof(state.proxmox.severity), px["severity"] | "warn");
  safeCopy(state.proxmox.shutdownState, sizeof(state.proxmox.shutdownState), px["shutdown_state"] | "unknown");

  JsonObjectConst m5 = doc["m5stack"].as<JsonObjectConst>();
  state.m5stack.available = m5["available"] | false;
  safeCopy(state.m5stack.severity, sizeof(state.m5stack.severity), m5["severity"] | "warn");
  state.m5stack.temperatureC = jsonFloat(m5["temperature_c"], 0);
  state.m5stack.ramAvailableMb = jsonInt(m5["ram_available_mb"], -1);
  state.m5stack.diskFreeGb = jsonFloat(m5["disk_free_gb"], 0);
  state.m5stack.stackflowOk = m5["stackflow_ok"] | false;
  state.m5stack.openaiOk = m5["openai_ok"] | false;
  state.m5stack.chatSmokeOk = m5["chat_smoke_ok"] | false;

  JsonArrayConst problems = doc["problems"].as<JsonArrayConst>();
  if (problems.isNull() || problems.size() == 0) {
    safeCopy(state.problems, sizeof(state.problems), "No active problems");
  } else {
    String joined;
    for (JsonVariantConst p : problems) {
      if (joined.length() > 0) joined += " | ";
      joined += p.as<const char *>();
    }
    safeCopy(state.problems, sizeof(state.problems), joined.c_str());
  }
}

void myDispFlush(lv_display_t *disp, const lv_area_t *area, uint8_t *pxMap) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  // LVGL renders RGB565 in host byte order. M5GFX/CoreS3 expects the bytes swapped
  // for LCD DMA/write operations; do the same explicit swap used by the upstream
  // LVGL + M5Stack PlatformIO example instead of relying on LV_COLOR_16_SWAP or
  // M5GFX writePixels' swap flag. The old double/implicit swap path produced
  // visibly wrong colors with M5Unified 0.2.x / LVGL 9.5.
  lv_draw_sw_rgb565_swap(pxMap, w * h);
  psDisplayWritePixels(area->x1, area->y1, w, h, reinterpret_cast<uint16_t *>(pxMap), false);
  lv_display_flush_ready(disp);
}

void myTouchRead(lv_indev_t *, lv_indev_data_t *data) {
  int32_t x = 0;
  int32_t y = 0;
  if (psTouchPressed(&x, &y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void stylePanel(lv_obj_t *card, lv_color_t bg, lv_color_t border) {
  lv_obj_set_width(card, lv_pct(100));
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(card, 6, 0);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_set_style_radius(card, 14, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, border, 0);
  lv_obj_set_style_bg_color(card, bg, 0);
  lv_obj_set_style_shadow_width(card, 10, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_60, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_ofs_y(card, 3, 0);
}

lv_obj_t *makeCard(lv_obj_t *parent, const char *title) {
  lv_obj_t *card = lv_obj_create(parent);
  stylePanel(card, lv_color_hex(0x171b24), lv_color_hex(0x394152));

  lv_obj_t *label = lv_label_create(card);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_color(label, lv_color_hex(0xe8eefc), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
  return card;
}

lv_obj_t *makeHeroCard(lv_obj_t *parent, const char *title) {
  lv_obj_t *card = lv_obj_create(parent);
  stylePanel(card, lv_color_hex(0x111827), severityColor(state.severity));
  lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_set_style_pad_gap(card, 7, 0);

  lv_obj_t *label = lv_label_create(card);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_color(label, lv_color_hex(0xf8fbff), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  return card;
}

lv_obj_t *addLine(lv_obj_t *parent, const char *text) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(0xc8d0df), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, lv_pct(100));
  return label;
}

void addBadge(lv_obj_t *parent, const char *text, lv_color_t color) {
  lv_obj_t *badge = lv_label_create(parent);
  lv_label_set_text(badge, text);
  lv_obj_set_style_text_color(badge, lv_color_white(), 0);
  lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
  lv_obj_set_style_bg_color(badge, color, 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(badge, 10, 0);
  lv_obj_set_style_pad_hor(badge, 8, 0);
  lv_obj_set_style_pad_ver(badge, 4, 0);
}

lv_obj_t *addMetricRow(lv_obj_t *parent, const char *label, const char *value) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *left = lv_label_create(row);
  lv_label_set_text(left, label);
  lv_obj_set_style_text_color(left, lv_color_hex(0x8fa0b8), 0);
  lv_obj_set_style_text_font(left, &lv_font_montserrat_12, 0);

  lv_obj_t *right = lv_label_create(row);
  lv_label_set_text(right, value);
  lv_obj_set_style_text_color(right, lv_color_hex(0xf8fbff), 0);
  lv_obj_set_style_text_font(right, &lv_font_montserrat_14, 0);
  return row;
}

void addStatusPillRow(lv_obj_t *parent, const char *a, lv_color_t ca, const char *b, lv_color_t cb, const char *c, lv_color_t cc) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_gap(row, 4, 0);
  addBadge(row, a, ca);
  addBadge(row, b, cb);
  addBadge(row, c, cc);
}

void addPercentBar(lv_obj_t *parent, int value, lv_color_t color) {
  lv_obj_t *bar = lv_bar_create(parent);
  lv_obj_set_width(bar, lv_pct(100));
  lv_obj_set_height(bar, 10);
  lv_bar_set_range(bar, 0, 100);
  int clamped = value < 0 ? 0 : (value > 100 ? 100 : value);
  lv_bar_set_value(bar, clamped, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x283041), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
}

void setupPage(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(tab, 8, 0);
  lv_obj_set_style_pad_gap(tab, 8, 0);
}

const char *runtimeText(int seconds, char *buf, size_t bufSize) {
  if (seconds < 0) return "unknown";
  int minutes = seconds / 60;
  int rem = seconds % 60;
  snprintf(buf, bufSize, "%dm%02ds", minutes, rem);
  return buf;
}

const char *okDown(bool ok) {
  return ok ? "OK" : "DOWN";
}

bool haFunctional() {
  return state.ha.available && state.ha.mqtt;
}

const char *nutStatusBadge() {
  if (!state.ups.available || !state.nut.serverActive || !state.nut.driverActive) return "NUT UNK";
  if (state.ups.lowBattery) return "NUT CRIT";
  if (state.ups.onBattery) return "NUT WARN";
  return "NUT OK";
}

void renderHome() {
  lv_obj_clean(homeTab);
  setupPage(homeTab);
  lv_obj_t *card = makeHeroCard(homeTab, "POWER SENTINEL");
  addBadge(card, state.offline ? "STALE" : state.severity, state.offline ? lv_palette_main(LV_PALETTE_ORANGE) : severityColor(state.severity));
  addLine(card, state.ups.lowBattery ? "LOW BATTERY" : (state.ups.onBattery ? "ON BATTERY" : (state.ups.available ? "GRID ONLINE" : "UPS UNAVAILABLE")));

  char line[128];
  char battery[24];
  char runtime[24];
  snprintf(line, sizeof(line), "%s   %s",
           intOrUnknown(state.ups.batteryPercent, battery, sizeof(battery), "%"),
           runtimeText(state.ups.runtimeSeconds, runtime, sizeof(runtime)));
  addMetricRow(card, "battery / runtime", line);
  addPercentBar(card, state.ups.batteryPercent, state.ups.lowBattery ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN));

  char load[24];
  char inputV[24];
  snprintf(line, sizeof(line), "%s   %s",
           intOrUnknown(state.ups.loadPercent, load, sizeof(load), "%"),
           floatOrUnknown(state.ups.inputVoltage, inputV, sizeof(inputV), "V"));
  addMetricRow(card, "load / input", line);
  addPercentBar(card, state.ups.loadPercent, lv_palette_main(LV_PALETTE_BLUE));

  char nutPill[24];
  snprintf(nutPill, sizeof(nutPill), "%s", nutStatusBadge());
  char pvePill[16];
  snprintf(pvePill, sizeof(pvePill), "PVE %s", okDown(state.proxmox.available));
  char haPill[16];
  snprintf(haPill, sizeof(haPill), "HA %s", okDown(haFunctional()));
  addStatusPillRow(card, nutPill, state.ups.onBattery ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREEN),
                   pvePill, state.proxmox.available ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED),
                   haPill, haFunctional() ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));

  snprintf(line, sizeof(line), "NET UNK   M5S %s", state.m5stack.available ? "OK" : "DOWN");
  addMetricRow(card, "local", line);
  snprintf(line, sizeof(line), "Problems: %s", state.problems);
  addLine(card, line);
}

void renderNut() {
  lv_obj_clean(nutTab);
  setupPage(nutTab);
  lv_obj_t *card = makeCard(nutTab, "NUT");
  if (state.offline) {
    addBadge(card, "NO LIVE DATA", lv_palette_main(LV_PALETTE_ORANGE));
  }
  const char *upsBadge = !state.ups.available ? "UPS UNKNOWN" : (state.ups.lowBattery ? "LOW BATTERY" : (state.ups.onBattery ? "ON BATTERY" : "ONLINE"));
  lv_color_t upsColor = !state.ups.available ? lv_palette_main(LV_PALETTE_GREY) :
                        (state.ups.lowBattery ? lv_palette_main(LV_PALETTE_RED) :
                         (state.ups.onBattery ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREEN)));
  addBadge(card, upsBadge, upsColor);
  char line[128];
  snprintf(line, sizeof(line), "Status: %s (%s)", state.ups.statusLabel, state.ups.status);
  addLine(card, line);
  char battery[24];
  char runtime[24];
  snprintf(line, sizeof(line), "Battery %s   Runtime %s",
           intOrUnknown(state.ups.batteryPercent, battery, sizeof(battery), "%"),
           runtimeText(state.ups.runtimeSeconds, runtime, sizeof(runtime)));
  addLine(card, line);
  addPercentBar(card, state.ups.batteryPercent, state.ups.lowBattery ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN));
  char load[24];
  char watts[24];
  char inputV[24];
  snprintf(line, sizeof(line), "Load %s / %s   Input %s",
           intOrUnknown(state.ups.loadPercent, load, sizeof(load), "%"),
           intOrUnknown(state.ups.loadW, watts, sizeof(watts), "W"),
           floatOrUnknown(state.ups.inputVoltage, inputV, sizeof(inputV), "V"));
  addLine(card, line);
  addPercentBar(card, state.ups.loadPercent, lv_palette_main(LV_PALETTE_BLUE));

  lv_obj_t *nutCard = makeCard(nutTab, "NUT server");
  snprintf(line, sizeof(line), "server %s   driver %s", okDown(state.nut.serverActive), okDown(state.nut.driverActive));
  addLine(nutCard, line);
  snprintf(line, sizeof(line), "monitor %s   mode %s", state.nut.monitorActive ? "ARMED" : "off", state.nut.mode);
  addLine(nutCard, line);
  char clientCount[24];
  snprintf(line, sizeof(line), "shutdown %s   clients %s", state.nut.shutdownState, state.nut.clientCount < 0 ? "unknown" : intOrUnknown(state.nut.clientCount, clientCount, sizeof(clientCount)));
  addLine(nutCard, line);
  snprintf(line, sizeof(line), "client list: %s", state.nut.clients);
  addLine(nutCard, line);

  lv_obj_t *details = makeCard(nutTab, "UPS details");
  char battV[24];
  char outV[24];
  snprintf(line, sizeof(line), "Model: %s", state.ups.model);
  addLine(details, line);
  snprintf(line, sizeof(line), "BattV %s   Out %s",
           floatOrUnknown(state.ups.batteryVoltage, battV, sizeof(battV), "V"),
           floatOrUnknown(state.ups.outputVoltage, outV, sizeof(outV), "V"));
  addLine(details, line);
  snprintf(line, sizeof(line), "Nominal %s   Driver %s", intOrUnknown(state.ups.realpowerNominalW, watts, sizeof(watts), "W"), state.ups.driver);
  addLine(details, line);
  snprintf(line, sizeof(line), "Beeper %s", state.ups.beeperStatus);
  addLine(details, line);
  snprintf(line, sizeof(line), "Transfer: %s", state.ups.transferReason);
  addLine(details, line);
}

void renderHa() {
  lv_obj_clean(haTab);
  setupPage(haTab);
  lv_obj_t *card = makeCard(haTab, "Home Assistant");
  addBadge(card, state.ha.available ? "HA API OK" : "HA API DOWN", severityColor(state.ha.severity));
  addLine(card, state.ha.available ? "Home Assistant API reachable" : "Home Assistant API unreachable");
  addLine(card, state.ha.mqtt ? "MQTT broker reachable" : "MQTT broker unreachable");
}

void renderProxmox() {
  lv_obj_clean(proxmoxTab);
  setupPage(proxmoxTab);
  lv_obj_t *card = makeCard(proxmoxTab, "Proxmox");
  addBadge(card, state.proxmox.available ? "PVE OK" : "PVE DOWN", severityColor(state.proxmox.severity));
  char line[96];
  snprintf(line, sizeof(line), "API: %s", state.proxmox.available ? "reachable" : "unreachable");
  addLine(card, line);
  snprintf(line, sizeof(line), "Shutdown state: %s", state.proxmox.shutdownState);
  addLine(card, line);
}

void renderM5s() {
  lv_obj_clean(m5sTab);
  setupPage(m5sTab);
  lv_obj_t *card = makeCard(m5sTab, "M5S");
  addBadge(card, state.m5stack.available ? "M5S OK" : "M5S DOWN", severityColor(state.m5stack.severity));
  char line[160];
  char temp[24];
  char ram[24];
  snprintf(line, sizeof(line), "Temp %s   RAM %s",
           floatOrUnknown(state.m5stack.temperatureC, temp, sizeof(temp), " C"),
           intOrUnknown(state.m5stack.ramAvailableMb, ram, sizeof(ram), " MB"));
  addLine(card, line);
  char disk[24];
  snprintf(line, sizeof(line), "Disk free %s", floatOrUnknown(state.m5stack.diskFreeGb, disk, sizeof(disk), " GB"));
  addLine(card, line);
  snprintf(line, sizeof(line), "StackFlow %s   OpenAI %s", state.m5stack.stackflowOk ? "OK" : "FAIL", state.m5stack.openaiOk ? "OK" : "FAIL");
  addLine(card, line);
  snprintf(line, sizeof(line), "Chat smoke %s", state.m5stack.chatSmokeOk ? "OK" : "FAIL");
  addLine(card, line);

  lv_obj_t *transport = makeCard(m5sTab, "Transport");
  addBadge(transport, state.offline ? "OFFLINE" : "LIVE", state.offline ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREEN));
  snprintf(line, sizeof(line), "Source %s   Schema %s", state.source, state.schema);
  addLine(transport, line);
  snprintf(line, sizeof(line), "Transport: %s", state.transportStatus);
  addLine(transport, line);
  snprintf(line, sizeof(line), "Poll %lu ok / %lu fail / last %lums",
           static_cast<unsigned long>(fetchOkCount),
           static_cast<unsigned long>(fetchFailCount),
           static_cast<unsigned long>(lastFetchDurationMs));
  addLine(transport, line);
  snprintf(line, sizeof(line), "FW %s", POWER_SENTINEL_FIRMWARE_BUILD);
  addLine(transport, line);
  snprintf(line, sizeof(line), "UART %d/%d %d", POWER_SENTINEL_UART_RX_PIN, POWER_SENTINEL_UART_TX_PIN, POWER_SENTINEL_UART_BAUD);
  addLine(transport, line);
  snprintf(line, sizeof(line), "Problems: %s", state.problems);
  addLine(transport, line);
}

void renderAll() {
  renderHome();
  renderNut();
  renderProxmox();
  renderHa();
  renderM5s();
}

bool wifiConfigured() {
  return strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "your-ssid") != 0;
}

void connectWiFi() {
  if (!wifiConfigured()) {
    Serial.println("WiFi not configured; HTTP fallback disabled until credentials are set.");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting WiFi SSID '%s'", WIFI_SSID);
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; ++i) {
    delay(250);
    Serial.print('.');
  }
  Serial.println(WiFi.status() == WL_CONNECTED ? " connected" : " failed");
}

bool fetchSummary() {
  if (WiFi.status() != WL_CONNECTED) {
    safeCopy(state.transportStatus, sizeof(state.transportStatus), "HTTP skipped: WiFi disconnected");
    return false;
  }
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(POWER_SENTINEL_SUMMARY_URL)) {
    safeCopy(state.transportStatus, sizeof(state.transportStatus), "HTTP begin failed");
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("HTTP summary failed: %d\n", code);
    http.end();
    snprintf(state.transportStatus, sizeof(state.transportStatus), "HTTP failed: %d", code);
    return false;
  }
  String body = http.getString();
  http.end();
  parseSummary(body, true);
  safeCopy(state.source, sizeof(state.source), "http");
  safeCopy(state.transportStatus, sizeof(state.transportStatus), "HTTP OK");
  return true;
}

#if POWER_SENTINEL_TRANSPORT_SERIAL
void initSerialTransport() {
  LlmSerial.begin(POWER_SENTINEL_UART_BAUD, SERIAL_8N1, POWER_SENTINEL_UART_RX_PIN, POWER_SENTINEL_UART_TX_PIN);
  LlmSerial.setTimeout(POWER_SENTINEL_SERIAL_TIMEOUT_MS);
  Serial.printf("Serial transport enabled: RX=%d TX=%d baud=%d mode=stackflow\n",
                POWER_SENTINEL_UART_RX_PIN, POWER_SENTINEL_UART_TX_PIN, POWER_SENTINEL_UART_BAUD);
}

bool readSerialLine(String &line, uint32_t timeoutMs) {
  line = "";
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    while (LlmSerial.available() > 0) {
      char ch = static_cast<char>(LlmSerial.read());
      if (ch == '\r') continue;
      if (ch == '\n') return true;
      if (line.length() < POWER_SENTINEL_SERIAL_MAX_JSON_BYTES) line += ch;
    }
    delay(2);
    lv_timer_handler();
  }
  return line.length() > 0;
}


bool parseStackFlowSummaryResponse(const String &line, const String &requestId, uint8_t attempt) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, line);
  if (err) {
    Serial.printf("StackFlow JSON parse error: %s\n", err.c_str());
    snprintf(state.transportStatus, sizeof(state.transportStatus), "StackFlow JSON parse fail try %u", static_cast<unsigned>(attempt));
    return false;
  }

  const char *rxRequestId = doc["request_id"] | "";
  if (requestId.length() > 0 && strcmp(rxRequestId, requestId.c_str()) != 0) {
    Serial.printf("StackFlow stale/unrelated response: request_id=%s expected=%s\n", rxRequestId, requestId.c_str());
    snprintf(state.transportStatus, sizeof(state.transportStatus), "StackFlow stale id try %u", static_cast<unsigned>(attempt));
    return false;
  }

  int errorCode = doc["error"]["code"] | -999;
  if (errorCode != 0) {
    const char *message = doc["error"]["message"] | "unknown";
    Serial.printf("StackFlow summary error: code=%d message=%s\n", errorCode, message);
    snprintf(state.transportStatus, sizeof(state.transportStatus), "StackFlow error %d try %u", errorCode, static_cast<unsigned>(attempt));
    return false;
  }

  JsonVariantConst data = doc["data"];
  if (data.isNull()) {
    safeCopy(state.transportStatus, sizeof(state.transportStatus), "StackFlow response missing data");
    return false;
  }

  String body;
  serializeJson(data, body);
  parseSummary(body, true);
  safeCopy(state.source, sizeof(state.source), "stackflow");
  snprintf(state.transportStatus, sizeof(state.transportStatus), "StackFlow OK: %u bytes try %u/%u",
           static_cast<unsigned>(line.length()), static_cast<unsigned>(attempt), static_cast<unsigned>(POWER_SENTINEL_SERIAL_RETRIES));
  Serial.printf("StackFlow summary OK: %u bytes\n", static_cast<unsigned>(line.length()));
  return true;
}

bool fetchSerialSummary() {
  for (uint8_t attempt = 1; attempt <= POWER_SENTINEL_SERIAL_RETRIES; ++attempt) {
    while (LlmSerial.available() > 0) LlmSerial.read();

    String requestId = "ps-" + String(++stackflowRequestId);
    JsonDocument request;
    request["request_id"] = requestId;
    request["work_id"] = "sentinel";
    request["action"] = "summary";
    request["object"] = "None";
    request["data"] = "None";
    serializeJson(request, LlmSerial);
    LlmSerial.print('\n');
    LlmSerial.flush();
    Serial.printf("Serial TX StackFlow sentinel.summary id=%s attempt %u/%u\n",
                  requestId.c_str(), static_cast<unsigned>(attempt), static_cast<unsigned>(POWER_SENTINEL_SERIAL_RETRIES));

    String line;
    if (!readSerialLine(line, POWER_SENTINEL_SERIAL_TIMEOUT_MS)) {
      Serial.println("StackFlow summary failed: no response line");
      snprintf(state.transportStatus, sizeof(state.transportStatus), "StackFlow no response try %u/%u",
               static_cast<unsigned>(attempt), static_cast<unsigned>(POWER_SENTINEL_SERIAL_RETRIES));
      delay(120);
      continue;
    }
    Serial.printf("Serial RX StackFlow: %.160s\n", line.c_str());
    if (parseStackFlowSummaryResponse(line, requestId, attempt)) return true;
    delay(120);
  }
  return false;
}
#endif

bool fetchLiveSummary() {
  ++fetchAttemptCount;
  uint32_t startedMs = millis();
  bool ok = false;
#if POWER_SENTINEL_TRANSPORT_SERIAL
  ok = fetchSerialSummary();
  if (ok) {
    lastFetchDurationMs = millis() - startedMs;
    ++fetchOkCount;
    return true;
  }
#endif
#if POWER_SENTINEL_HTTP_FALLBACK
  ok = fetchSummary();
#else
  ok = false;
#endif
  lastFetchDurationMs = millis() - startedMs;
  if (ok) {
    ++fetchOkCount;
  } else {
    ++fetchFailCount;
  }
  return ok;
}

void applyFetchResult(bool ok) {
  if (ok) {
    state.offline = false;
    return;
  }
  if (state.lastGoodMillis > 0 && millis() - state.lastGoodMillis <= POWER_SENTINEL_STALE_GRACE_MS) {
    state.offline = false;
    safeCopy(state.source, sizeof(state.source), "serial-stale");
    return;
  }
  state.offline = true;
  safeCopy(state.source, sizeof(state.source), "offline");
}

void initLvgl() {
  lv_init();
  display = lv_display_create(kScreenW, kScreenH);
  lv_display_set_flush_cb(display, myDispFlush);
  lv_display_set_buffers(display, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, myTouchRead);
}

void applyAppTheme() {
  lv_obj_t *screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, lv_color_hex(0x070b12), 0);
  lv_obj_set_style_bg_grad_color(screen, lv_color_hex(0x121826), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_text_font(screen, &lv_font_montserrat_14, 0);

  lv_obj_set_style_bg_color(tabview, lv_color_hex(0x070b12), 0);
  lv_obj_t *tabBar = lv_tabview_get_tab_bar(tabview);
  lv_obj_set_style_bg_color(tabBar, lv_color_hex(0x0b1220), 0);
  lv_obj_set_style_bg_opa(tabBar, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(tabBar, 4, 0);
  lv_obj_set_style_pad_gap(tabBar, 2, 0);
  lv_obj_set_style_text_font(tabBar, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(tabBar, lv_color_hex(0x9fb0c8), LV_PART_ITEMS);
  lv_obj_set_style_text_color(tabBar, lv_color_hex(0xf8fbff), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(tabBar, lv_color_hex(0x2563eb), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(tabBar, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_radius(tabBar, 9, LV_PART_ITEMS);
}

void initUi() {
  tabview = lv_tabview_create(lv_screen_active());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
  lv_tabview_set_tab_bar_size(tabview, 34);
  applyAppTheme();
  homeTab = lv_tabview_add_tab(tabview, "HOME");
  nutTab = lv_tabview_add_tab(tabview, "NUT");
  proxmoxTab = lv_tabview_add_tab(tabview, "PVE");
  haTab = lv_tabview_add_tab(tabview, "HA");
  m5sTab = lv_tabview_add_tab(tabview, "M5S");
  renderAll();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("Power Sentinel firmware build: %s\n", POWER_SENTINEL_FIRMWARE_BUILD);
  // CoreS3 stacked 5V bus direction is deliberately configurable:
  // - false: accept power from the LLM Mate/Module/base without feeding 5V back.
  // - true:  feed 5V from CoreS3 USB-C to the M-Bus/stack.
  // The stock firmware appears to support both directions dynamically; M5Unified
  // exposes this as one explicit output-enable bit, so the Power Sentinel firmware
  // chooses the safe external-input mode by default.
#if POWER_SENTINEL_STACK_POWER_OUT
  psM5Begin(true);
#else
  psM5Begin(false);
#endif
  psDisplaySetRotation(1);
  psDisplaySetBrightness(160);

  initLvgl();
  initUi();
#if POWER_SENTINEL_TRANSPORT_SERIAL
  initSerialTransport();
#endif
#if POWER_SENTINEL_HTTP_FALLBACK
  connectWiFi();
#endif
  bool firstFetchOk = fetchLiveSummary();
  applyFetchResult(firstFetchOk);
  renderAll();
  lastFetchMs = millis();
  lastLvTickMs = millis();
}

void loop() {
  uint32_t now = millis();
  lv_tick_inc(now - lastLvTickMs);
  lastLvTickMs = now;
  lv_timer_handler();
  delay(5);
  if (now - lastFetchMs > SUMMARY_POLL_MS) {
    lastFetchMs = now;
    bool ok = fetchLiveSummary();
    applyFetchResult(ok);
    renderAll();
  }
}

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <lvgl.h>
#include <M5Unified.h>
#include <WiFi.h>

#if __has_include("power_sentinel_config.h")
#include "power_sentinel_config.h"
#else
#include "power_sentinel_config.example.h"
#endif

namespace {
constexpr uint16_t kScreenW = 320;
constexpr uint16_t kScreenH = 240;

struct UpsState {
  bool available = true;
  bool onBattery = false;
  bool lowBattery = false;
  bool stale = false;
  char status[24] = "OL";
  char statusLabel[32] = "Online";
  int batteryPercent = 96;
  int runtimeSeconds = 4120;
  int loadPercent = 18;
  float inputVoltage = 229.4f;
  float outputVoltage = 230.1f;
  int ageSeconds = 4;
};

struct ServiceState {
  bool available = true;
  bool mqtt = true;
  bool stackflowOk = true;
  bool openaiOk = true;
  bool chatSmokeOk = true;
  char severity[12] = "ok";
  char shutdownState[20] = "disarmed";
  float temperatureC = 42.5f;
  int ramAvailableMb = 742;
  float diskFreeGb = 18.7f;
};

struct SummaryState {
  char schema[32] = "power-sentinel.summary.v1";
  char timestamp[32] = "2026-05-17T15:30:00Z";
  char severity[12] = "ok";
  bool offline = false;
  uint32_t lastGoodMillis = 0;
  UpsState ups;
  ServiceState ha;
  ServiceState proxmox;
  ServiceState m5stack;
  char problems[192] = "No active problems";
};

SummaryState state;
uint32_t lastFetchMs = 0;
uint32_t lastLvTickMs = 0;

lv_color_t buf1[kScreenW * 24];
lv_color_t buf2[kScreenW * 24];
lv_display_t *display = nullptr;
lv_indev_t *indev = nullptr;

lv_obj_t *tabview = nullptr;
lv_obj_t *upsTab = nullptr;
lv_obj_t *haTab = nullptr;
lv_obj_t *proxmoxTab = nullptr;
lv_obj_t *m5Tab = nullptr;
lv_obj_t *offlineTab = nullptr;

String samplePayload() {
  return R"JSON({
    "schema":"power-sentinel.summary.v1",
    "timestamp":"2026-05-17T15:30:00Z",
    "severity":"ok",
    "ups":{"available":true,"status":"OL","status_label":"Online","on_battery":false,"low_battery":false,"battery_percent":96,"runtime_seconds":4120,"load_percent":18,"input_voltage":229.4,"output_voltage":230.1,"stale":false,"age_seconds":4},
    "homeassistant":{"available":true,"severity":"ok","mqtt":true},
    "proxmox":{"available":true,"severity":"ok","shutdown_state":"disarmed"},
    "m5stack":{"available":true,"severity":"ok","temperature_c":42.5,"ram_available_mb":742,"disk_free_gb":18.7,"stackflow_ok":true,"openai_ok":true,"chat_smoke_ok":true},
    "problems":[]
  })JSON";
}

const lv_color_t severityColor(const char *severity) {
  if (strcmp(severity, "critical") == 0) return lv_palette_main(LV_PALETTE_RED);
  if (strcmp(severity, "warn") == 0) return lv_palette_main(LV_PALETTE_ORANGE);
  return lv_palette_main(LV_PALETTE_GREEN);
}

void safeCopy(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  snprintf(dst, dstSize, "%s", src ? src : "");
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
  state.ups.ageSeconds = jsonInt(ups["age_seconds"], -1);

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
  M5.Display.startWrite();
  M5.Display.setAddrWindow(area->x1, area->y1, w, h);
  M5.Display.writePixels(reinterpret_cast<uint16_t *>(pxMap), w * h, true);
  M5.Display.endWrite();
  lv_display_flush_ready(disp);
}

void myTouchRead(lv_indev_t *, lv_indev_data_t *data) {
  M5.update();
  auto detail = M5.Touch.getDetail();
  if (detail.isPressed()) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = detail.x;
    data->point.y = detail.y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

lv_obj_t *makeCard(lv_obj_t *parent, const char *title) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_width(card, lv_pct(100));
  lv_obj_set_height(card, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(card, 6, 0);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x394152), 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x171b24), 0);

  lv_obj_t *label = lv_label_create(card);
  lv_label_set_text(label, title);
  lv_obj_set_style_text_color(label, lv_color_hex(0xe8eefc), 0);
  return card;
}

lv_obj_t *addLine(lv_obj_t *parent, const char *text) {
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_color(label, lv_color_hex(0xc8d0df), 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label, lv_pct(100));
  return label;
}

void addBadge(lv_obj_t *parent, const char *text, lv_color_t color) {
  lv_obj_t *badge = lv_label_create(parent);
  lv_label_set_text(badge, text);
  lv_obj_set_style_text_color(badge, lv_color_white(), 0);
  lv_obj_set_style_bg_color(badge, color, 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(badge, 8, 0);
  lv_obj_set_style_pad_hor(badge, 8, 0);
  lv_obj_set_style_pad_ver(badge, 4, 0);
}

void setupPage(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(tab, 8, 0);
  lv_obj_set_style_pad_gap(tab, 8, 0);
}

void renderUps() {
  lv_obj_clean(upsTab);
  setupPage(upsTab);
  lv_obj_t *card = makeCard(upsTab, "UPS / Power");
  char line[96];
  snprintf(line, sizeof(line), "Status: %s (%s)", state.ups.statusLabel, state.ups.status);
  addLine(card, line);
  snprintf(line, sizeof(line), "Battery: %d%%   Runtime: %d min", state.ups.batteryPercent, state.ups.runtimeSeconds / 60);
  addLine(card, line);
  lv_obj_t *bar = lv_bar_create(card);
  lv_obj_set_width(bar, lv_pct(100));
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, state.ups.batteryPercent < 0 ? 0 : state.ups.batteryPercent, LV_ANIM_OFF);
  snprintf(line, sizeof(line), "Load: %d%%   In/Out: %.1fV / %.1fV", state.ups.loadPercent, state.ups.inputVoltage, state.ups.outputVoltage);
  addLine(card, line);
  addBadge(card, state.ups.lowBattery ? "LOW BATTERY" : (state.ups.onBattery ? "ON BATTERY" : "ONLINE"),
           state.ups.lowBattery ? lv_palette_main(LV_PALETTE_RED) : (state.ups.onBattery ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREEN)));
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

void renderM5() {
  lv_obj_clean(m5Tab);
  setupPage(m5Tab);
  lv_obj_t *card = makeCard(m5Tab, "M5Stack / System");
  addBadge(card, state.m5stack.available ? "M5 OK" : "M5 DOWN", severityColor(state.m5stack.severity));
  char line[96];
  snprintf(line, sizeof(line), "Temp: %.1f C   RAM free: %d MB", state.m5stack.temperatureC, state.m5stack.ramAvailableMb);
  addLine(card, line);
  snprintf(line, sizeof(line), "Disk free: %.1f GB", state.m5stack.diskFreeGb);
  addLine(card, line);
  snprintf(line, sizeof(line), "StackFlow: %s   OpenAI: %s", state.m5stack.stackflowOk ? "OK" : "FAIL", state.m5stack.openaiOk ? "OK" : "FAIL");
  addLine(card, line);
  snprintf(line, sizeof(line), "Chat smoke: %s", state.m5stack.chatSmokeOk ? "OK" : "FAIL");
  addLine(card, line);
}

void renderOffline() {
  lv_obj_clean(offlineTab);
  setupPage(offlineTab);
  lv_obj_t *card = makeCard(offlineTab, "Offline / Stale Fallback");
  addBadge(card, state.offline ? "DEMO/OFFLINE" : "LIVE", state.offline ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREEN));
  char line[160];
  snprintf(line, sizeof(line), "Schema: %s", state.schema);
  addLine(card, line);
  snprintf(line, sizeof(line), "Timestamp: %s", state.timestamp);
  addLine(card, line);
  snprintf(line, sizeof(line), "Backend: %s", state.offline ? "sample data or unreachable" : POWER_SENTINEL_SUMMARY_URL);
  addLine(card, line);
  snprintf(line, sizeof(line), "Problems: %s", state.problems);
  addLine(card, line);
}

void renderAll() {
  renderUps();
  renderHa();
  renderProxmox();
  renderM5();
  renderOffline();
}

bool wifiConfigured() {
  return strlen(WIFI_SSID) > 0 && strcmp(WIFI_SSID, "your-ssid") != 0;
}

void connectWiFi() {
  if (!wifiConfigured()) {
    Serial.println("WiFi not configured; using sample data.");
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
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  http.setTimeout(5000);
  if (!http.begin(POWER_SENTINEL_SUMMARY_URL)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("HTTP summary failed: %d\n", code);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();
  parseSummary(body, true);
  return true;
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

void initUi() {
  tabview = lv_tabview_create(lv_screen_active());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_TOP);
  lv_tabview_set_tab_bar_size(tabview, 34);
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x0b0f17), 0);
  upsTab = lv_tabview_add_tab(tabview, "UPS");
  haTab = lv_tabview_add_tab(tabview, "HA");
  proxmoxTab = lv_tabview_add_tab(tabview, "PVE");
  m5Tab = lv_tabview_add_tab(tabview, "M5");
  offlineTab = lv_tabview_add_tab(tabview, "Offline");
  renderAll();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  // CoreS3 stacked 5V bus direction is deliberately configurable:
  // - false: accept power from the LLM Mate/Module/base without feeding 5V back.
  // - true:  feed 5V from CoreS3 USB-C to the M-Bus/stack.
  // The stock firmware appears to support both directions dynamically; M5Unified
  // exposes this as one explicit output-enable bit, so the Power Sentinel firmware
  // chooses the safe external-input mode by default.
#if POWER_SENTINEL_STACK_POWER_OUT
  cfg.output_power = true;
#else
  cfg.output_power = false;
#endif
  M5.begin(cfg);
  M5.Power.setChargeCurrent(200);
  M5.Display.setRotation(1);
  M5.Display.setBrightness(160);

  parseSummary(samplePayload(), false);
  initLvgl();
  initUi();
  connectWiFi();
  if (fetchSummary()) {
    state.offline = false;
    renderAll();
  }
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
    bool ok = fetchSummary();
    state.offline = !ok;
    renderAll();
  }
}

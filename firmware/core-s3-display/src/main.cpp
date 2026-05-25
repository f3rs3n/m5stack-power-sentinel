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

// Sidebar icon font: `ps_ui_tab_12` is a tiny generated LVGL bitmap font
// containing Montserrat ASCII plus five Symbols Nerd Font / FontAwesome PUA
// glyphs. Keep this set synchronized with the LVGL MCP fixture and regenerate
// with `scripts/generate-lvgl-icon-font.sh` if these codepoints change.
LV_FONT_DECLARE(ps_ui_tab_12);
LV_FONT_DECLARE(ps_ui_tab_18);
#define PS_ICON_SETTINGS "\xEF\x80\x93"       // U+F013 fa-gear
#define PS_ICON_HOME "\xEF\x80\x95"           // U+F015 fa-home
#define PS_ICON_NUT "\xF3\xB0\x9B\xB8"        // U+F06F8 nf-md-nut
#define PS_ICON_HOME_ASSISTANT "\xF3\xB0\x9F\x90"  // U+F07D0 nf-md-home_assistant
#define PS_ICON_SERVER "\xEF\x88\xB3"         // U+F233 fa-server

#ifndef POWER_SENTINEL_FIRMWARE_BUILD
#define POWER_SENTINEL_FIRMWARE_BUILD "stackflow-2026-05-23-pve-polish"
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
  int chatSmokeState = -1;
  int updatesAvailableCount = 0;
  char severity[12] = "unknown";
  char shutdownState[20] = "unknown";
  float temperatureC = 0.0f;
  int ramAvailableMb = -1;
  float diskFreeGb = 0.0f;
};

constexpr int MAX_PVE_WORKLOAD_CARDS = 6;
constexpr int MAX_NUT_CLIENT_CARDS = 6;

struct WorkloadMetric {
  char kind[4] = "VM";
  char name[28] = "none";
  int cpuPercent = -1;
  int ramPercent = -1;
  int diskPercent = -1;
  float ramTotalGb = 0.0f;
  float diskTotalGb = 0.0f;
};

struct NutClientCard {
  char role[10] = "secondary";
  char name[24] = "none";
  char state[28] = "unknown";
  bool armed = false;
  bool available = false;
  int lastSeenSeconds = -1;
};

struct ProxmoxState {
  bool available = false;
  char severity[12] = "unknown";
  char shutdownState[20] = "unknown";
  char node[32] = "unknown";
  char nodeStatus[20] = "unknown";
  char zfsStatus[20] = "UNKNOWN";
  char smartStatus[20] = "UNKNOWN";
  char vmNames[96] = "none";
  char lxcNames[96] = "none";
  int apiLatencyMs = -1;
  int cpuPercent = -1;
  int ramPercent = -1;
  int storagePercent = -1;
  float ramTotalGb = 0.0f;
  float storageTotalGb = 0.0f;
  char networkIfaces[64] = "none";
  char networkIface1[16] = "";
  char networkIface2[16] = "";
  char networkIface3[16] = "";
  int vmRunningCount = -1;
  int lxcRunningCount = -1;
  int workloadMetricCount = 0;
  WorkloadMetric workloads[MAX_PVE_WORKLOAD_CARDS];
  float cpuTempC = 0.0f;
};

struct Zigbee2MqttState {
  bool available = false;
  char severity[12] = "unknown";
  char stateText[20] = "unknown";
  char version[24] = "unknown";
  bool coordinatorAvailable = false;
  char coordinatorType[32] = "unknown";
  char coordinatorFirmware[32] = "unknown";
  int deviceTotal = -1;
  int deviceInterviewed = -1;
  int deviceDisabled = -1;
};

struct ShutdownState {
  bool armed = false;
  bool wouldShutdown = false;
  bool primaryReady = false;
  bool primaryMonitorActive = false;
  bool secondaryReady = false;
  char nutUpsmonState[16] = "disarmed";
  char nutUpsmonLabel[16] = "DISARMED";
  char clientState[28] = "not_configured";
  char clientName[24] = "none";
  int clientTotal = 0;
  int clientSecondaryTotal = 0;
  int clientConnected = 0;
  int clientArmed = 0;
  int clientUnknown = 0;
  int clientUnavailable = 0;
  int nutClientCardCount = 0;
  NutClientCard nutClients[MAX_NUT_CLIENT_CARDS];
  bool proxmoxClientArmed = false;
  bool proxmoxClientConnectedAsUpsmon = false;
  char proxmoxClientState[28] = "not_configured";
  int clientPackageInstalled = -1;
  int clientReachableViaUpsc = -1;
  bool clientConnectedAsUpsmon = false;
  char owner[16] = "upsmon";
  char reason[96] = "waiting";
  int chargeLowPercent = -1;
  int runtimeLowSeconds = -1;
};

struct NetworkState {
  bool available = false;
  bool defaultRoute = false;
  char severity[12] = "unknown";
  char probe[12] = "tcp";
  char target[32] = "unknown";
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
  ProxmoxState proxmox;
  Zigbee2MqttState zigbee2mqtt;
  ShutdownState shutdown;
  NetworkState network;
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
bool displayAsleep = false;

constexpr uint8_t kDisplayAwakeBrightness = 160;

lv_color_t buf1[kScreenW * 24];
lv_color_t buf2[kScreenW * 24];
lv_display_t *display = nullptr;
lv_indev_t *indev = nullptr;

lv_obj_t *tabview = nullptr;
lv_obj_t *homeTab = nullptr;
uint32_t renderedTabIndex = UINT32_MAX;
int32_t tabScrollX[5] = {0, 0, 0, 0, 0};

constexpr int PAGE_CARD_WIDTH = 252;
constexpr int PAGE_CARD_HEIGHT = 220;
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

int jsonTriBool(JsonVariantConst v) {
  if (v.isNull()) return -1;
  return v.as<bool>() ? 1 : 0;
}

const char *triText(int value) {
  if (value < 0) return "unknown";
  return value > 0 ? "yes" : "no";
}

float jsonBytesToGb(JsonVariantConst v) {
  if (v.isNull()) return 0.0f;
  return v.as<float>() / 1073741824.0f;
}

void parseProxmoxWorkloadItems(JsonArrayConst items, const char *kind) {
  if (items.isNull()) return;
  for (JsonObjectConst item : items) {
    if (state.proxmox.workloadMetricCount >= MAX_PVE_WORKLOAD_CARDS) return;
    WorkloadMetric &metric = state.proxmox.workloads[state.proxmox.workloadMetricCount++];
    safeCopy(metric.kind, sizeof(metric.kind), kind);
    safeCopy(metric.name, sizeof(metric.name), item["name"] | "unknown");
    metric.cpuPercent = jsonInt(item["cpu_percent"], -1);
    metric.ramPercent = jsonInt(item["ram_percent"], -1);
    metric.diskPercent = jsonInt(item["disk_percent"], -1);
    metric.ramTotalGb = jsonBytesToGb(item["ram_total_bytes"]);
    metric.diskTotalGb = jsonBytesToGb(item["disk_total_bytes"]);
  }
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
  safeCopy(state.ha.shutdownState, sizeof(state.ha.shutdownState), ha["status"] | "unknown");
  JsonObjectConst haUpdates = ha["updates"].as<JsonObjectConst>();
  state.ha.updatesAvailableCount = jsonInt(haUpdates["available_count"], 0);

  JsonObjectConst px = doc["proxmox"].as<JsonObjectConst>();
  state.proxmox.available = px["available"] | false;
  safeCopy(state.proxmox.severity, sizeof(state.proxmox.severity), px["severity"] | "warn");
  safeCopy(state.proxmox.shutdownState, sizeof(state.proxmox.shutdownState), px["shutdown_state"] | "unknown");
  safeCopy(state.proxmox.node, sizeof(state.proxmox.node), px["node"] | "unknown");
  safeCopy(state.proxmox.nodeStatus, sizeof(state.proxmox.nodeStatus), px["node_status"] | "unknown");
  state.proxmox.apiLatencyMs = jsonInt(px["api_latency_ms"], -1);
  state.proxmox.cpuPercent = jsonInt(px["cpu_percent"], -1);
  state.proxmox.ramPercent = jsonInt(px["ram_percent"], -1);
  state.proxmox.ramTotalGb = jsonBytesToGb(px["ram_total_bytes"]);
  state.proxmox.storagePercent = jsonInt(px["storage_percent"], -1);
  state.proxmox.storageTotalGb = jsonBytesToGb(px["storage_total_bytes"]);
  JsonArrayConst ifaces = px["active_network_interfaces"].as<JsonArrayConst>();
  safeCopy(state.proxmox.networkIface1, sizeof(state.proxmox.networkIface1), "");
  safeCopy(state.proxmox.networkIface2, sizeof(state.proxmox.networkIface2), "");
  safeCopy(state.proxmox.networkIface3, sizeof(state.proxmox.networkIface3), "");
  if (ifaces.isNull() || ifaces.size() == 0) {
    safeCopy(state.proxmox.networkIfaces, sizeof(state.proxmox.networkIfaces), "none");
  } else {
    String joined;
    int ifaceIndex = 0;
    for (JsonVariantConst item : ifaces) {
      const char *iface = item.as<const char *>();
      if (!iface || !iface[0]) continue;
      if (joined.length() > 0) joined += ", ";
      joined += iface;
      if (ifaceIndex == 0) safeCopy(state.proxmox.networkIface1, sizeof(state.proxmox.networkIface1), iface);
      if (ifaceIndex == 1) safeCopy(state.proxmox.networkIface2, sizeof(state.proxmox.networkIface2), iface);
      if (ifaceIndex == 2) safeCopy(state.proxmox.networkIface3, sizeof(state.proxmox.networkIface3), iface);
      ifaceIndex++;
    }
    safeCopy(state.proxmox.networkIfaces, sizeof(state.proxmox.networkIfaces), joined.length() > 0 ? joined.c_str() : "none");
  }
  state.proxmox.cpuTempC = jsonFloat(px["cpu_temp_c"], 0);
  JsonObjectConst zfs = px["zfs"].as<JsonObjectConst>();
  safeCopy(state.proxmox.zfsStatus, sizeof(state.proxmox.zfsStatus), zfs["status"] | "UNKNOWN");
  JsonObjectConst smart = px["smart"].as<JsonObjectConst>();
  safeCopy(state.proxmox.smartStatus, sizeof(state.proxmox.smartStatus), smart["status"] | "UNKNOWN");
  state.proxmox.workloadMetricCount = 0;
  JsonObjectConst vm = px["vm"].as<JsonObjectConst>();
  state.proxmox.vmRunningCount = jsonInt(vm["running_count"], -1);
  JsonArrayConst vmNames = vm["running_names"].as<JsonArrayConst>();
  if (vmNames.isNull() || vmNames.size() == 0) {
    safeCopy(state.proxmox.vmNames, sizeof(state.proxmox.vmNames), "none");
  } else {
    String joined;
    for (JsonVariantConst item : vmNames) {
      if (joined.length() > 0) joined += ", ";
      joined += item.as<const char *>();
    }
    safeCopy(state.proxmox.vmNames, sizeof(state.proxmox.vmNames), joined.c_str());
  }
  parseProxmoxWorkloadItems(vm["running_items"].as<JsonArrayConst>(), "VM");
  JsonObjectConst lxc = px["lxc"].as<JsonObjectConst>();
  state.proxmox.lxcRunningCount = jsonInt(lxc["running_count"], -1);
  JsonArrayConst lxcNames = lxc["running_names"].as<JsonArrayConst>();
  if (lxcNames.isNull() || lxcNames.size() == 0) {
    safeCopy(state.proxmox.lxcNames, sizeof(state.proxmox.lxcNames), "none");
  } else {
    String joined;
    for (JsonVariantConst item : lxcNames) {
      if (joined.length() > 0) joined += ", ";
      joined += item.as<const char *>();
    }
    safeCopy(state.proxmox.lxcNames, sizeof(state.proxmox.lxcNames), joined.c_str());
  }
  parseProxmoxWorkloadItems(lxc["running_items"].as<JsonArrayConst>(), "LXC");

  JsonObjectConst sd = doc["shutdown"].as<JsonObjectConst>();
  state.shutdown.armed = sd["armed"] | false;
  state.shutdown.wouldShutdown = sd["would_shutdown"] | false;
  state.shutdown.primaryReady = sd["primary_ready"] | false;
  state.shutdown.primaryMonitorActive = sd["primary_monitor_active"] | false;
  state.shutdown.secondaryReady = sd["secondary_ready"] | false;
  safeCopy(state.shutdown.owner, sizeof(state.shutdown.owner), sd["real_shutdown_owner"] | "upsmon");
  safeCopy(state.shutdown.reason, sizeof(state.shutdown.reason), sd["reason"] | "waiting");
  JsonObjectConst nutUpsmon = sd["nut_upsmon"].as<JsonObjectConst>();
  if (!nutUpsmon.isNull()) {
    safeCopy(state.shutdown.nutUpsmonState, sizeof(state.shutdown.nutUpsmonState), nutUpsmon["state"] | (state.shutdown.armed ? "armed" : "disarmed"));
    safeCopy(state.shutdown.nutUpsmonLabel, sizeof(state.shutdown.nutUpsmonLabel), nutUpsmon["label"] | (state.shutdown.armed ? "ARMED" : "DISARMED"));
  } else {
    safeCopy(state.shutdown.nutUpsmonState, sizeof(state.shutdown.nutUpsmonState), state.shutdown.armed ? "armed" : "disarmed");
    safeCopy(state.shutdown.nutUpsmonLabel, sizeof(state.shutdown.nutUpsmonLabel), state.shutdown.armed ? "ARMED" : "DISARMED");
  }
  JsonObjectConst thresholds = sd["thresholds"].as<JsonObjectConst>();
  state.shutdown.chargeLowPercent = jsonInt(thresholds["battery_charge_low_percent"], -1);
  state.shutdown.runtimeLowSeconds = jsonInt(thresholds["battery_runtime_low_seconds"], -1);
  JsonObjectConst clientSummary = sd["nut_client_summary"].as<JsonObjectConst>();
  state.shutdown.clientTotal = jsonInt(clientSummary["total"], 0);
  state.shutdown.clientSecondaryTotal = jsonInt(clientSummary["secondary_total"], 0);
  state.shutdown.clientConnected = jsonInt(clientSummary["connected"], 0);
  state.shutdown.clientArmed = jsonInt(clientSummary["armed"], 0);
  state.shutdown.clientUnknown = jsonInt(clientSummary["unknown"], 0);
  state.shutdown.clientUnavailable = jsonInt(clientSummary["unavailable"], 0);
  JsonObjectConst pveNutClient = sd["proxmox_nut_client"].as<JsonObjectConst>();
  if (!pveNutClient.isNull()) {
    state.shutdown.proxmoxClientArmed = pveNutClient["armed"] | false;
    state.shutdown.proxmoxClientConnectedAsUpsmon = pveNutClient["connected_as_upsmon"] | false;
    safeCopy(state.shutdown.proxmoxClientState, sizeof(state.shutdown.proxmoxClientState), pveNutClient["state"] | "not_configured");
  } else {
    state.shutdown.proxmoxClientArmed = false;
    state.shutdown.proxmoxClientConnectedAsUpsmon = false;
    safeCopy(state.shutdown.proxmoxClientState, sizeof(state.shutdown.proxmoxClientState), "not_configured");
  }
  state.shutdown.nutClientCardCount = 0;
  JsonArrayConst nutClients = sd["nut_clients"].as<JsonArrayConst>();
  if (!nutClients.isNull() && nutClients.size() > 0) {
    for (JsonObjectConst client : nutClients) {
      if (state.shutdown.nutClientCardCount >= MAX_NUT_CLIENT_CARDS) break;
      NutClientCard &card = state.shutdown.nutClients[state.shutdown.nutClientCardCount++];
      safeCopy(card.role, sizeof(card.role), client["role"] | "secondary");
      safeCopy(card.name, sizeof(card.name), client["name"] | "client");
      safeCopy(card.state, sizeof(card.state), client["state"] | "unknown");
      card.armed = client["armed"] | false;
      card.available = client["available"].isNull() ? true : client["available"].as<bool>();
      card.lastSeenSeconds = jsonInt(client["last_seen_seconds"], -1);
    }
    JsonObjectConst client = nutClients[0].as<JsonObjectConst>();
    safeCopy(state.shutdown.clientName, sizeof(state.shutdown.clientName), client["name"] | "client");
    safeCopy(state.shutdown.clientState, sizeof(state.shutdown.clientState), client["state"] | "not_configured");
    state.shutdown.clientPackageInstalled = jsonTriBool(client["package_installed"]);
    state.shutdown.clientReachableViaUpsc = jsonTriBool(client["reachable_via_upsc"]);
    state.shutdown.clientConnectedAsUpsmon = client["connected_as_upsmon"] | false;
  } else {
    safeCopy(state.shutdown.clientName, sizeof(state.shutdown.clientName), "none");
    safeCopy(state.shutdown.clientState, sizeof(state.shutdown.clientState), "not_configured");
    state.shutdown.clientPackageInstalled = -1;
    state.shutdown.clientReachableViaUpsc = -1;
    state.shutdown.clientConnectedAsUpsmon = false;
  }

  JsonObjectConst z2m = doc["zigbee2mqtt"].as<JsonObjectConst>();
  state.zigbee2mqtt.available = z2m["available"] | false;
  safeCopy(state.zigbee2mqtt.severity, sizeof(state.zigbee2mqtt.severity), z2m["severity"] | "warn");
  safeCopy(state.zigbee2mqtt.stateText, sizeof(state.zigbee2mqtt.stateText), z2m["state"] | "unknown");
  safeCopy(state.zigbee2mqtt.version, sizeof(state.zigbee2mqtt.version), z2m["version"] | "unknown");
  JsonObjectConst coord = z2m["coordinator"].as<JsonObjectConst>();
  state.zigbee2mqtt.coordinatorAvailable = coord["available"] | false;
  safeCopy(state.zigbee2mqtt.coordinatorType, sizeof(state.zigbee2mqtt.coordinatorType), coord["type"] | "unknown");
  safeCopy(state.zigbee2mqtt.coordinatorFirmware, sizeof(state.zigbee2mqtt.coordinatorFirmware), coord["firmware"] | "unknown");
  JsonObjectConst zdev = z2m["devices"].as<JsonObjectConst>();
  state.zigbee2mqtt.deviceTotal = jsonInt(zdev["total"], -1);
  state.zigbee2mqtt.deviceInterviewed = jsonInt(zdev["interviewed"], -1);
  state.zigbee2mqtt.deviceDisabled = jsonInt(zdev["disabled"], -1);

  JsonObjectConst net = doc["network"].as<JsonObjectConst>();
  state.network.available = net["available"] | false;
  state.network.defaultRoute = net["default_route"] | false;
  safeCopy(state.network.severity, sizeof(state.network.severity), net["severity"] | "warn");
  safeCopy(state.network.probe, sizeof(state.network.probe), net["probe"] | "tcp");
  safeCopy(state.network.target, sizeof(state.network.target), net["target"] | "unknown");

  JsonObjectConst m5 = doc["m5stack"].as<JsonObjectConst>();
  state.m5stack.available = m5["available"] | false;
  safeCopy(state.m5stack.severity, sizeof(state.m5stack.severity), m5["severity"] | "warn");
  state.m5stack.temperatureC = jsonFloat(m5["temperature_c"], 0);
  state.m5stack.ramAvailableMb = jsonInt(m5["ram_available_mb"], -1);
  state.m5stack.diskFreeGb = jsonFloat(m5["disk_free_gb"], 0);
  state.m5stack.stackflowOk = m5["stackflow_ok"] | false;
  state.m5stack.openaiOk = m5["openai_ok"] | false;
  state.m5stack.chatSmokeState = m5["chat_smoke_ok"].isNull() ? -1 : (m5["chat_smoke_ok"].as<bool>() ? 1 : 0);

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

void enterDisplaySleep() {
  displayAsleep = true;
  psDisplaySetBrightness(0);
  Serial.println("Display sleep: brightness off; touch anywhere to wake");
}

void wakeDisplay() {
  if (!displayAsleep) return;
  displayAsleep = false;
  psDisplaySetBrightness(kDisplayAwakeBrightness);
  Serial.println("Display wake: brightness restored");
}

void onSleepButtonClicked(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) return;
  enterDisplaySleep();
}

void myTouchRead(lv_indev_t *, lv_indev_data_t *data) {
  int32_t x = 0;
  int32_t y = 0;
  if (psTouchPressed(&x, &y)) {
    if (displayAsleep) {
      wakeDisplay();
      data->state = LV_INDEV_STATE_REL;
      return;
    }
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

void stylePanel(lv_obj_t *card, lv_color_t bg, lv_color_t border) {
  if (!card) return;
  lv_obj_set_width(card, PAGE_CARD_WIDTH);
  lv_obj_set_height(card, PAGE_CARD_HEIGHT);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(card, 6, 0);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_set_style_radius(card, 14, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, border, 0);
  lv_obj_set_style_bg_color(card, bg, 0);
  // Shadows are intentionally enabled again after the lazy active-tab renderer
  // proved stable. This build is a performance/stability trial to isolate the
  // cost of LVGL software shadows without recreating all five tabs at once.
  lv_obj_set_style_shadow_width(card, 10, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_60, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_ofs_y(card, 3, 0);
  lv_obj_set_scroll_dir(card, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
}

void addBadge(lv_obj_t *parent, const char *text, lv_color_t color);

lv_obj_t *makeCard(lv_obj_t *parent, const char *title) {
  lv_obj_t *card = lv_obj_create(parent);
  if (!card) return nullptr;
  stylePanel(card, lv_color_hex(0x171b24), lv_color_hex(0x394152));

  lv_obj_t *label = lv_label_create(card);
  if (label) {
    lv_obj_set_style_text_color(label, lv_color_hex(0xe8eefc), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_label_set_text(label, title ? title : "");
  }
  return card;
}

lv_obj_t *makeStatusCard(lv_obj_t *parent, const char *title, const char *status, lv_color_t statusColor) {
  lv_obj_t *card = lv_obj_create(parent);
  if (!card) return nullptr;
  stylePanel(card, lv_color_hex(0x171b24), lv_color_hex(0x394152));
  lv_obj_set_style_pad_gap(card, 5, 0);

  lv_obj_t *header = lv_obj_create(card);
  if (header) {
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_t *label = lv_label_create(header);
    if (label) {
      lv_obj_set_width(label, 160);
      lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
      lv_obj_set_style_text_color(label, lv_color_hex(0xe8eefc), 0);
      lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
      lv_label_set_text(label, title ? title : "");
    }
    addBadge(header, status ? status : "", statusColor);
  }
  return card;
}

lv_obj_t *makeHeroCard(lv_obj_t *parent, const char *title) {
  lv_obj_t *card = lv_obj_create(parent);
  if (!card) return nullptr;
  stylePanel(card, lv_color_hex(0x111827), severityColor(state.severity));
  lv_obj_set_style_pad_all(card, 10, 0);
  lv_obj_set_style_pad_gap(card, 7, 0);

  lv_obj_t *label = lv_label_create(card);
  if (label) {
    lv_label_set_text(label, title ? title : "");
    lv_obj_set_style_text_color(label, lv_color_hex(0xf8fbff), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
  }
  return card;
}

lv_obj_t *addLine(lv_obj_t *parent, const char *text) {
  if (!parent) return nullptr;
  lv_obj_t *label = lv_label_create(parent);
  if (!label) {
    Serial.printf("LVGL: lv_label_create failed in addLine: %.40s\n", text ? text : "");
    return nullptr;
  }
  lv_obj_set_width(label, lv_pct(100));
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(label, lv_color_hex(0xc8d0df), 0);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
  lv_label_set_text(label, text ? text : "");
  return label;
}

void addBadge(lv_obj_t *parent, const char *text, lv_color_t color) {
  if (!parent) return;
  lv_obj_t *badge = lv_label_create(parent);
  if (!badge) return;
  lv_obj_set_style_text_color(badge, lv_color_white(), 0);
  lv_obj_set_style_text_font(badge, &lv_font_montserrat_12, 0);
  lv_obj_set_style_bg_color(badge, color, 0);
  lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(badge, 10, 0);
  lv_obj_set_style_pad_hor(badge, 8, 0);
  lv_obj_set_style_pad_ver(badge, 4, 0);
  lv_label_set_text(badge, text ? text : "");
}

lv_obj_t *addMetricRow(lv_obj_t *parent, const char *label, const char *value) {
  if (!parent) return nullptr;
  lv_obj_t *row = lv_obj_create(parent);
  if (!row) return nullptr;
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *left = lv_label_create(row);
  if (left) {
    lv_obj_set_style_text_color(left, lv_color_hex(0x8fa0b8), 0);
    lv_obj_set_style_text_font(left, &lv_font_montserrat_12, 0);
    lv_label_set_text(left, label ? label : "");
  }

  lv_obj_t *right = lv_label_create(row);
  if (right) {
    lv_obj_set_style_text_color(right, lv_color_hex(0xf8fbff), 0);
    lv_obj_set_style_text_font(right, &lv_font_montserrat_14, 0);
    lv_label_set_text(right, value ? value : "");
  }
  return row;
}

void addStatusPillRow(lv_obj_t *parent, const char *a, lv_color_t ca, const char *b, lv_color_t cb, const char *c, lv_color_t cc) {
  if (!parent) return;
  lv_obj_t *row = lv_obj_create(parent);
  if (!row) return;
  lv_obj_remove_style_all(row);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_gap(row, 4, 0);
  addBadge(row, a, ca);
  addBadge(row, b, cb);
  addBadge(row, c, cc);
}

void addHomeSleepButton(lv_obj_t *parent) {
  if (!parent) return;
  lv_obj_t *button = lv_button_create(parent);
  if (!button) return;
  lv_obj_set_width(button, lv_pct(100));
  lv_obj_set_height(button, 34);
  lv_obj_set_style_radius(button, 12, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x1f2937), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, lv_color_hex(0x4b5563), 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_shadow_opa(button, LV_OPA_TRANSP, 0);
  lv_obj_add_event_cb(button, onSleepButtonClicked, LV_EVENT_CLICKED, nullptr);

  lv_obj_t *label = lv_label_create(button);
  if (label) {
    lv_label_set_text(label, "SLEEP DISPLAY");
    lv_obj_set_style_text_color(label, lv_color_hex(0xe5e7eb), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_center(label);
  }
}

void addPercentBar(lv_obj_t *parent, int value, lv_color_t color) {
  if (!parent) return;
  lv_obj_t *bar = lv_bar_create(parent);
  if (!bar) return;
  lv_obj_set_width(bar, lv_pct(100));
  lv_obj_set_height(bar, 10);
  lv_bar_set_range(bar, 0, 100);
  int clamped = value < 0 ? 0 : (value > 100 ? 100 : value);
  lv_bar_set_value(bar, clamped, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x283041), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
}

const char *gbText(float gb, char *buf, size_t bufSize) {
  if (gb <= 0.0f) return "";
  if (gb >= 1024.0f) {
    snprintf(buf, bufSize, "%.1fTB", gb / 1024.0f);
  } else if (gb >= 10.0f) {
    snprintf(buf, bufSize, "%.0fGB", gb);
  } else {
    snprintf(buf, bufSize, "%.1fGB", gb);
  }
  return buf;
}

void addCompactMetricWithBar(lv_obj_t *parent, const char *label, int percent, const char *total, lv_color_t color, int barHeight = 5) {
  if (!parent) return;
  char percentText[16];
  char leftText[36];
  const char *percentValue = percent < 0 ? "--" : intOrUnknown(percent, percentText, sizeof(percentText), "%");
  snprintf(leftText, sizeof(leftText), "%s %s", label ? label : "", percentValue);

  lv_obj_t *row = lv_obj_create(parent);
  if (row) {
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *left = lv_label_create(row);
    if (left) {
      lv_obj_set_width(left, total && total[0] ? 132 : lv_pct(100));
      lv_label_set_long_mode(left, LV_LABEL_LONG_CLIP);
      lv_obj_set_style_text_color(left, lv_color_hex(0xc8d0df), 0);
      lv_obj_set_style_text_font(left, &lv_font_montserrat_12, 0);
      lv_label_set_text(left, leftText);
    }
    if (total && total[0]) {
      lv_obj_t *right = lv_label_create(row);
      if (right) {
        lv_obj_set_width(right, 68);
        lv_label_set_long_mode(right, LV_LABEL_LONG_CLIP);
        lv_obj_set_style_text_align(right, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_set_style_text_color(right, lv_color_hex(0xf8fbff), 0);
        lv_obj_set_style_text_font(right, &lv_font_montserrat_12, 0);
        lv_label_set_text(right, total);
      }
    }
  }

  lv_obj_t *bar = lv_bar_create(parent);
  if (!bar) return;
  lv_obj_set_width(bar, lv_pct(100));
  lv_obj_set_height(bar, barHeight);
  lv_bar_set_range(bar, 0, 100);
  int clamped = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
  lv_bar_set_value(bar, clamped, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x283041), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, color, LV_PART_INDICATOR);
}

void addMiniWorkloadMetric(lv_obj_t *parent, const char *label, int percent, const char *total, lv_color_t color) {
  addCompactMetricWithBar(parent, label, percent, total, color, 5);
}

lv_obj_t *makeWorkloadMiniCard(lv_obj_t *parent, const WorkloadMetric &metric) {
  lv_obj_t *card = lv_obj_create(parent);
  if (!card) return nullptr;
  lv_obj_set_width(card, lv_pct(100));
  lv_obj_set_height(card, (PAGE_CARD_HEIGHT - 8) / 2);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(card, 6, 0);
  lv_obj_set_style_pad_gap(card, 2, 0);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x394152), 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x171b24), 0);
  lv_obj_set_style_shadow_width(card, 6, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_60, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_ofs_y(card, 2, 0);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

  char title[48];
  snprintf(title, sizeof(title), "%s %s", metric.kind, metric.name);
  lv_obj_t *label = lv_label_create(card);
  if (label) {
    lv_obj_set_width(label, lv_pct(100));
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, lv_color_hex(0xe8eefc), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_label_set_text(label, title);
  }

  char ramTotal[16];
  char diskTotal[16];
  addMiniWorkloadMetric(card, "CPU", metric.cpuPercent, "", lv_palette_main(LV_PALETTE_BLUE));
  addMiniWorkloadMetric(card, "RAM", metric.ramPercent, gbText(metric.ramTotalGb, ramTotal, sizeof(ramTotal)), lv_palette_main(LV_PALETTE_PURPLE));
  addMiniWorkloadMetric(card, "HDD", metric.diskPercent, gbText(metric.diskTotalGb, diskTotal, sizeof(diskTotal)), lv_palette_main(LV_PALETTE_TEAL));
  return card;
}

lv_obj_t *makeWorkloadInfoMiniCard(lv_obj_t *parent, const char *title, const char *detail) {
  lv_obj_t *card = lv_obj_create(parent);
  if (!card) return nullptr;
  lv_obj_set_width(card, lv_pct(100));
  lv_obj_set_height(card, (PAGE_CARD_HEIGHT - 8) / 2);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_set_style_pad_gap(card, 6, 0);
  lv_obj_set_style_radius(card, 12, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x394152), 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x171b24), 0);
  lv_obj_set_style_shadow_width(card, 6, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_60, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_ofs_y(card, 2, 0);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

  lv_obj_t *headline = lv_label_create(card);
  if (headline) {
    lv_obj_set_width(headline, lv_pct(100));
    lv_label_set_long_mode(headline, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(headline, lv_color_hex(0xe8eefc), 0);
    lv_obj_set_style_text_font(headline, &lv_font_montserrat_14, 0);
    lv_label_set_text(headline, title ? title : "No running VM/LXC");
  }
  lv_obj_t *body = lv_label_create(card);
  if (body) {
    lv_obj_set_width(body, lv_pct(100));
    lv_label_set_long_mode(body, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(body, lv_color_hex(0xc8d0df), 0);
    lv_obj_set_style_text_font(body, &lv_font_montserrat_12, 0);
    lv_label_set_text(body, detail ? detail : "workload list is empty");
  }
  return card;
}

const char *nutClientDisplayState(const NutClientCard &client) {
  if (!client.available) return "UNREACHABLE";
  if (client.armed) return "ARMED";
  if (strcmp(client.state, "disarmed") == 0) return "DISARMED";
  if (strcmp(client.state, "reachable_via_upsc") == 0) return "REACHABLE";
  if (strcmp(client.state, "connected_as_upsmon") == 0) return "CONNECTED";
  if (strcmp(client.state, "configured_not_connected") == 0) return "CONFIGURED";
  if (strcmp(client.state, "unknown") == 0) return "UNKNOWN";
  if (strcmp(client.state, "unavailable") == 0) return "UNREACHABLE";
  return "UNKNOWN";
}

lv_color_t nutClientStateColor(const NutClientCard &client) {
  if (!client.available || strcmp(client.state, "unavailable") == 0) return lv_palette_main(LV_PALETTE_RED);
  if (client.armed || strcmp(client.state, "connected_as_upsmon") == 0) return lv_palette_main(LV_PALETTE_GREEN);
  if (strcmp(client.state, "reachable_via_upsc") == 0) return lv_palette_main(LV_PALETTE_BLUE);
  if (strcmp(client.state, "configured_not_connected") == 0 || strcmp(client.state, "disarmed") == 0) return lv_palette_main(LV_PALETTE_ORANGE);
  return lv_palette_main(LV_PALETTE_GREY);
}

lv_obj_t *makeNutClientMiniCard(lv_obj_t *parent, const NutClientCard &client) {
  lv_obj_t *card = lv_obj_create(parent);
  if (!card) return nullptr;
  lv_obj_set_width(card, lv_pct(100));
  lv_obj_set_height(card, 44);
  lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(card, 6, 0);
  lv_obj_set_style_pad_gap(card, 4, 0);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, lv_color_hex(0x394152), 0);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x171b24), 0);
  lv_obj_set_style_shadow_width(card, 4, 0);
  lv_obj_set_style_shadow_opa(card, LV_OPA_40, 0);
  lv_obj_set_style_shadow_color(card, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_ofs_y(card, 1, 0);
  lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

  char title[48];
  snprintf(title, sizeof(title), "%s %s", strcmp(client.role, "primary") == 0 ? "PRIMARY" : "SECONDARY", client.name);
  lv_obj_t *headline = lv_label_create(card);
  if (headline) {
    lv_obj_set_width(headline, 132);
    lv_label_set_long_mode(headline, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(headline, lv_color_hex(0xe8eefc), 0);
    lv_obj_set_style_text_font(headline, &lv_font_montserrat_14, 0);
    lv_label_set_text(headline, title);
  }
  addBadge(card, nutClientDisplayState(client), nutClientStateColor(client));
  return card;
}

void setupPage(lv_obj_t *tab) {
  lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_all(tab, 8, 0);
  lv_obj_set_style_pad_gap(tab, 8, 0);
  lv_obj_set_scroll_dir(tab, LV_DIR_HOR);
  lv_obj_set_scroll_snap_x(tab, LV_SCROLL_SNAP_CENTER);
  lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_clear_flag(tab, LV_OBJ_FLAG_SCROLL_CHAIN_HOR);
  lv_obj_add_flag(tab, LV_OBJ_FLAG_SCROLL_CHAIN_VER);
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

const char *severityText() {
  if (strcmp(state.severity, "critical") == 0) return "CRITICAL";
  if (strcmp(state.severity, "warn") == 0) return "WARN";
  if (strcmp(state.severity, "ok") == 0) return "OK";
  return "UNKNOWN";
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
  lv_obj_set_style_pad_all(homeTab, 4, 0);
  lv_obj_set_style_pad_gap(homeTab, 4, 0);

  lv_obj_t *card = lv_obj_create(homeTab);
  stylePanel(card, lv_color_hex(0x111827), severityColor(state.severity));
  lv_obj_set_height(card, PAGE_CARD_HEIGHT);
  lv_obj_set_style_pad_all(card, 7, 0);
  lv_obj_set_style_pad_gap(card, 4, 0);

  lv_obj_t *header = lv_obj_create(card);
  lv_obj_remove_style_all(header);
  lv_obj_set_width(header, lv_pct(100));
  lv_obj_set_height(header, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "POWER SENTINEL");
  lv_obj_set_width(title, 215);
  lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(title, lv_color_hex(0xf8fbff), 0);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
  addBadge(header, state.offline ? "STALE" : severityText(), state.offline ? lv_palette_main(LV_PALETTE_ORANGE) : severityColor(state.severity));

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
  snprintf(pvePill, sizeof(pvePill), "PVE %s", state.proxmox.available ? "online" : "offline");
  char haPill[16];
  snprintf(haPill, sizeof(haPill), "HA %s", haFunctional() ? "online" : "offline");
  addStatusPillRow(card, nutPill, state.ups.onBattery ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREEN),
                   pvePill, state.proxmox.available ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED),
                   haPill, haFunctional() ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));

  lv_obj_t *bottom = lv_obj_create(homeTab);
  stylePanel(bottom, lv_color_hex(0x171b24), lv_color_hex(0x394152));
  lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(bottom, 8, 0);

  lv_obj_t *bottomTitle = lv_label_create(bottom);
  lv_label_set_text(bottomTitle, "Local controls");
  lv_obj_set_style_text_color(bottomTitle, lv_color_hex(0xe8eefc), 0);
  lv_obj_set_style_text_font(bottomTitle, &lv_font_montserrat_16, 0);

  lv_obj_t *local = lv_obj_create(bottom);
  lv_obj_set_width(local, lv_pct(100));
  lv_obj_set_height(local, 82);
  lv_obj_set_flex_flow(local, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_gap(local, 2, 0);
  lv_obj_set_style_pad_all(local, 6, 0);
  lv_obj_set_style_radius(local, 12, 0);
  lv_obj_set_style_border_width(local, 1, 0);
  lv_obj_set_style_border_color(local, lv_color_hex(0x394152), 0);
  lv_obj_set_style_bg_color(local, lv_color_hex(0x171b24), 0);
  lv_obj_set_style_bg_opa(local, LV_OPA_COVER, 0);

  snprintf(line, sizeof(line), "local NET %s M5S %s", state.network.available ? "OK" : "DOWN", state.m5stack.available ? "OK" : "DOWN");
  lv_obj_t *localValue = lv_label_create(local);
  lv_label_set_text(localValue, line);
  lv_obj_set_width(localValue, lv_pct(100));
  lv_label_set_long_mode(localValue, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(localValue, lv_color_hex(0xf8fbff), 0);
  lv_obj_set_style_text_font(localValue, &lv_font_montserrat_14, 0);
  snprintf(line, sizeof(line), "Problems: %s", state.problems);
  lv_obj_t *problemValue = lv_label_create(local);
  lv_label_set_text(problemValue, line);
  lv_obj_set_width(problemValue, lv_pct(100));
  lv_label_set_long_mode(problemValue, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_color(problemValue, lv_color_hex(0xc8d0df), 0);
  lv_obj_set_style_text_font(problemValue, &lv_font_montserrat_12, 0);

  lv_obj_t *button = lv_button_create(bottom);
  lv_obj_set_width(button, lv_pct(100));
  lv_obj_set_height(button, 56);
  lv_obj_set_style_radius(button, 12, 0);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x1f2937), 0);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(button, 1, 0);
  lv_obj_set_style_border_color(button, lv_color_hex(0x4b5563), 0);
  lv_obj_add_event_cb(button, onSleepButtonClicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *sleepLabel = lv_label_create(button);
  lv_label_set_text(sleepLabel, "SLEEP DISPLAY");
  lv_label_set_long_mode(sleepLabel, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sleepLabel, lv_pct(100));
  lv_obj_set_style_text_align(sleepLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_color(sleepLabel, lv_color_hex(0xe5e7eb), 0);
  lv_obj_set_style_text_font(sleepLabel, &lv_font_montserrat_14, 0);
  lv_obj_center(sleepLabel);
}

const char *batteryStatusBadge() {
  if (state.ups.lowBattery) return "LOW";
  if (state.ups.onBattery) return "DISCHARGING";
  if (state.ups.available) return "CHARGING";
  return "UNKNOWN";
}

lv_color_t batteryStatusColor() {
  if (state.ups.lowBattery) return lv_palette_main(LV_PALETTE_RED);
  if (state.ups.onBattery) return lv_palette_main(LV_PALETTE_ORANGE);
  if (state.ups.available) return lv_palette_main(LV_PALETTE_GREEN);
  return lv_palette_main(LV_PALETTE_GREY);
}

void renderNut() {
  lv_obj_clean(nutTab);
  setupPage(nutTab);
  const char *upsBadge = state.offline ? "STALE" : (!state.ups.available ? "UNAVAILABLE" : (state.ups.lowBattery ? "LOW BATT" : (state.ups.onBattery ? "ON BATT" : "ONLINE")));
  lv_color_t upsColor = state.offline ? lv_palette_main(LV_PALETTE_ORANGE) :
                        (!state.ups.available ? lv_palette_main(LV_PALETTE_GREY) :
                         (state.ups.lowBattery ? lv_palette_main(LV_PALETTE_RED) :
                          (state.ups.onBattery ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREEN))));
  char line[128];
  char battery[24];
  char runtime[24];
  char load[24];
  char watts[24];
  char inputV[24];
  char batteryV[24];

  lv_obj_t *ups = makeStatusCard(nutTab, "UPS", upsBadge, upsColor);
  snprintf(line, sizeof(line), "Model %s", state.ups.model);
  addLine(ups, line);
  snprintf(line, sizeof(line), "Batt %s   Runtime %s",
           intOrUnknown(state.ups.batteryPercent, battery, sizeof(battery), "%"),
           runtimeText(state.ups.runtimeSeconds, runtime, sizeof(runtime)));
  addLine(ups, line);
  addPercentBar(ups, state.ups.batteryPercent, state.ups.lowBattery ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN));
  snprintf(line, sizeof(line), "Load %s   Power %s",
           intOrUnknown(state.ups.loadPercent, load, sizeof(load), "%"),
           intOrUnknown(state.ups.loadW, watts, sizeof(watts), "W"));
  addLine(ups, line);
  addPercentBar(ups, state.ups.loadPercent, lv_palette_main(LV_PALETTE_BLUE));
  snprintf(line, sizeof(line), "Input %s", floatOrUnknown(state.ups.inputVoltage, inputV, sizeof(inputV), "V"));
  addLine(ups, line);

  lv_obj_t *batteryCard = makeStatusCard(nutTab, "BATTERY", batteryStatusBadge(), batteryStatusColor());
  snprintf(line, sizeof(line), "Battery Charge %s", intOrUnknown(state.ups.batteryPercent, battery, sizeof(battery), "%"));
  addLine(batteryCard, line);
  addPercentBar(batteryCard, state.ups.batteryPercent, state.ups.lowBattery ? lv_palette_main(LV_PALETTE_RED) : lv_palette_main(LV_PALETTE_GREEN));
  snprintf(line, sizeof(line), "Runtime Remaining %s", runtimeText(state.ups.runtimeSeconds, runtime, sizeof(runtime)));
  addLine(batteryCard, line);
  snprintf(line, sizeof(line), "Battery Voltage %s", floatOrUnknown(state.ups.batteryVoltage, batteryV, sizeof(batteryV), "V"));
  addLine(batteryCard, line);

  lv_obj_t *powerCard = makeCard(nutTab, "POWER");
  snprintf(line, sizeof(line), "Power Usage %s", intOrUnknown(state.ups.loadW, watts, sizeof(watts), "W"));
  addLine(powerCard, line);
  snprintf(line, sizeof(line), "System Load %s", intOrUnknown(state.ups.loadPercent, load, sizeof(load), "%"));
  addLine(powerCard, line);
  addPercentBar(powerCard, state.ups.loadPercent, lv_palette_main(LV_PALETTE_BLUE));
  snprintf(line, sizeof(line), "Input Voltage %s", floatOrUnknown(state.ups.inputVoltage, inputV, sizeof(inputV), "V"));
  addLine(powerCard, line);
  if (state.ups.outputVoltage > 0.0f) {
    char outputV[24];
    snprintf(line, sizeof(line), "Output Voltage %s", floatOrUnknown(state.ups.outputVoltage, outputV, sizeof(outputV), "V"));
    addLine(powerCard, line);
  }
  snprintf(line, sizeof(line), "Nominal Power %s", intOrUnknown(state.ups.realpowerNominalW, watts, sizeof(watts), "W"));
  addLine(powerCard, line);

  lv_obj_t *protection = makeStatusCard(nutTab, "PROTECTION", state.shutdown.clientArmed > 0 ? "ARMED" : "DISARMED", state.shutdown.clientArmed > 0 ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_ORANGE));
  snprintf(line, sizeof(line), "Connected clients %d/%d", state.shutdown.clientConnected, state.shutdown.clientTotal);
  addLine(protection, line);
  snprintf(line, sizeof(line), "NUT services: upsd %s driver %s", okDown(state.nut.serverActive), okDown(state.nut.driverActive));
  addLine(protection, line);
  if (state.shutdown.nutClientCardCount == 0) {
    addLine(protection, "Client data unavailable");
  } else {
    for (int i = 0; i < state.shutdown.nutClientCardCount; ++i) {
      const NutClientCard &client = state.shutdown.nutClients[i];
      makeNutClientMiniCard(protection, client);
    }
  }
}

void renderHa() {
  lv_obj_clean(haTab);
  setupPage(haTab);
  lv_obj_t *card = makeStatusCard(haTab, "HOME ASSISTANT", state.ha.available ? "ONLINE" : "OFFLINE", state.ha.available ? severityColor(state.ha.severity) : lv_palette_main(LV_PALETTE_RED));
  char line[128];
  snprintf(line, sizeof(line), "API %s   MQTT %s", state.ha.available ? "OK" : "DOWN", state.ha.mqtt ? "OK" : "DOWN");
  addMetricRow(card, "core", line);
  snprintf(line, sizeof(line), "Updates %d", state.ha.updatesAvailableCount);
  addMetricRow(card, "updates", line);

  lv_obj_t *z2m = makeCard(haTab, "Z2M / Coordinator");
  addBadge(z2m, state.zigbee2mqtt.available ? "Z2M OK" : "Z2M WARN", severityColor(state.zigbee2mqtt.severity));
  snprintf(line, sizeof(line), "State %s", state.zigbee2mqtt.stateText);
  addLine(z2m, line);
  snprintf(line, sizeof(line), "Coordinator %s", state.zigbee2mqtt.coordinatorAvailable ? "OK" : "UNKNOWN");
  addMetricRow(z2m, "coordinator", line);
  snprintf(line, sizeof(line), "%s   fw %s", state.zigbee2mqtt.coordinatorType, state.zigbee2mqtt.coordinatorFirmware);
  addLine(z2m, line);
  char total[24];
  char interviewed[24];
  snprintf(line, sizeof(line), "Z2M devices: %s/%s",
           intOrUnknown(state.zigbee2mqtt.deviceInterviewed, interviewed, sizeof(interviewed)),
           intOrUnknown(state.zigbee2mqtt.deviceTotal, total, sizeof(total)));
  addMetricRow(z2m, "devices", line);
  char disabled[24];
  snprintf(line, sizeof(line), "Disabled %s", intOrUnknown(state.zigbee2mqtt.deviceDisabled, disabled, sizeof(disabled)));
  addLine(z2m, line);
}

void renderProxmox() {
  lv_obj_clean(proxmoxTab);
  setupPage(proxmoxTab);
  lv_obj_t *card = makeStatusCard(proxmoxTab, "PROXMOX", state.proxmox.available ? "ONLINE" : "OFFLINE", state.proxmox.available ? severityColor(state.proxmox.severity) : lv_palette_main(LV_PALETTE_RED));

  char line[128];
  char ramTotal[16];
  char storageTotal[16];
  addCompactMetricWithBar(card, "CPU", state.proxmox.cpuPercent, "", lv_palette_main(LV_PALETTE_BLUE), 10);
  addCompactMetricWithBar(card, "RAM", state.proxmox.ramPercent, gbText(state.proxmox.ramTotalGb, ramTotal, sizeof(ramTotal)), lv_palette_main(LV_PALETTE_PURPLE), 10);
  addCompactMetricWithBar(card, "Storage", state.proxmox.storagePercent, gbText(state.proxmox.storageTotalGb, storageTotal, sizeof(storageTotal)), lv_palette_main(LV_PALETTE_TEAL), 10);

  lv_obj_t *healthRow = lv_obj_create(card);
  if (healthRow) {
    lv_obj_remove_style_all(healthRow);
    lv_obj_set_width(healthRow, lv_pct(100));
    lv_obj_set_height(healthRow, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(healthRow, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_gap(healthRow, 4, 0);
    addBadge(healthRow, strcmp(state.proxmox.zfsStatus, "ONLINE") == 0 ? "ZFS online" : "ZFS warn", strcmp(state.proxmox.zfsStatus, "ONLINE") == 0 ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_ORANGE));
    addBadge(healthRow, strcmp(state.proxmox.smartStatus, "PASSED") == 0 || strcmp(state.proxmox.smartStatus, "OK") == 0 ? "SMART ok" : "SMART warn", strcmp(state.proxmox.smartStatus, "PASSED") == 0 || strcmp(state.proxmox.smartStatus, "OK") == 0 ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_ORANGE));
  }

  if (state.proxmox.networkIface1[0]) {
    lv_obj_t *netRow = lv_obj_create(card);
    if (netRow) {
      lv_obj_remove_style_all(netRow);
      lv_obj_set_width(netRow, lv_pct(100));
      lv_obj_set_height(netRow, LV_SIZE_CONTENT);
      lv_obj_set_flex_flow(netRow, LV_FLEX_FLOW_ROW_WRAP);
      lv_obj_set_style_pad_gap(netRow, 4, 0);
      addBadge(netRow, state.proxmox.networkIface1, lv_palette_main(LV_PALETTE_CYAN));
      if (state.proxmox.networkIface2[0]) addBadge(netRow, state.proxmox.networkIface2, lv_palette_main(LV_PALETTE_CYAN));
      if (state.proxmox.networkIface3[0]) addBadge(netRow, state.proxmox.networkIface3, lv_palette_main(LV_PALETTE_CYAN));
    }
  }

  const bool nutClientArmed = state.shutdown.proxmoxClientArmed;
  addBadge(card, nutClientArmed ? "NUT armed" : "NUT disarmed", nutClientArmed ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_ORANGE));

  if (state.proxmox.workloadMetricCount == 0) {
    lv_obj_t *page = lv_obj_create(proxmoxTab);
    if (page) {
      lv_obj_remove_style_all(page);
      lv_obj_set_width(page, PAGE_CARD_WIDTH);
      lv_obj_set_height(page, PAGE_CARD_HEIGHT);
      lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_style_pad_gap(page, 8, 0);
      const int totalRunning = (state.proxmox.vmRunningCount > 0 ? state.proxmox.vmRunningCount : 0) +
                               (state.proxmox.lxcRunningCount > 0 ? state.proxmox.lxcRunningCount : 0);
      if (totalRunning == 0) {
        makeWorkloadInfoMiniCard(page, "No running VM/LXC", "node has no active workloads");
      } else {
        snprintf(line, sizeof(line), "%d running; metrics unavailable", totalRunning);
        makeWorkloadInfoMiniCard(page, "Workload metrics unavailable", line);
      }
    }
  } else {
    for (int i = 0; i < state.proxmox.workloadMetricCount; i += 2) {
      lv_obj_t *page = lv_obj_create(proxmoxTab);
      if (!page) continue;
      lv_obj_remove_style_all(page);
      lv_obj_set_width(page, PAGE_CARD_WIDTH);
      lv_obj_set_height(page, PAGE_CARD_HEIGHT);
      lv_obj_set_flex_flow(page, LV_FLEX_FLOW_COLUMN);
      lv_obj_set_style_pad_gap(page, 8, 0);
      makeWorkloadMiniCard(page, state.proxmox.workloads[i]);
      if (i + 1 < state.proxmox.workloadMetricCount) {
        makeWorkloadMiniCard(page, state.proxmox.workloads[i + 1]);
      }
    }
  }
}

void renderM5s() {
  lv_obj_clean(m5sTab);
  setupPage(m5sTab);
  lv_obj_t *card = makeStatusCard(m5sTab, "M5S", state.m5stack.available ? "ONLINE" : "OFFLINE", state.m5stack.available ? severityColor(state.m5stack.severity) : lv_palette_main(LV_PALETTE_RED));
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
  const char *chatSmokeText = state.m5stack.chatSmokeState < 0 ? "n/a" : (state.m5stack.chatSmokeState > 0 ? "OK" : "FAIL");
  snprintf(line, sizeof(line), "Chat smoke %s", chatSmokeText);
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
  char apiMs[24];
  snprintf(line, sizeof(line), "PVE API %s", intOrUnknown(state.proxmox.apiLatencyMs, apiMs, sizeof(apiMs), "ms"));
  addLine(transport, line);
  snprintf(line, sizeof(line), "FW %s", POWER_SENTINEL_FIRMWARE_BUILD);
  addLine(transport, line);
  snprintf(line, sizeof(line), "UART %d/%d %d", POWER_SENTINEL_UART_RX_PIN, POWER_SENTINEL_UART_TX_PIN, POWER_SENTINEL_UART_BAUD);
  addLine(transport, line);
  snprintf(line, sizeof(line), "Problems: %s", state.problems);
  addLine(transport, line);
}

void cleanInactiveTabs(uint32_t active) {
  lv_obj_t *tabs[] = {homeTab, nutTab, proxmoxTab, haTab, m5sTab};
  for (uint32_t i = 0; i < 5; ++i) {
    if (i != active && tabs[i]) {
      lv_obj_clean(tabs[i]);
    }
  }
}

lv_obj_t *tabForIndex(uint32_t index) {
  switch (index) {
    case 0: return homeTab;
    case 1: return nutTab;
    case 2: return proxmoxTab;
    case 3: return haTab;
    case 4: return m5sTab;
    default: return nullptr;
  }
}

void saveTabScroll(uint32_t index) {
  if (index >= 5) return;
  lv_obj_t *tab = tabForIndex(index);
  if (!tab) return;
  tabScrollX[index] = lv_obj_get_scroll_x(tab);
}

void restoreTabScroll(uint32_t index) {
  if (index >= 5) return;
  lv_obj_t *tab = tabForIndex(index);
  if (!tab) return;
  lv_obj_update_layout(tab);
  lv_obj_scroll_to_x(tab, tabScrollX[index], LV_ANIM_OFF);
}

void renderTab(uint32_t active) {
  saveTabScroll(renderedTabIndex);
  cleanInactiveTabs(active);
  renderedTabIndex = active;
  switch (active) {
    case 0:
      renderHome();
      break;
    case 1:
      renderNut();
      break;
    case 2:
      renderProxmox();
      break;
    case 3:
      renderHa();
      break;
    case 4:
      renderM5s();
      break;
    default:
      renderHome();
      renderedTabIndex = 0;
      break;
  }
  restoreTabScroll(renderedTabIndex);
}

void renderActiveTab() {
  if (!tabview) return;
  renderTab(lv_tabview_get_tab_active(tabview));
}

void onTabChanged(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_VALUE_CHANGED) return;
  renderActiveTab();
}

void renderAll() {
  // Keep the old name for tests/guards, but do not instantiate all five dense
  // pages at once. The CoreS3 LVGL heap cannot safely hold every dashboard page
  // plus live PVE workload mini-cards. Render only the visible tab and clear the
  // inactive tabs so tab switching remains stable under the real StackFlow payload.
  renderActiveTab();
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
  lv_obj_set_style_pad_hor(tabBar, 3, 0);
  lv_obj_set_style_pad_ver(tabBar, 6, 0);
  lv_obj_set_style_pad_gap(tabBar, 6, 0);
  lv_obj_set_style_text_font(tabBar, &ps_ui_tab_18, 0);
  lv_obj_set_style_text_align(tabBar, LV_TEXT_ALIGN_CENTER, LV_PART_ITEMS);
  // Keep inactive sidebar labels bright like the live CoreS3; the selected
  // blue pill already provides the active-tab cue.
  lv_obj_set_style_text_color(tabBar, lv_color_hex(0xf8fbff), LV_PART_ITEMS);
  lv_obj_set_style_text_opa(tabBar, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_opa(tabBar, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_text_color(tabBar, lv_color_hex(0xf8fbff), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_text_opa(tabBar, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_color(tabBar, lv_color_hex(0x0f1b2d), LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(tabBar, LV_OPA_60, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(tabBar, lv_color_hex(0x2563eb), LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_bg_opa(tabBar, LV_OPA_COVER, LV_PART_ITEMS | LV_STATE_CHECKED);
  lv_obj_set_style_radius(tabBar, 12, LV_PART_ITEMS);
}

void forceSidebarLabelContrast(lv_obj_t *tabBar) {
  if (!tabBar) return;
  uint32_t count = lv_obj_get_child_count(tabBar);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t *button = lv_obj_get_child(tabBar, i);
    if (!button) continue;
    lv_obj_set_style_text_color(button, lv_color_hex(0xf8fbff), 0);
    lv_obj_set_style_text_opa(button, LV_OPA_COVER, 0);
    uint32_t childCount = lv_obj_get_child_count(button);
    for (uint32_t j = 0; j < childCount; ++j) {
      lv_obj_t *label = lv_obj_get_child(button, j);
      if (!label) continue;
      lv_obj_set_style_text_color(label, lv_color_hex(0xf8fbff), 0);
      lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    }
  }
}

void initUi() {
  tabview = lv_tabview_create(lv_screen_active());
  lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT);
  lv_tabview_set_tab_bar_size(tabview, 44);
  lv_obj_t *tabContent = lv_tabview_get_content(tabview);
  lv_obj_set_scroll_dir(tabContent, LV_DIR_VER);
  lv_obj_set_scroll_snap_y(tabContent, LV_SCROLL_SNAP_CENTER);
  applyAppTheme();
  homeTab = lv_tabview_add_tab(tabview, PS_ICON_HOME);
  nutTab = lv_tabview_add_tab(tabview, PS_ICON_NUT);
  proxmoxTab = lv_tabview_add_tab(tabview, PS_ICON_SERVER);
  haTab = lv_tabview_add_tab(tabview, PS_ICON_HOME_ASSISTANT);
  m5sTab = lv_tabview_add_tab(tabview, PS_ICON_SETTINGS);
  forceSidebarLabelContrast(lv_tabview_get_tab_bar(tabview));
  lv_obj_add_event_cb(tabview, onTabChanged, LV_EVENT_VALUE_CHANGED, nullptr);
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
  psDisplaySetBrightness(kDisplayAwakeBrightness);

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

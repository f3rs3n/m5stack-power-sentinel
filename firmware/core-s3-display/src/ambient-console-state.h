#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef ARDUINO
#include <Arduino.h>
#include <ArduinoJson.h>
#endif

#include "core-s3-transport-diagnostics.h"

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

inline void ambientConsoleSafeCopy(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

#ifdef ARDUINO
inline int ambientConsoleJsonInt(JsonVariantConst v, int fallback) {
  if (v.isNull()) return fallback;
  if (v.is<int>()) return v.as<int>();
  if (v.is<float>()) return static_cast<int>(v.as<float>());
  if (v.is<const char *>()) return atoi(v.as<const char *>());
  return fallback;
}

inline float ambientConsoleJsonFloat(JsonVariantConst v, float fallback) {
  if (v.isNull()) return fallback;
  if (v.is<float>() || v.is<int>()) return v.as<float>();
  if (v.is<const char *>()) return atof(v.as<const char *>());
  return fallback;
}

inline bool ambientConsoleJsonBool(JsonVariantConst v, bool fallback = false) {
  if (v.isNull()) return fallback;
  if (v.is<bool>()) return v.as<bool>();
  if (v.is<int>()) return v.as<int>() != 0;
  if (v.is<const char *>()) {
    const char *s = v.as<const char *>();
    return strcmp(s, "1") == 0 || strcmp(s, "true") == 0 || strcmp(s, "yes") == 0;
  }
  return fallback;
}
#endif

inline bool ambientConsoleValidHhmm(const char *s) {
  if (!s) return false;
  return s[0] >= '0' && s[0] <= '2' &&
         s[1] >= '0' && s[1] <= '9' &&
         s[2] == ':' &&
         s[3] >= '0' && s[3] <= '5' &&
         s[4] >= '0' && s[4] <= '9' &&
         s[5] == '\0' &&
         !(s[0] == '2' && s[1] > '3');
}

#ifdef ARDUINO
inline bool ambientConsoleParseSummary(SummaryState &state,
                                       CoreS3TransportFailureKind &pendingTransportFailureKind,
                                       const String &json,
                                       const char *source) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.printf("NUT summary JSON parse error: %s\n", err.c_str());
    snprintf(state.transportStatus, sizeof(state.transportStatus), "JSON parse failed: %s", err.c_str());
    pendingTransportFailureKind = CORE_S3_TRANSPORT_FAILURE_JSON_PARSE;
    return false;
  }
  ambientConsoleSafeCopy(state.schema, sizeof(state.schema), doc["schema"] | "power-sentinel.summary.v1");
  ambientConsoleSafeCopy(state.severity, sizeof(state.severity), doc["severity"] | "unknown");
  ambientConsoleSafeCopy(state.source, sizeof(state.source), source);
  JsonObjectConst ups = doc["ups"].as<JsonObjectConst>();
  state.ups.available = ambientConsoleJsonBool(ups["available"], false);
  state.ups.stale = ambientConsoleJsonBool(ups["stale"], !state.ups.available);
  state.ups.onBattery = ambientConsoleJsonBool(ups["on_battery"], false);
  state.ups.lowBattery = ambientConsoleJsonBool(ups["low_battery"], false);
  state.ups.charging = ambientConsoleJsonBool(ups["charging"], false);
  ambientConsoleSafeCopy(state.ups.status, sizeof(state.ups.status), ups["status"] | ups["status_label"] | "unknown");
  state.ups.batteryPercent = ambientConsoleJsonInt(ups["battery_percent"], -1);
  state.ups.runtimeSeconds = ambientConsoleJsonInt(ups["runtime_seconds"], -1);
  state.ups.loadPercent = ambientConsoleJsonInt(ups["load_percent"], -1);
  state.ups.inputVoltage = ambientConsoleJsonFloat(ups["input_voltage"], 0.0f);
  state.ups.ageSeconds = ambientConsoleJsonInt(ups["age_seconds"], -1);
  JsonObjectConst nut = doc["nut"].as<JsonObjectConst>();
  state.nut.clientCount = ambientConsoleJsonInt(nut["client_count"], -1);
  state.nut.wouldShutdown = ambientConsoleJsonBool(nut["would_shutdown"], false);
  JsonObjectConst module = doc["module"].as<JsonObjectConst>();
  state.moduleLanConnected = ambientConsoleJsonBool(module["lan_connected"], false);
  const char *timeHhmm = module["time_hhmm"] | "--:--";
  ambientConsoleSafeCopy(state.moduleTimeHhmm, sizeof(state.moduleTimeHhmm), ambientConsoleValidHhmm(timeHhmm) ? timeHhmm : "--:--");
  JsonObjectConst proxmox = doc["modules"]["proxmox"].as<JsonObjectConst>();
  state.proxmox.enabled = ambientConsoleJsonBool(proxmox["enabled"], false);
  state.proxmox.implemented = ambientConsoleJsonBool(proxmox["implemented"], false);
  ambientConsoleSafeCopy(state.proxmox.condition, sizeof(state.proxmox.condition), proxmox["condition"] | "unavailable");
  ambientConsoleSafeCopy(state.proxmox.status, sizeof(state.proxmox.status), proxmox["status"] | "not_observed");
  JsonObjectConst proxmoxCpuCard = proxmox["cards"]["cpu"].as<JsonObjectConst>();
  state.proxmox.cpuPercent = ambientConsoleJsonInt(proxmoxCpuCard["value_percent"], -1);
  ambientConsoleSafeCopy(state.proxmox.cpuCondition, sizeof(state.proxmox.cpuCondition), proxmoxCpuCard["condition"] | "healthy");
  JsonObjectConst proxmoxRamCard = proxmox["cards"]["ram"].as<JsonObjectConst>();
  state.proxmox.ramPercent = ambientConsoleJsonInt(proxmoxRamCard["value_percent"], -1);
  ambientConsoleSafeCopy(state.proxmox.ramCondition, sizeof(state.proxmox.ramCondition), proxmoxRamCard["condition"] | "healthy");
  JsonObjectConst proxmoxGuestCard = proxmox["cards"]["guests"].as<JsonObjectConst>();
  state.proxmox.guestRunning = ambientConsoleJsonInt(proxmoxGuestCard["running"], -1);
  state.proxmox.guestTotal = ambientConsoleJsonInt(proxmoxGuestCard["total"], -1);
  ambientConsoleSafeCopy(state.proxmox.guestCondition, sizeof(state.proxmox.guestCondition), proxmoxGuestCard["condition"] | "healthy");
  JsonObjectConst proxmoxStorageCard = proxmox["cards"]["storage"].as<JsonObjectConst>();
  state.proxmox.storagePercent = ambientConsoleJsonInt(proxmoxStorageCard["value_percent"], -1);
  ambientConsoleSafeCopy(state.proxmox.storageCondition, sizeof(state.proxmox.storageCondition), proxmoxStorageCard["condition"] | "healthy");
  JsonObjectConst proxmoxNetworkCard = proxmox["cards"]["network"].as<JsonObjectConst>();
  state.proxmox.networkPercent = ambientConsoleJsonInt(proxmoxNetworkCard["value_percent"], -1);
  ambientConsoleSafeCopy(state.proxmox.networkCondition, sizeof(state.proxmox.networkCondition), proxmoxNetworkCard["condition"] | "healthy");
  JsonObjectConst proxmoxEnvironment = proxmox["environment"].as<JsonObjectConst>();
  state.proxmox.nodeCount = ambientConsoleJsonInt(proxmoxEnvironment["node_count"], -1);
  state.proxmox.onlineNodeCount = ambientConsoleJsonInt(proxmoxEnvironment["online_node_count"], -1);
  state.proxmox.guestTotalCount = ambientConsoleJsonInt(proxmoxEnvironment["guest_total_count"], -1);
  state.proxmox.guestRunningCount = ambientConsoleJsonInt(proxmoxEnvironment["guest_running_count"], -1);
  state.proxmox.storageCount = ambientConsoleJsonInt(proxmoxEnvironment["storage_count"], -1);
  state.proxmox.maxStorageUsedPercent = -1;
  JsonArrayConst proxmoxStorage = proxmox["storage"].as<JsonArrayConst>();
  for (JsonObjectConst item : proxmoxStorage) {
    int used = ambientConsoleJsonInt(item["used_percent"], -1);
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
  ambientConsoleSafeCopy(state.proxmox.signalKind, sizeof(state.proxmox.signalKind), selectedProxmoxSignal["kind"] | "");
  ambientConsoleSafeCopy(state.proxmox.signalSummary, sizeof(state.proxmox.signalSummary), selectedProxmoxSignal["summary"] | "");
  JsonObjectConst selectedProxmoxSignalContext = selectedProxmoxSignal["context"].as<JsonObjectConst>();
  const char *signalNode = selectedProxmoxSignalContext["node"] | "";
  const char *signalStorage = selectedProxmoxSignalContext["storage"] | "";
  const char *signalName = selectedProxmoxSignalContext["name"] | "";
  int signalUsedPercent = ambientConsoleJsonInt(selectedProxmoxSignalContext["used_percent"], -1);
  int signalVmid = ambientConsoleJsonInt(selectedProxmoxSignalContext["vmid"], -1);
  if (signalNode[0] != '\0') {
    if (signalStorage[0] != '\0' && signalUsedPercent >= 0) {
      snprintf(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "%s/%s %d%%", signalNode, signalStorage, signalUsedPercent);
    } else {
      ambientConsoleSafeCopy(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), signalNode);
    }
  } else if (signalStorage[0] != '\0' && signalUsedPercent >= 0) {
    snprintf(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "%s %d%%", signalStorage, signalUsedPercent);
  } else if (signalName[0] != '\0' && signalVmid >= 0) {
    snprintf(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "%s %d", signalName, signalVmid);
  } else if (signalVmid >= 0) {
    snprintf(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "VMID %d", signalVmid);
  } else {
    ambientConsoleSafeCopy(state.proxmox.signalContext, sizeof(state.proxmox.signalContext), "");
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
#endif

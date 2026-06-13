#pragma once

#include <stdio.h>
#include <stdint.h>
#include <string.h>

struct ProxmoxAmbientView {
  bool enabled;
  bool implemented;
  bool hasLiveData;
  char condition[16];
  char status[24];
  char signalKind[24];
  char signalSummary[48];
  int nodeCount;
  int onlineNodeCount;
  int watchedGuestCount;
  int runningWatchedGuestCount;
  int storageCount;
  int maxStorageUsedPercent;
};

struct ProxmoxAmbientCard {
  char label[12];
  char value[16];
  char unit[8];
  char stateText[24];
  char stateClass[16];
  char visualClass[16];
};

struct ProxmoxAmbientPageModel {
  char condition[16];
  char telemetryState[24];
  char heroTitle[16];
  char heroValue[16];
  char heroDetail[32];
  char visualClass[16];
  uint8_t cardCount;
  ProxmoxAmbientCard cards[4];
};

inline void proxmoxAmbientCopy(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

inline bool proxmoxAmbientHasLiveData(const ProxmoxAmbientView &view) {
  return view.hasLiveData && view.nodeCount >= 0 && view.onlineNodeCount >= 0;
}

inline const char *proxmoxAmbientCondition(const ProxmoxAmbientView &view) {
  if (!view.enabled || !view.implemented) return "unavailable";
  if (view.condition[0] == '\0') return "unavailable";
  return view.condition;
}

inline const char *proxmoxAmbientTelemetryState(const ProxmoxAmbientView &view) {
  if (!view.enabled) return "disabled";
  if (!view.implemented) return "unavailable";
  if (view.status[0] != '\0') return view.status;
  if (!proxmoxAmbientHasLiveData(view)) return "not_observed";
  return "observed";
}

inline const char *proxmoxAmbientVisualClass(const ProxmoxAmbientView &view) {
  const char *condition = proxmoxAmbientCondition(view);
  if (strcmp(condition, "critical") == 0) return "red";
  if (strcmp(condition, "warning") == 0) return "orange";
  if (strcmp(condition, "stale") == 0) return "gray";
  if (strcmp(condition, "healthy") == 0) return "green";
  return "purple";
}

inline const char *proxmoxAmbientHeroValue(const ProxmoxAmbientView &view) {
  const char *state = proxmoxAmbientTelemetryState(view);
  if (strcmp(state, "unconfigured") == 0) return "SETUP";
  if (strcmp(state, "api_unavailable") == 0) return "OFFLINE";
  if (strcmp(state, "not_observed") == 0) return "WAIT";
  if (strcmp(state, "stale") == 0) return "STALE";
  if (strcmp(proxmoxAmbientCondition(view), "healthy") == 0) return "OK";
  return "CHECK";
}

inline const char *proxmoxAmbientHeroDetail(const ProxmoxAmbientView &view) {
  const char *state = proxmoxAmbientTelemetryState(view);
  if (strcmp(state, "unconfigured") == 0) return "Config missing";
  if (strcmp(state, "api_unavailable") == 0) return "API unavailable";
  if (strcmp(state, "not_observed") == 0) return "No live data";
  if (view.signalSummary[0] != '\0') return view.signalSummary;
  return "Read-only API";
}

inline void proxmoxAmbientFormatCountPair(bool valid, int numerator, int denominator, char *buf, size_t len) {
  if (!valid) snprintf(buf, len, "--");
  else snprintf(buf, len, "%d/%d", numerator, denominator);
}

inline void fillProxmoxAmbientApiCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  proxmoxAmbientCopy(card.label, sizeof(card.label), "API");
  proxmoxAmbientCopy(card.value, sizeof(card.value), proxmoxAmbientHasLiveData(view) ? "OK" : "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "");
  const char *state = proxmoxAmbientTelemetryState(view);
  if (strcmp(state, "unconfigured") == 0) proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), "UNCONFIGURED");
  else if (strcmp(state, "api_unavailable") == 0) proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), "API UNAVAILABLE");
  else if (strcmp(state, "not_observed") == 0) proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), "NOT OBSERVED");
  else proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), "OBSERVED");
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), proxmoxAmbientVisualClass(view));
}

inline void fillProxmoxAmbientNodesCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  proxmoxAmbientCopy(card.label, sizeof(card.label), "Nodes");
  proxmoxAmbientFormatCountPair(proxmoxAmbientHasLiveData(view), view.onlineNodeCount, view.nodeCount, card.value, sizeof(card.value));
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "online");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), proxmoxAmbientHasLiveData(view) ? "ONLINE" : "UNAVAILABLE");
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), proxmoxAmbientHasLiveData(view) ? "green" : "purple");
}

inline void fillProxmoxAmbientGuestsCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  proxmoxAmbientCopy(card.label, sizeof(card.label), "Guests");
  bool valid = proxmoxAmbientHasLiveData(view) && view.watchedGuestCount >= 0 && view.runningWatchedGuestCount >= 0;
  proxmoxAmbientFormatCountPair(valid, view.runningWatchedGuestCount, view.watchedGuestCount, card.value, sizeof(card.value));
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "running");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), valid ? "RUNNING" : "UNAVAILABLE");
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), valid ? "green" : "purple");
}

inline void fillProxmoxAmbientStorageCard(ProxmoxAmbientCard &card, const ProxmoxAmbientView &view) {
  proxmoxAmbientCopy(card.label, sizeof(card.label), "Storage");
  bool valid = proxmoxAmbientHasLiveData(view) && view.storageCount >= 0 && view.maxStorageUsedPercent >= 0;
  if (valid) snprintf(card.value, sizeof(card.value), "%d", view.maxStorageUsedPercent);
  else snprintf(card.value, sizeof(card.value), "--");
  proxmoxAmbientCopy(card.unit, sizeof(card.unit), "% max");
  proxmoxAmbientCopy(card.stateText, sizeof(card.stateText), valid ? "OK" : "UNAVAILABLE");
  proxmoxAmbientCopy(card.stateClass, sizeof(card.stateClass), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(card.visualClass, sizeof(card.visualClass), valid ? "green" : "purple");
}

inline ProxmoxAmbientPageModel makeProxmoxAmbientPageModel(const ProxmoxAmbientView &view) {
  ProxmoxAmbientPageModel model{};
  proxmoxAmbientCopy(model.condition, sizeof(model.condition), proxmoxAmbientCondition(view));
  proxmoxAmbientCopy(model.telemetryState, sizeof(model.telemetryState), proxmoxAmbientTelemetryState(view));
  proxmoxAmbientCopy(model.heroTitle, sizeof(model.heroTitle), "PROXMOX");
  proxmoxAmbientCopy(model.heroValue, sizeof(model.heroValue), proxmoxAmbientHeroValue(view));
  proxmoxAmbientCopy(model.heroDetail, sizeof(model.heroDetail), proxmoxAmbientHeroDetail(view));
  proxmoxAmbientCopy(model.visualClass, sizeof(model.visualClass), proxmoxAmbientVisualClass(view));
  model.cardCount = proxmoxAmbientHasLiveData(view) ? 4 : 3;
  fillProxmoxAmbientApiCard(model.cards[0], view);
  fillProxmoxAmbientNodesCard(model.cards[1], view);
  fillProxmoxAmbientGuestsCard(model.cards[2], view);
  if (model.cardCount > 3) fillProxmoxAmbientStorageCard(model.cards[3], view);
  return model;
}

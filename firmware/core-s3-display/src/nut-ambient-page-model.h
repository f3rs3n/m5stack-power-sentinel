#pragma once

#include <stdio.h>
#include <string.h>

#include "ledcards-interface-page.h"

enum NutAmbientMetricKind {
  NUT_AMBIENT_METRIC_BATTERY,
  NUT_AMBIENT_METRIC_RUNTIME,
  NUT_AMBIENT_METRIC_LOAD,
  NUT_AMBIENT_METRIC_INPUT,
  NUT_AMBIENT_METRIC_NUT,
};

struct NutAmbientMetricCard {
  NutAmbientMetricKind kind;
  char metricId[12];
  char label[12];
  char value[16];
  char compactValue[16];
  char unit[8];
  char compactUnit[8];
  char stateText[24];
  char stateClass[16];
  char visualClass[16];
};

struct NutAmbientPageModel {
  char condition[16];
  char telemetryState[16];
  bool shutdownRelevant;
  bool hasMissingMetrics;
  NutAmbientMetricKind heroMetric;
  uint8_t cardCount;
  NutAmbientMetricCard cards[5];
};

struct NutAmbientHeroPolicyInput {
  NutAmbientMetricKind currentMetric;
  bool touchOverrideActive;
  NutAmbientMetricKind touchOverrideMetric;
  uint32_t touchOverrideUntilMs;
  uint32_t lastHeroSwapMs;
  uint32_t nowMillis;
  uint32_t cooldownMs;
};

struct NutAmbientHeroPolicyDecision {
  NutAmbientMetricKind acceptedMetric;
  bool touchOverrideActive;
  uint32_t lastHeroSwapMs;
};

struct NutAmbientTouchHeroOverride {
  bool active;
  NutAmbientMetricKind metric;
  uint32_t untilMs;
};

inline void nutAmbientCopy(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

inline bool nutAmbientTelemetryUnavailable(const LedcardsInterfaceNutView &view) {
  return !view.upsAvailable;
}

inline bool nutAmbientTelemetryStale(const LedcardsInterfaceNutView &view) {
  return view.offline || view.upsStale;
}

inline bool nutAmbientTelemetryMissing(const LedcardsInterfaceNutView &view) {
  return nutAmbientTelemetryUnavailable(view) || nutAmbientTelemetryStale(view);
}

inline bool nutAmbientHasMissingMetrics(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryMissing(view)) return false;
  return view.batteryPercent < 0 ||
         view.runtimeSeconds < 0 ||
         view.loadPercent < 0 ||
         view.inputVoltage <= 0.0f ||
         view.nutClientCount < 0;
}

inline const char *nutAmbientTelemetryState(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (nutAmbientHasMissingMetrics(view)) return "partial";
  return "live";
}

inline const char *nutAmbientCondition(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (view.onBattery && view.lowBattery) return "critical";
  if (view.onBattery || view.lowBattery) return "warning";
  return "healthy";
}

inline bool nutAmbientShutdownRelevant(const LedcardsInterfaceNutView &view) {
  return !nutAmbientTelemetryMissing(view) && view.onBattery && view.lowBattery;
}

inline const char *nutAmbientMissingStateText(const LedcardsInterfaceNutView &view) {
  return nutAmbientTelemetryUnavailable(view) ? "UNAVAILABLE" : "STALE";
}

inline void nutAmbientFormatIntOrDash(int value, char *buf, size_t len) {
  if (value < 0) snprintf(buf, len, "--");
  else snprintf(buf, len, "%d", value);
}

inline void nutAmbientFormatVoltageOrDash(float value, char *buf, size_t len) {
  if (value <= 0.0f) snprintf(buf, len, "--");
  else snprintf(buf, len, "%.0f", value);
}

inline void nutAmbientFormatRuntimeFull(int seconds, char *buf, size_t len) {
  if (seconds < 0) snprintf(buf, len, "--");
  else snprintf(buf, len, "%02d:%02d", seconds / 60, seconds % 60);
}

inline void nutAmbientFormatRuntimeMinutes(int seconds, char *buf, size_t len) {
  if (seconds < 0) snprintf(buf, len, "--");
  else snprintf(buf, len, "%d", (seconds + 30) / 60);
}

inline const char *nutAmbientBatteryStateText(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryMissing(view)) return nutAmbientMissingStateText(view);
  if (view.batteryPercent < 0) return "UNAVAILABLE";
  if (view.onBattery) {
    if (view.batteryPercent < 10) return "CRITICAL BATTERY";
    if (view.lowBattery || view.batteryPercent < 20) return "LOW BATTERY";
    return "ON BATTERY";
  }
  if (view.batteryPercent < 20) return "CHARGING";
  if (view.batteryPercent >= 90) return "FULL";
  if (view.batteryPercent >= 50) return "ALMOST FULL";
  return "CHARGING";
}

inline const char *nutAmbientBatteryStateClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (view.batteryPercent < 0) return "unavailable";
  if (view.onBattery && (view.lowBattery || view.batteryPercent < 10)) return "critical";
  if (view.onBattery || view.lowBattery || view.batteryPercent < 50) return "warning";
  return "healthy";
}

inline const char *nutAmbientBatteryVisualClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "purple";
  if (nutAmbientTelemetryStale(view)) return "gray";
  if (view.batteryPercent < 0) return "purple";
  if (view.onBattery) {
    if (view.batteryPercent < 10) return "red";
    if (view.lowBattery || view.batteryPercent < 20) return "orange";
    return "yellow";
  }
  if (view.batteryPercent < 50) return "orange";
  if (view.batteryPercent < 90) return "yellow";
  return "green";
}

inline const char *nutAmbientLoadStateText(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryMissing(view)) return nutAmbientMissingStateText(view);
  if (view.loadPercent < 0) return "UNAVAILABLE";
  if (view.loadPercent >= 90) return "OVERLOAD";
  if (view.loadPercent >= 70) return "HIGH LOAD";
  if (view.loadPercent < 10) return "LOW";
  return "NORMAL";
}

inline const char *nutAmbientLoadStateClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (view.loadPercent < 0) return "unavailable";
  if (view.loadPercent >= 90) return "critical";
  if (view.loadPercent >= 70) return "warning";
  return "healthy";
}

inline const char *nutAmbientLoadVisualClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "purple";
  if (nutAmbientTelemetryStale(view)) return "gray";
  if (view.loadPercent < 0) return "purple";
  if (view.loadPercent >= 90) return "red";
  if (view.loadPercent >= 70) return "orange";
  if (view.loadPercent < 10) return "blue";
  return "green";
}

inline const char *nutAmbientInputStateText(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryMissing(view)) return nutAmbientMissingStateText(view);
  if (view.inputVoltage <= 0.0f) return view.onBattery ? "GRID OFFLINE" : "UNAVAILABLE";
  if (view.inputVoltage < 190.0f) return "INPUT LOW";
  if (view.inputVoltage < 210.0f) return "MARGINAL INPUT";
  return "GRID ONLINE";
}

inline const char *nutAmbientInputStateClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (view.inputVoltage <= 0.0f) return view.onBattery ? "critical" : "unavailable";
  if (view.inputVoltage < 210.0f) return "warning";
  return "healthy";
}

inline const char *nutAmbientInputVisualClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "purple";
  if (nutAmbientTelemetryStale(view)) return "gray";
  if (view.inputVoltage <= 0.0f) return view.onBattery ? "red" : "gray";
  if (view.inputVoltage < 190.0f) return "orange";
  if (view.inputVoltage < 210.0f) return "yellow";
  return "green";
}

inline const char *nutAmbientNutStateText(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryMissing(view)) return nutAmbientMissingStateText(view);
  if (view.nutClientCount < 0) return "UNAVAILABLE";
  if (view.nutClientCount == 0) return "NO CLIENTS";
  return "CLIENTS";
}

inline const char *nutAmbientNutStateClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (view.nutClientCount < 0) return "unavailable";
  if (view.nutClientCount == 0) return "warning";
  return "healthy";
}

inline const char *nutAmbientNutVisualClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "purple";
  if (nutAmbientTelemetryStale(view)) return "gray";
  if (view.nutClientCount < 0) return "purple";
  if (view.nutClientCount == 0) return "orange";
  return "blue";
}

inline const char *nutAmbientRuntimeStateText(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryMissing(view)) return nutAmbientMissingStateText(view);
  if (view.onBattery) {
    if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 120) return "CRITICAL RUNTIME";
    if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 300) return "SHORT RUNTIME";
    return "ON BATTERY";
  }
  if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 120) return "CRITICAL RESERVE";
  if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 300) return "LOW RESERVE";
  if (view.runtimeSeconds < 0) return "UNAVAILABLE";
  return "RESERVE";
}

inline const char *nutAmbientRuntimeStateClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (view.runtimeSeconds < 0) return "unavailable";
  if (view.runtimeSeconds < 120) return "critical";
  if (view.onBattery || view.runtimeSeconds < 300) return "warning";
  return "healthy";
}

inline const char *nutAmbientRuntimeVisualClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "purple";
  if (nutAmbientTelemetryStale(view)) return "gray";
  if (view.runtimeSeconds < 0) return "purple";
  if (view.runtimeSeconds < 120) return "red";
  if (view.runtimeSeconds < 300) return view.onBattery ? "orange" : "yellow";
  return view.onBattery ? "yellow" : "blue";
}

inline NutAmbientMetricKind chooseNutAmbientHeroMetric(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryMissing(view)) return NUT_AMBIENT_METRIC_NUT;

  if (view.onBattery) {
    if (view.runtimeSeconds >= 0 && view.runtimeSeconds < 120) return NUT_AMBIENT_METRIC_RUNTIME;
    if (view.lowBattery || (view.batteryPercent >= 0 && view.batteryPercent < 20)) return NUT_AMBIENT_METRIC_BATTERY;
    if (view.runtimeSeconds >= 0) return NUT_AMBIENT_METRIC_RUNTIME;
    if (view.loadPercent >= 70) return NUT_AMBIENT_METRIC_LOAD;
    return NUT_AMBIENT_METRIC_RUNTIME;
  }

  if (view.batteryPercent >= 0 && view.batteryPercent < 20) return NUT_AMBIENT_METRIC_BATTERY;
  if (view.loadPercent >= 70) return NUT_AMBIENT_METRIC_LOAD;
  if (view.inputVoltage > 0.0f && view.inputVoltage < 210.0f) return NUT_AMBIENT_METRIC_INPUT;
  return NUT_AMBIENT_METRIC_BATTERY;
}

inline bool nutAmbientTouchHeroOverrideStillActive(uint32_t nowMillis, uint32_t untilMs) {
  return (int32_t)(untilMs - nowMillis) > 0;
}

inline NutAmbientTouchHeroOverride makeNutAmbientTouchHeroOverride(NutAmbientMetricKind metric, uint32_t nowMillis, uint32_t durationMs) {
  NutAmbientTouchHeroOverride touch{};
  touch.active = true;
  touch.metric = metric;
  touch.untilMs = nowMillis + durationMs;
  return touch;
}

inline NutAmbientHeroPolicyDecision acceptNutAmbientHeroMetric(const NutAmbientPageModel &model, const NutAmbientHeroPolicyInput &input) {
  NutAmbientHeroPolicyDecision decision{};
  decision.acceptedMetric = input.currentMetric;
  decision.touchOverrideActive = input.touchOverrideActive;
  decision.lastHeroSwapMs = input.lastHeroSwapMs;

  if (input.touchOverrideActive) {
    if (nutAmbientTouchHeroOverrideStillActive(input.nowMillis, input.touchOverrideUntilMs)) {
      decision.acceptedMetric = input.touchOverrideMetric;
      return decision;
    }
    decision.touchOverrideActive = false;
  }

  NutAmbientMetricKind candidate = model.heroMetric;
  if (candidate == input.currentMetric) {
    decision.acceptedMetric = candidate;
    return decision;
  }

  if (input.lastHeroSwapMs == 0 || input.nowMillis - input.lastHeroSwapMs >= input.cooldownMs) {
    decision.acceptedMetric = candidate;
    decision.lastHeroSwapMs = input.nowMillis;
    return decision;
  }

  return decision;
}

inline void fillNutAmbientCard(NutAmbientMetricCard &card, NutAmbientMetricKind kind, const LedcardsInterfaceNutView &view) {
  card.kind = kind;
  switch (kind) {
    case NUT_AMBIENT_METRIC_BATTERY:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "battery");
      nutAmbientCopy(card.label, sizeof(card.label), "Battery");
      nutAmbientFormatIntOrDash(nutAmbientTelemetryMissing(view) ? -1 : view.batteryPercent, card.value, sizeof(card.value));
      nutAmbientCopy(card.compactValue, sizeof(card.compactValue), card.value);
      nutAmbientCopy(card.unit, sizeof(card.unit), "%");
      nutAmbientCopy(card.compactUnit, sizeof(card.compactUnit), "%");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientBatteryStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientBatteryStateClass(view));
      nutAmbientCopy(card.visualClass, sizeof(card.visualClass), nutAmbientBatteryVisualClass(view));
      return;
    case NUT_AMBIENT_METRIC_RUNTIME:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "runtime");
      nutAmbientCopy(card.label, sizeof(card.label), "Runtime");
      nutAmbientFormatRuntimeFull(nutAmbientTelemetryMissing(view) ? -1 : view.runtimeSeconds, card.value, sizeof(card.value));
      nutAmbientFormatRuntimeMinutes(nutAmbientTelemetryMissing(view) ? -1 : view.runtimeSeconds, card.compactValue, sizeof(card.compactValue));
      nutAmbientCopy(card.unit, sizeof(card.unit), "mm:ss");
      nutAmbientCopy(card.compactUnit, sizeof(card.compactUnit), "m");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientRuntimeStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientRuntimeStateClass(view));
      nutAmbientCopy(card.visualClass, sizeof(card.visualClass), nutAmbientRuntimeVisualClass(view));
      return;
    case NUT_AMBIENT_METRIC_LOAD:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "load");
      nutAmbientCopy(card.label, sizeof(card.label), "Load");
      nutAmbientFormatIntOrDash(nutAmbientTelemetryMissing(view) ? -1 : view.loadPercent, card.value, sizeof(card.value));
      nutAmbientCopy(card.compactValue, sizeof(card.compactValue), card.value);
      nutAmbientCopy(card.unit, sizeof(card.unit), "%");
      nutAmbientCopy(card.compactUnit, sizeof(card.compactUnit), "%");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientLoadStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientLoadStateClass(view));
      nutAmbientCopy(card.visualClass, sizeof(card.visualClass), nutAmbientLoadVisualClass(view));
      return;
    case NUT_AMBIENT_METRIC_INPUT:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "input");
      nutAmbientCopy(card.label, sizeof(card.label), "Input");
      nutAmbientFormatVoltageOrDash(nutAmbientTelemetryMissing(view) ? 0.0f : view.inputVoltage, card.value, sizeof(card.value));
      nutAmbientCopy(card.compactValue, sizeof(card.compactValue), card.value);
      nutAmbientCopy(card.unit, sizeof(card.unit), "V");
      nutAmbientCopy(card.compactUnit, sizeof(card.compactUnit), "V");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientInputStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientInputStateClass(view));
      nutAmbientCopy(card.visualClass, sizeof(card.visualClass), nutAmbientInputVisualClass(view));
      return;
    case NUT_AMBIENT_METRIC_NUT:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "nut");
      nutAmbientCopy(card.label, sizeof(card.label), "NUT");
      nutAmbientFormatIntOrDash(nutAmbientTelemetryMissing(view) ? -1 : view.nutClientCount, card.value, sizeof(card.value));
      nutAmbientCopy(card.compactValue, sizeof(card.compactValue), card.value);
      nutAmbientCopy(card.unit, sizeof(card.unit), "client");
      nutAmbientCopy(card.compactUnit, sizeof(card.compactUnit), "client");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientNutStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientNutStateClass(view));
      nutAmbientCopy(card.visualClass, sizeof(card.visualClass), nutAmbientNutVisualClass(view));
      return;
  }
}

inline NutAmbientPageModel makeNutAmbientPageModel(const LedcardsInterfaceNutView &view) {
  NutAmbientPageModel model{};
  nutAmbientCopy(model.condition, sizeof(model.condition), nutAmbientCondition(view));
  nutAmbientCopy(model.telemetryState, sizeof(model.telemetryState), nutAmbientTelemetryState(view));
  model.shutdownRelevant = nutAmbientShutdownRelevant(view);
  model.hasMissingMetrics = nutAmbientHasMissingMetrics(view);
  model.heroMetric = chooseNutAmbientHeroMetric(view);
  model.cardCount = 5;
  fillNutAmbientCard(model.cards[0], NUT_AMBIENT_METRIC_BATTERY, view);
  fillNutAmbientCard(model.cards[1], NUT_AMBIENT_METRIC_RUNTIME, view);
  fillNutAmbientCard(model.cards[2], NUT_AMBIENT_METRIC_LOAD, view);
  fillNutAmbientCard(model.cards[3], NUT_AMBIENT_METRIC_INPUT, view);
  fillNutAmbientCard(model.cards[4], NUT_AMBIENT_METRIC_NUT, view);
  return model;
}

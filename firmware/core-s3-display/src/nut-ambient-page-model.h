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
  char unit[8];
  char stateText[24];
  char stateClass[16];
};

struct NutAmbientPageModel {
  char condition[16];
  NutAmbientMetricKind heroMetric;
  uint8_t cardCount;
  NutAmbientMetricCard cards[5];
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

inline const char *nutAmbientCondition(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (view.onBattery && view.lowBattery) return "critical";
  if (view.onBattery || view.lowBattery) return "warning";
  return "healthy";
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

inline const char *nutAmbientInputStateText(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryMissing(view)) return nutAmbientMissingStateText(view);
  if (view.inputVoltage <= 0.0f) return view.onBattery ? "GRID OFFLINE" : "INPUT LOST";
  if (view.inputVoltage < 190.0f) return "INPUT LOW";
  if (view.inputVoltage < 210.0f) return "MARGINAL INPUT";
  return "GRID ONLINE";
}

inline const char *nutAmbientInputStateClass(const LedcardsInterfaceNutView &view) {
  if (nutAmbientTelemetryUnavailable(view)) return "unavailable";
  if (nutAmbientTelemetryStale(view)) return "stale";
  if (view.inputVoltage <= 0.0f) return view.onBattery ? "critical" : "stale";
  if (view.inputVoltage < 210.0f) return "warning";
  return "healthy";
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

inline void fillNutAmbientCard(NutAmbientMetricCard &card, NutAmbientMetricKind kind, const LedcardsInterfaceNutView &view) {
  card.kind = kind;
  switch (kind) {
    case NUT_AMBIENT_METRIC_BATTERY:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "battery");
      nutAmbientCopy(card.label, sizeof(card.label), "Battery");
      nutAmbientFormatIntOrDash(nutAmbientTelemetryMissing(view) ? -1 : view.batteryPercent, card.value, sizeof(card.value));
      nutAmbientCopy(card.unit, sizeof(card.unit), "%");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientBatteryStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientBatteryStateClass(view));
      return;
    case NUT_AMBIENT_METRIC_RUNTIME:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "runtime");
      nutAmbientCopy(card.label, sizeof(card.label), "Runtime");
      nutAmbientFormatRuntimeFull(nutAmbientTelemetryMissing(view) ? -1 : view.runtimeSeconds, card.value, sizeof(card.value));
      nutAmbientCopy(card.unit, sizeof(card.unit), "mm:ss");
      if (nutAmbientTelemetryMissing(view)) {
        nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientMissingStateText(view));
        nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientTelemetryUnavailable(view) ? "unavailable" : "stale");
      } else if (view.onBattery) {
        nutAmbientCopy(card.stateText, sizeof(card.stateText), "ON BATTERY");
        nutAmbientCopy(card.stateClass, sizeof(card.stateClass), "warning");
      } else {
        nutAmbientCopy(card.stateText, sizeof(card.stateText), view.runtimeSeconds >= 0 && view.runtimeSeconds < 300 ? "LOW RESERVE" : "RESERVE");
        nutAmbientCopy(card.stateClass, sizeof(card.stateClass), view.runtimeSeconds >= 0 && view.runtimeSeconds < 300 ? "warning" : "healthy");
      }
      return;
    case NUT_AMBIENT_METRIC_LOAD:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "load");
      nutAmbientCopy(card.label, sizeof(card.label), "Load");
      nutAmbientFormatIntOrDash(nutAmbientTelemetryMissing(view) ? -1 : view.loadPercent, card.value, sizeof(card.value));
      nutAmbientCopy(card.unit, sizeof(card.unit), "%");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientLoadStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientLoadStateClass(view));
      return;
    case NUT_AMBIENT_METRIC_INPUT:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "input");
      nutAmbientCopy(card.label, sizeof(card.label), "Input");
      nutAmbientFormatVoltageOrDash(nutAmbientTelemetryMissing(view) ? 0.0f : view.inputVoltage, card.value, sizeof(card.value));
      nutAmbientCopy(card.unit, sizeof(card.unit), "V");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientInputStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientInputStateClass(view));
      return;
    case NUT_AMBIENT_METRIC_NUT:
      nutAmbientCopy(card.metricId, sizeof(card.metricId), "nut");
      nutAmbientCopy(card.label, sizeof(card.label), "NUT");
      nutAmbientFormatIntOrDash(nutAmbientTelemetryMissing(view) ? -1 : view.nutClientCount, card.value, sizeof(card.value));
      nutAmbientCopy(card.unit, sizeof(card.unit), "client");
      nutAmbientCopy(card.stateText, sizeof(card.stateText), nutAmbientNutStateText(view));
      nutAmbientCopy(card.stateClass, sizeof(card.stateClass), nutAmbientNutStateClass(view));
      return;
  }
}

inline NutAmbientPageModel makeNutAmbientPageModel(const LedcardsInterfaceNutView &view) {
  NutAmbientPageModel model{};
  nutAmbientCopy(model.condition, sizeof(model.condition), nutAmbientCondition(view));
  model.heroMetric = chooseNutAmbientHeroMetric(view);
  model.cardCount = 5;
  fillNutAmbientCard(model.cards[0], NUT_AMBIENT_METRIC_BATTERY, view);
  fillNutAmbientCard(model.cards[1], NUT_AMBIENT_METRIC_RUNTIME, view);
  fillNutAmbientCard(model.cards[2], NUT_AMBIENT_METRIC_LOAD, view);
  fillNutAmbientCard(model.cards[3], NUT_AMBIENT_METRIC_INPUT, view);
  fillNutAmbientCard(model.cards[4], NUT_AMBIENT_METRIC_NUT, view);
  return model;
}

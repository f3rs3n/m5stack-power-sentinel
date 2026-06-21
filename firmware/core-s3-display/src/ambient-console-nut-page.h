#pragma once

#include <stdint.h>

#include "ambient-console-state.h"
#include "ledcards-interface-page.h"

struct AmbientConsoleNutPage {
  const char *condition(const SummaryState &state) const {
    return state.nut.condition;
  }

  LedcardsInterfaceNutView makeView(const SummaryState &state,
                                    bool wifiConnected,
                                    int localBatteryPercent,
                                    bool localBatteryCharging,
                                    uint32_t now,
                                    uint8_t pageCount,
                                    uint8_t pageIndex,
                                    uint32_t linkStatusTimeoutMs) const {
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
    ambientConsoleSafeCopy(view.condition, sizeof(view.condition), state.nut.condition);
    view.ageSeconds = state.ups.ageSeconds;
    view.moduleLanConnected = state.moduleLanConnected;
    view.wifiConnected = wifiConnected;
    view.linkOk = state.lastGoodMillis != 0 && now - state.lastGoodMillis <= linkStatusTimeoutMs;
    ambientConsoleSafeCopy(view.transportStatus, sizeof(view.transportStatus), state.transportStatus);
    ambientConsoleSafeCopy(view.moduleTimeHhmm, sizeof(view.moduleTimeHhmm), ambientConsoleValidHhmm(state.moduleTimeHhmm) ? state.moduleTimeHhmm : "--:--");
    view.localBatteryPercent = localBatteryPercent;
    view.localBatteryCharging = localBatteryCharging;
    view.localBatteryKnown = view.localBatteryPercent >= 0;
    view.pageCount = pageCount;
    view.pageIndex = pageIndex;
    view.nowMillis = now;
    return view;
  }

  void create(const LedcardsInterfaceNutView &view) const {
    createLedcardsInterfaceUi(view);
  }

  void render(const LedcardsInterfaceNutView &view) const {
    updateLedcardsInterfaceUi(view);
  }
};

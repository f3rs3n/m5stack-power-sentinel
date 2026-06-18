#pragma once

#include <stdint.h>

#include "ambient-console-state.h"
#include "ledcards-interface-page.h"

struct AmbientConsoleShell {
  uint32_t linkStatusTimeoutMs = 10000UL;
  uint8_t currentPageIndex = 0;
  bool topBarPageTouchActive = false;
  bool linkStatusRenderedValid = false;
  bool lastRenderedLinkOk = false;

  explicit AmbientConsoleShell(uint32_t linkTimeoutMs = 10000UL) : linkStatusTimeoutMs(linkTimeoutMs) {}

  bool proxmoxPageAvailable(const SummaryState &state) const {
    return state.proxmox.enabled && state.proxmox.implemented;
  }

  uint8_t ambientPageCount(const SummaryState &state) const {
    return proxmoxPageAvailable(state) ? 2 : 1;
  }

  void clampCurrentPageIndex(const SummaryState &state) {
    uint8_t pageCount = ambientPageCount(state);
    if (currentPageIndex >= pageCount) currentPageIndex = 0;
  }

  bool linkOkAt(const SummaryState &state, uint32_t now) const {
    return state.lastGoodMillis != 0 && now - state.lastGoodMillis <= linkStatusTimeoutMs;
  }

  LedcardsInterfaceNutView makeLedcardsInterfaceNutView(const SummaryState &state,
                                                        bool wifiConnected,
                                                        int localBatteryPercent,
                                                        bool localBatteryCharging,
                                                        uint32_t now) {
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
    view.moduleLanConnected = state.moduleLanConnected;
    view.wifiConnected = wifiConnected;
    view.linkOk = linkOkAt(state, now);
    ambientConsoleSafeCopy(view.transportStatus, sizeof(view.transportStatus), state.transportStatus);
    ambientConsoleSafeCopy(view.moduleTimeHhmm, sizeof(view.moduleTimeHhmm), ambientConsoleValidHhmm(state.moduleTimeHhmm) ? state.moduleTimeHhmm : "--:--");
    view.localBatteryPercent = localBatteryPercent;
    view.localBatteryCharging = localBatteryCharging;
    view.localBatteryKnown = view.localBatteryPercent >= 0;
    clampCurrentPageIndex(state);
    view.pageCount = ambientPageCount(state);
    view.pageIndex = currentPageIndex;
    view.nowMillis = now;
    return view;
  }

  ProxmoxAmbientView makeProxmoxAmbientView(const SummaryState &state) const {
    ProxmoxAmbientView view{};
    view.enabled = state.proxmox.enabled;
    view.implemented = state.proxmox.implemented;
    view.hasLiveData = state.proxmox.hasLiveData;
    ambientConsoleSafeCopy(view.condition, sizeof(view.condition), state.proxmox.condition);
    ambientConsoleSafeCopy(view.status, sizeof(view.status), state.proxmox.status);
    view.cpuPercent = state.proxmox.cpuPercent;
    ambientConsoleSafeCopy(view.cpuCondition, sizeof(view.cpuCondition), state.proxmox.cpuCondition);
    view.ramPercent = state.proxmox.ramPercent;
    ambientConsoleSafeCopy(view.ramCondition, sizeof(view.ramCondition), state.proxmox.ramCondition);
    view.guestRunning = state.proxmox.guestRunning;
    view.guestTotal = state.proxmox.guestTotal;
    ambientConsoleSafeCopy(view.guestCondition, sizeof(view.guestCondition), state.proxmox.guestCondition);
    view.storagePercent = state.proxmox.storagePercent;
    ambientConsoleSafeCopy(view.storageCondition, sizeof(view.storageCondition), state.proxmox.storageCondition);
    view.networkPercent = state.proxmox.networkPercent;
    ambientConsoleSafeCopy(view.networkCondition, sizeof(view.networkCondition), state.proxmox.networkCondition);
    return view;
  }

  void create(const SummaryState &state,
              bool wifiConnected,
              int localBatteryPercent,
              bool localBatteryCharging,
              uint32_t now) {
    createLedcardsInterfaceUi(makeLedcardsInterfaceNutView(state, wifiConnected, localBatteryPercent, localBatteryCharging, now));
  }

  void refresh(const SummaryState &state,
               bool wifiConnected,
               int localBatteryPercent,
               bool localBatteryCharging,
               uint32_t now) {
    LedcardsInterfaceNutView view = makeLedcardsInterfaceNutView(state, wifiConnected, localBatteryPercent, localBatteryCharging, now);
    if (currentPageIndex == 1 && proxmoxPageAvailable(state)) {
      renderProxmoxAmbientUi(makeProxmoxAmbientView(state), view);
    } else {
      updateLedcardsInterfaceUi(view);
    }
    lastRenderedLinkOk = view.linkOk;
    linkStatusRenderedValid = true;
  }

  bool handleTopBarPageTap(SummaryState &state,
                           int32_t x,
                           int32_t y,
                           bool wifiConnected,
                           int localBatteryPercent,
                           bool localBatteryCharging,
                           uint32_t now) {
    uint8_t pageCount = ambientPageCount(state);
    if (pageCount <= 1 || y > 24) return false;
    if (!topBarPageTouchActive) {
      uint8_t previousPageIndex = currentPageIndex;
      uint8_t targetPageIndex = x < 160 ? 0 : 1;
      currentPageIndex = targetPageIndex;
      clampCurrentPageIndex(state);
      LedcardsInterfaceNutView view = makeLedcardsInterfaceNutView(state, wifiConnected, localBatteryPercent, localBatteryCharging, now);
      if (!transitionLedcardsInterfacePageUi(previousPageIndex, currentPageIndex, view, makeProxmoxAmbientView(state))) {
        refresh(state, wifiConnected, localBatteryPercent, localBatteryCharging, now);
      }
      topBarPageTouchActive = true;
    }
    return true;
  }

  void noteTouchReleased() {
    topBarPageTouchActive = false;
  }

  bool shouldRefreshForLinkStatus(const SummaryState &state, uint32_t now) const {
    bool currentLinkOk = linkOkAt(state, now);
    return !linkStatusRenderedValid || currentLinkOk != lastRenderedLinkOk;
  }
};

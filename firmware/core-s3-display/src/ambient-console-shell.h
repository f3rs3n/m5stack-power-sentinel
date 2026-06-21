#pragma once

#include <stdint.h>
#include <string.h>

#include "ambient-console-nut-page.h"
#include "ambient-console-page-registry.h"
#include "ambient-console-proxmox-page.h"
#include "ambient-console-state.h"
#include "ledcards-interface-page.h"

struct AmbientConsoleShell {
  static constexpr uint32_t kAutoPageFocusCooldownMs = 60000UL;

  uint32_t linkStatusTimeoutMs = 10000UL;
  uint32_t autoPageFocusCooldownMs = kAutoPageFocusCooldownMs;
  uint8_t currentPageIndex = 0;
  bool autoPageFocusActive = false;
  bool autoPageFocusConditionsValid = false;
  uint8_t autoPageFocusReturnPageIndex = 0;
  uint32_t lastAutoPageFocusSwitchMs = 0;
  char lastAutoPageFocusNutCondition[16] = "";
  char lastAutoPageFocusProxmoxCondition[16] = "";
  bool topBarPageTouchActive = false;
  bool linkStatusRenderedValid = false;
  bool lastRenderedLinkOk = false;
  AmbientConsoleNutPage nutPage{};
  AmbientConsoleProxmoxPage proxmoxPage{};

  explicit AmbientConsoleShell(uint32_t linkTimeoutMs = 10000UL) : linkStatusTimeoutMs(linkTimeoutMs) {}

  AmbientConsolePageAvailability pageAvailability(const SummaryState &state) const {
    AmbientConsolePageAvailability availability{};
    availability.nutEnabled = true;
    availability.proxmoxEnabled = state.proxmox.enabled;
    availability.proxmoxImplemented = state.proxmox.implemented;
    return availability;
  }

  AmbientConsolePageRegistry pageRegistry(const SummaryState &state) const {
    return ambientConsoleBuildPageRegistry(pageAvailability(state));
  }

  bool proxmoxPageAvailable(const SummaryState &state) const {
    return ambientConsolePageIndex(pageRegistry(state), AMBIENT_CONSOLE_PAGE_PROXMOX) != kAmbientConsolePageMissing;
  }

  uint8_t ambientPageCount(const SummaryState &state) const {
    return pageRegistry(state).pageCount;
  }

  void clampCurrentPageIndex(const SummaryState &state) {
    currentPageIndex = ambientConsoleClampPageIndex(pageRegistry(state), currentPageIndex);
  }

  AmbientConsolePageId currentPageId(const SummaryState &state) const {
    AmbientConsolePageRegistry registry = pageRegistry(state);
    uint8_t pageIndex = ambientConsoleClampPageIndex(registry, currentPageIndex);
    const AmbientConsolePage *page = ambientConsolePageAt(registry, pageIndex);
    return page ? page->id : AMBIENT_CONSOLE_PAGE_NUT;
  }

  bool linkOkAt(const SummaryState &state, uint32_t now) const {
    return state.lastGoodMillis != 0 && now - state.lastGoodMillis <= linkStatusTimeoutMs;
  }

  static uint8_t autoPageFocusConditionRank(const char *condition) {
    if (strcmp(condition, "critical") == 0) return 3;
    if (strcmp(condition, "warning") == 0) return 2;
    return 0;
  }

  bool autoPageFocusCooldownElapsed(uint32_t now) const {
    return lastAutoPageFocusSwitchMs == 0 || now - lastAutoPageFocusSwitchMs >= autoPageFocusCooldownMs;
  }

  bool autoPageFocusConditionChanged(AmbientConsolePageId pageId, const SummaryState &state) const {
    if (!autoPageFocusConditionsValid) return true;
    if (pageId == AMBIENT_CONSOLE_PAGE_PROXMOX) return strcmp(lastAutoPageFocusProxmoxCondition, proxmoxPage.condition(state)) != 0;
    return strcmp(lastAutoPageFocusNutCondition, nutPage.condition(state)) != 0;
  }

  uint8_t autoPageFocusRankForPage(AmbientConsolePageId pageId, const SummaryState &state) const {
    if (pageId == AMBIENT_CONSOLE_PAGE_PROXMOX) return autoPageFocusConditionRank(proxmoxPage.condition(state));
    return autoPageFocusConditionRank(nutPage.condition(state));
  }

  void rememberAutoPageFocusConditions(const SummaryState &state) {
    ambientConsoleSafeCopy(lastAutoPageFocusNutCondition, sizeof(lastAutoPageFocusNutCondition), nutPage.condition(state));
    ambientConsoleSafeCopy(lastAutoPageFocusProxmoxCondition, sizeof(lastAutoPageFocusProxmoxCondition), proxmoxPage.condition(state));
    autoPageFocusConditionsValid = true;
  }

  uint8_t bestAutoPageFocusTarget(const SummaryState &state) const {
    AmbientConsolePageRegistry registry = pageRegistry(state);
    uint8_t bestIndex = kAmbientConsolePageMissing;
    uint8_t bestRank = 0;
    for (uint8_t i = 0; i < registry.pageCount; ++i) {
      const AmbientConsolePage *page = ambientConsolePageAt(registry, i);
      if (!page) continue;
      uint8_t rank = autoPageFocusRankForPage(page->id, state);
      if (rank < 2) continue;
      if (rank > bestRank) {
        bestIndex = page->index;
        bestRank = rank;
      }
    }
    return bestIndex;
  }

  bool anyAutoPageFocusConditionChanged(const SummaryState &state) const {
    AmbientConsolePageRegistry registry = pageRegistry(state);
    for (uint8_t i = 0; i < registry.pageCount; ++i) {
      const AmbientConsolePage *page = ambientConsolePageAt(registry, i);
      if (page && autoPageFocusConditionChanged(page->id, state)) return true;
    }
    return false;
  }

  void cancelAutoPageFocus(uint32_t now) {
    autoPageFocusActive = false;
    lastAutoPageFocusSwitchMs = now;
  }

  bool applyAutoPageFocus(const SummaryState &state, uint32_t now) {
    AmbientConsolePageRegistry registry = pageRegistry(state);
    if (registry.pageCount <= 1) {
      currentPageIndex = ambientConsoleClampPageIndex(registry, currentPageIndex);
      autoPageFocusActive = false;
      rememberAutoPageFocusConditions(state);
      return false;
    }

    bool firstObservation = !autoPageFocusConditionsValid;
    bool anyConditionChanged = anyAutoPageFocusConditionChanged(state);
    currentPageIndex = ambientConsoleClampPageIndex(registry, currentPageIndex);
    uint8_t targetPageIndex = bestAutoPageFocusTarget(state);

    if (targetPageIndex != kAmbientConsolePageMissing) {
      AmbientConsolePageId targetPageId = currentPageId(state);
      const AmbientConsolePage *targetPage = ambientConsolePageAt(registry, targetPageIndex);
      if (targetPage) targetPageId = targetPage->id;
      bool targetConditionChanged = autoPageFocusConditionChanged(targetPageId, state);
      bool shouldFocus = firstObservation || targetConditionChanged || (autoPageFocusActive && anyConditionChanged);
      if (targetPageIndex != currentPageIndex && shouldFocus) {
        if (autoPageFocusCooldownElapsed(now)) {
          if (!autoPageFocusActive) autoPageFocusReturnPageIndex = currentPageIndex;
          currentPageIndex = targetPageIndex;
          autoPageFocusActive = true;
          lastAutoPageFocusSwitchMs = now;
          rememberAutoPageFocusConditions(state);
          return true;
        }
        return false;
      }
      rememberAutoPageFocusConditions(state);
      return false;
    }

    if (autoPageFocusActive) {
      autoPageFocusActive = false;
      uint8_t returnPageIndex = ambientConsoleClampPageIndex(registry, autoPageFocusReturnPageIndex);
      if (returnPageIndex != currentPageIndex) {
        currentPageIndex = returnPageIndex;
        lastAutoPageFocusSwitchMs = now;
        rememberAutoPageFocusConditions(state);
        return true;
      }
    }

    rememberAutoPageFocusConditions(state);
    return false;
  }

  LedcardsInterfaceNutView makeNutPageView(const SummaryState &state,
                                           bool wifiConnected,
                                           int localBatteryPercent,
                                           bool localBatteryCharging,
                                           uint32_t now) {
    clampCurrentPageIndex(state);
    return nutPage.makeView(state, wifiConnected, localBatteryPercent, localBatteryCharging, now, ambientPageCount(state), currentPageIndex, linkStatusTimeoutMs);
  }

  void create(const SummaryState &state,
              bool wifiConnected,
              int localBatteryPercent,
              bool localBatteryCharging,
              uint32_t now) {
    nutPage.create(makeNutPageView(state, wifiConnected, localBatteryPercent, localBatteryCharging, now));
  }

  void refresh(const SummaryState &state,
               bool wifiConnected,
               int localBatteryPercent,
               bool localBatteryCharging,
               uint32_t now) {
    uint8_t previousPageIndex = currentPageIndex;
    bool autoFocused = applyAutoPageFocus(state, now);
    LedcardsInterfaceNutView view = makeNutPageView(state, wifiConnected, localBatteryPercent, localBatteryCharging, now);
    if (autoFocused && transitionLedcardsInterfacePageUi(previousPageIndex, currentPageIndex, view, proxmoxPage.makeView(state))) {
      lastRenderedLinkOk = view.linkOk;
      linkStatusRenderedValid = true;
      return;
    }
    if (currentPageId(state) == AMBIENT_CONSOLE_PAGE_PROXMOX) {
      proxmoxPage.render(proxmoxPage.makeView(state), view);
    } else {
      nutPage.render(view);
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
      AmbientConsolePageRegistry registry = pageRegistry(state);
      uint8_t targetPageIndex = x < 160 ? 0 : ambientConsolePageIndex(registry, AMBIENT_CONSOLE_PAGE_PROXMOX);
      if (targetPageIndex == kAmbientConsolePageMissing) return false;
      currentPageIndex = targetPageIndex;
      cancelAutoPageFocus(now);
      clampCurrentPageIndex(state);
      LedcardsInterfaceNutView view = makeNutPageView(state, wifiConnected, localBatteryPercent, localBatteryCharging, now);
      if (!transitionLedcardsInterfacePageUi(previousPageIndex, currentPageIndex, view, proxmoxPage.makeView(state))) {
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

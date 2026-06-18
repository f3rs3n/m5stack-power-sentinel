#pragma once

#include <stdint.h>

#include "ambient-console-nut-page.h"
#include "ambient-console-page-registry.h"
#include "ambient-console-proxmox-page.h"
#include "ambient-console-state.h"
#include "ledcards-interface-page.h"

struct AmbientConsoleShell {
  uint32_t linkStatusTimeoutMs = 10000UL;
  uint8_t currentPageIndex = 0;
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
    LedcardsInterfaceNutView view = makeNutPageView(state, wifiConnected, localBatteryPercent, localBatteryCharging, now);
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

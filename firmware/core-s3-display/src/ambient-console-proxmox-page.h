#pragma once

#include "ambient-console-state.h"
#include "ledcards-interface-page.h"

struct AmbientConsoleProxmoxPage {
  const char *condition(const SummaryState &state) const {
    return state.proxmox.condition;
  }

  ProxmoxAmbientView makeView(const SummaryState &state) const {
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

  void render(const ProxmoxAmbientView &view, const LedcardsInterfaceNutView &statusView) const {
    renderProxmoxAmbientUi(view, statusView);
  }
};

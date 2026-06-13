#pragma once

#include <stdint.h>

#include "proxmox-ambient-page-model.h"

struct LedcardsInterfaceNutView {
  bool offline;
  bool upsAvailable;
  bool upsStale;
  bool onBattery;
  bool lowBattery;
  bool charging;
  int batteryPercent;
  int runtimeSeconds;
  int loadPercent;
  float inputVoltage;
  int nutClientCount;
  int ageSeconds;
  bool moduleLanConnected;
  bool wifiConnected;
  bool linkOk;
  char moduleTimeHhmm[6];
  int localBatteryPercent;
  bool localBatteryCharging;
  bool localBatteryKnown;
  uint8_t pageCount;
  uint8_t pageIndex;
  uint32_t nowMillis;
};

void createLedcardsInterfaceUi(const LedcardsInterfaceNutView &view);
void updateLedcardsInterfaceUi(const LedcardsInterfaceNutView &view);
void renderProxmoxAmbientUnavailableUi(const ProxmoxAmbientView &view, const LedcardsInterfaceNutView &statusView);
void renderProxmoxAmbientUi(const ProxmoxAmbientView &view, const LedcardsInterfaceNutView &statusView);

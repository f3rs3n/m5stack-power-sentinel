#pragma once

#include <stdint.h>

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
  uint32_t nowMillis;
};

void createLedcardsInterfaceUi(const LedcardsInterfaceNutView &view);
void updateLedcardsInterfaceUi(const LedcardsInterfaceNutView &view);

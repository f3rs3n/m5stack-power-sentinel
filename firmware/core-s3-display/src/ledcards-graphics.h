#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

constexpr uint32_t kLedcardsGreen = 0x14dc78;
constexpr uint32_t kLedcardsBlue = 0x1cb5f0;
constexpr uint32_t kLedcardsYellow = 0xfcca3d;
constexpr uint32_t kLedcardsOrange = 0xff8a2a;
constexpr uint32_t kLedcardsOrangeText = 0xffb064;
constexpr uint32_t kLedcardsRed = 0xff4e3e;
constexpr uint32_t kLedcardsRedText = 0xff6a57;
constexpr uint32_t kLedcardsGray = 0x6c7470;
constexpr uint32_t kLedcardsGrayText = 0xc1c5c1;
constexpr uint32_t kLedcardsPurple = 0x9b5cff;
constexpr uint32_t kLedcardsPurpleText = 0xc4b5fd;

struct LedcardsAmbientCardRender {
  char value[16];
  char label[12];
  char unit[8];
  char stateText[20];
  uint32_t accent;
  uint32_t fill;
  uint32_t stateColor;
};

struct LedcardsRingSlotPosition {
  int x;
  int y;
};

constexpr uint8_t kLedcardsRingSlotCount = 5;

inline void ledcardsAmbientCopy(char *dst, size_t dstSize, const char *src) {
  if (!dst || dstSize == 0) return;
  if (!src) src = "";
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

inline uint32_t ledcardsStateFill(uint32_t c) {
  if (c == kLedcardsRed) return 0x200d0c;
  if (c == kLedcardsOrange) return 0x241307;
  if (c == kLedcardsYellow) return 0x221c08;
  if (c == kLedcardsBlue) return 0x07161d;
  if (c == kLedcardsPurple) return 0x180f26;
  if (c == kLedcardsGray) return 0x101514;
  return 0x071c12;
}

inline uint32_t ledcardsStateTextColor(uint32_t c) {
  if (c == kLedcardsRed) return kLedcardsRedText;
  if (c == kLedcardsOrange) return kLedcardsOrangeText;
  if (c == kLedcardsYellow) return kLedcardsYellow;
  if (c == kLedcardsBlue) return kLedcardsBlue;
  if (c == kLedcardsPurple) return kLedcardsPurpleText;
  if (c == kLedcardsGray) return kLedcardsGrayText;
  return 0x9bb2a0;
}

inline uint32_t ledcardsVisualClassColor(const char *visualClass) {
  if (!visualClass) return kLedcardsGreen;
  if (strcmp(visualClass, "red") == 0) return kLedcardsRed;
  if (strcmp(visualClass, "orange") == 0) return kLedcardsOrange;
  if (strcmp(visualClass, "yellow") == 0) return kLedcardsYellow;
  if (strcmp(visualClass, "blue") == 0) return kLedcardsBlue;
  if (strcmp(visualClass, "purple") == 0) return kLedcardsPurple;
  if (strcmp(visualClass, "gray") == 0) return kLedcardsGray;
  return kLedcardsGreen;
}

inline LedcardsRingSlotPosition ledcardsRingSlotPosition(uint8_t slot) {
  switch (slot) {
    case 0: return {43, 57};
    case 1: return {166, 124};
    case 2: return {166, 182};
    case 3: return {12, 182};
    default: return {12, 124};
  }
}

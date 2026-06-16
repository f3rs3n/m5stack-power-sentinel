#pragma once

#include <stdint.h>

#ifndef POWER_SENTINEL_ALS_INTERPOLATION_RAW_1
#define POWER_SENTINEL_ALS_INTERPOLATION_RAW_1 12
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_RAW_2
#define POWER_SENTINEL_ALS_INTERPOLATION_RAW_2 45
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_RAW_3
#define POWER_SENTINEL_ALS_INTERPOLATION_RAW_3 70
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_RAW_4
#define POWER_SENTINEL_ALS_INTERPOLATION_RAW_4 95
#endif
#ifndef POWER_SENTINEL_ALS_INTERPOLATION_MAX_RAW
#define POWER_SENTINEL_ALS_INTERPOLATION_MAX_RAW 135
#endif

#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_0
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_0 20
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_1
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_1 20
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_2
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_2 21
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_3
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_3 21
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_4
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_4 22
#endif
#ifndef POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_5
#define POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_5 22
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_0
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_0 21
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_1
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_1 22
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_2
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_2 23
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_3
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_3 24
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_4
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_4 25
#endif
#ifndef POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_5
#define POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_5 25
#endif

enum class PsBrightnessMode : uint8_t {
  Dim,
  Awake,
};

constexpr uint8_t kPsBrightnessPointCount = 6;
constexpr uint16_t kPsAlsInterpolationRaw[kPsBrightnessPointCount] = {
  0,
  POWER_SENTINEL_ALS_INTERPOLATION_RAW_1,
  POWER_SENTINEL_ALS_INTERPOLATION_RAW_2,
  POWER_SENTINEL_ALS_INTERPOLATION_RAW_3,
  POWER_SENTINEL_ALS_INTERPOLATION_RAW_4,
  POWER_SENTINEL_ALS_INTERPOLATION_MAX_RAW,
};
constexpr uint8_t kPsDimBacklightLevels[kPsBrightnessPointCount] = {
  POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_0,
  POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_1,
  POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_2,
  POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_3,
  POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_4,
  POWER_SENTINEL_ALS_DIM_BACKLIGHT_LEVEL_5,
};
constexpr uint8_t kPsAwakeBacklightLevels[kPsBrightnessPointCount] = {
  POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_0,
  POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_1,
  POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_2,
  POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_3,
  POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_4,
  POWER_SENTINEL_ALS_AWAKE_BACKLIGHT_LEVEL_5,
};

inline uint8_t psCoreS3BacklightLevelForBrightness(uint8_t brightness) {
  return brightness == 0 ? 0 : static_cast<uint8_t>((static_cast<uint16_t>(brightness) + 641U) >> 5);
}

inline uint8_t psBrightnessForCoreS3BacklightLevel(uint8_t level) {
  if (level == 0) return 0;
  int16_t lower = static_cast<int16_t>(static_cast<uint16_t>(level) * 32U) - 641;
  int16_t upper = static_cast<int16_t>(static_cast<uint16_t>(level + 1U) * 32U) - 642;
  if (lower < 1) lower = 1;
  if (upper > 255) upper = 255;
  if (upper < lower) upper = lower;
  return static_cast<uint8_t>((lower + upper) / 2);
}

inline const uint8_t *psBacklightCurveForMode(PsBrightnessMode mode) {
  return mode == PsBrightnessMode::Dim ? kPsDimBacklightLevels : kPsAwakeBacklightLevels;
}

inline uint8_t psInterpolateBacklightLevel(const uint8_t curve[kPsBrightnessPointCount], uint16_t raw) {
  if (raw <= kPsAlsInterpolationRaw[0]) return curve[0];
  for (uint8_t i = 1; i < kPsBrightnessPointCount; ++i) {
    uint16_t lowerRaw = kPsAlsInterpolationRaw[i - 1];
    uint16_t upperRaw = kPsAlsInterpolationRaw[i];
    if (upperRaw <= lowerRaw) continue;
    if (raw <= upperRaw) {
      int32_t lowerLevel = curve[i - 1];
      int32_t upperLevel = curve[i];
      int32_t span = static_cast<int32_t>(upperRaw - lowerRaw);
      int32_t position = static_cast<int32_t>(raw - lowerRaw);
      int32_t value = lowerLevel + ((upperLevel - lowerLevel) * position + (span / 2)) / span;
      if (value < 0) return 0;
      if (value > 255) return 255;
      return static_cast<uint8_t>(value);
    }
  }
  return curve[kPsBrightnessPointCount - 1];
}

inline uint8_t psBacklightLevelForRaw(uint16_t raw, PsBrightnessMode mode) {
  return psInterpolateBacklightLevel(psBacklightCurveForMode(mode), raw);
}

inline uint8_t psBrightnessForRaw(uint16_t raw, PsBrightnessMode mode) {
  return psBrightnessForCoreS3BacklightLevel(psBacklightLevelForRaw(raw, mode));
}

inline uint8_t psMinBacklightLevelForMode(PsBrightnessMode mode) {
  return psBacklightCurveForMode(mode)[0];
}

inline uint8_t psMaxBacklightLevelForMode(PsBrightnessMode mode) {
  return psBacklightCurveForMode(mode)[kPsBrightnessPointCount - 1];
}

#include "m5_hal.h"

#include <M5Unified.h>

#if __has_include("power_sentinel_config.h")
#include "power_sentinel_config.h"
#else
#include "power_sentinel_config.example.h"
#endif

#ifndef POWER_SENTINEL_ALS_SENSOR_ENABLED
#define POWER_SENTINEL_ALS_SENSOR_ENABLED 1
#endif

#if POWER_SENTINEL_ALS_SENSOR_ENABLED
#include <utility/LTR5XX.h>

namespace {
Ltr5xx_Init_Basic_Para ltr553Config = LTR5XX_BASE_PARA_CONFIG_DEFAULT;
LTR5XX ltr553;
bool ltr553Ready = false;
}  // namespace
#endif

void psM5Begin(bool stackPowerOut) {
  auto cfg = M5.config();
  // CoreS3 stacked 5V bus direction is deliberately configurable:
  // - false: accept power from the LLM Mate/Module/base without feeding 5V back.
  // - true:  feed 5V from CoreS3 USB-C to the M-Bus/stack.
  cfg.output_power = stackPowerOut;
  M5.begin(cfg);
  M5.Power.setChargeCurrent(200);
}

void psM5Update() {
  M5.update();
}

void psDisplayWritePixels(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *pixels, bool swapBytes) {
  M5.Display.startWrite();
  M5.Display.setAddrWindow(x, y, w, h);
  M5.Display.writePixels(pixels, static_cast<size_t>(w) * static_cast<size_t>(h), swapBytes);
  M5.Display.endWrite();
}

void psDisplaySetRotation(uint8_t rotation) {
  M5.Display.setRotation(rotation);
}

void psDisplaySetBrightness(uint8_t brightness) {
  M5.Display.setBrightness(brightness);
}

bool psTouchPressed(int32_t *x, int32_t *y) {
  M5.update();
  auto detail = M5.Touch.getDetail();
  if (!detail.isPressed()) return false;
  if (x) *x = detail.x;
  if (y) *y = detail.y;
  return true;
}

bool psAmbientLightBegin() {
#if POWER_SENTINEL_ALS_SENSOR_ENABLED
  if (ltr553Ready) return true;
  ltr553Config.ps_led_pulse_freq = LTR5XX_LED_PULSE_FREQ_40KHZ;
  ltr553Config.ps_measurement_rate = LTR5XX_PS_MEASUREMENT_RATE_50MS;
  ltr553Config.als_gain = LTR5XX_ALS_GAIN_48X;
  ltr553Ready = ltr553.begin(&ltr553Config);
  if (!ltr553Ready) return false;
  ltr553.setAlsMode(LTR5XX_ALS_ACTIVE_MODE);
  return true;
#else
  return false;
#endif
}

bool psAmbientLightRead(uint16_t *alsRaw) {
#if POWER_SENTINEL_ALS_SENSOR_ENABLED
  if (!ltr553Ready || alsRaw == nullptr) return false;
  *alsRaw = ltr553.getAlsValue();
  return true;
#else
  (void)alsRaw;
  return false;
#endif
}

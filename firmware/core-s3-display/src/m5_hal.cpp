#include "m5_hal.h"

#include <M5Unified.h>

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

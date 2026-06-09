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

  if (!stackPowerOut) {
    // M5Unified::Power.setExtOutput(false) clears CoreS3 BUS_EN but also clears
    // AW9523 BOOST_EN. On the CoreS3 + LLM Kit Power Sentinel stack this can
    // make the display/power path go dark after boot even though the firmware
    // display standby state is still AWAKE. Re-enable only BOOST_EN here; do not
    // re-enable BUS_EN, so the CoreS3 still does not feed 5V back into M-Bus.
    static constexpr uint8_t kAw9523Addr = 0x58;
    static constexpr uint8_t kAw9523Port1OutputReg = 0x03;
    static constexpr uint8_t kCoreS3BoostEnableBit = 0x80;
    M5.In_I2C.bitOn(kAw9523Addr, kAw9523Port1OutputReg, kCoreS3BoostEnableBit, 400000);
  }
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

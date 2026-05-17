#pragma once

#include <Arduino.h>
#include <stdint.h>

void psM5Begin(bool stackPowerOut);
void psM5Update();
void psDisplayWritePixels(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t *pixels, bool swapBytes);
void psDisplaySetRotation(uint8_t rotation);
void psDisplaySetBrightness(uint8_t brightness);
bool psTouchPressed(int32_t *x, int32_t *y);

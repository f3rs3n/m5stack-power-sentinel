#include <Arduino.h>
#include <M5Unified.h>
#include <utility/LTR5XX.h>

#ifndef POWER_SENTINEL_ALS_PROBE_INTERVAL_MS
#define POWER_SENTINEL_ALS_PROBE_INTERVAL_MS 250UL
#endif

Ltr5xx_Init_Basic_Para ltr553Config = LTR5XX_BASE_PARA_CONFIG_DEFAULT;
LTR5XX ltr553;
uint32_t lastSampleMs = 0;
bool sensorReady = false;

void setup() {
  auto cfg = M5.config();
  cfg.output_power = false;
  M5.begin(cfg);
  M5.Power.setChargeCurrent(200);

  Serial.begin(115200);
  delay(300);
  Serial.println("# Power Sentinel CoreS3 LTR553 ALS development probe");
  Serial.println("# CSV columns: ms,als_raw");

  ltr553Config.ps_led_pulse_freq = LTR5XX_LED_PULSE_FREQ_40KHZ;
  ltr553Config.ps_measurement_rate = LTR5XX_PS_MEASUREMENT_RATE_50MS;
  ltr553Config.als_gain = LTR5XX_ALS_GAIN_48X;

  sensorReady = ltr553.begin(&ltr553Config);
  if (!sensorReady) {
    Serial.println("# ERROR: LTR553 init failed");
    return;
  }

  ltr553.setAlsMode(LTR5XX_ALS_ACTIVE_MODE);
  Serial.println("ms,als_raw");
}

void loop() {
  M5.update();
  if (!sensorReady) {
    delay(1000);
    return;
  }

  const uint32_t now = millis();
  if (now - lastSampleMs < POWER_SENTINEL_ALS_PROBE_INTERVAL_MS) return;
  lastSampleMs = now;

  const uint16_t alsRaw = ltr553.getAlsValue();
  Serial.printf("%lu,%u\n", static_cast<unsigned long>(now), alsRaw);
}

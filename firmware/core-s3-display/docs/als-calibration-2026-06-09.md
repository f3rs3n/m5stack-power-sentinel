# CoreS3 LTR553 ALS calibration — 2026-06-09

Development probe: `m5stack-cores3-als-probe`

Setup:
- CoreS3 probe flashed through DOOMTRAIN / COM4.
- Sensor: integrated LTR553 ALS only.
- Serial CSV: `ms,als_raw`, 250 ms interval.
- Light: `light.plafoniera_studio`, fixed `color_temp_kelvin=3200` during steps.
- Monitor PC turned off for the final run after an initial contaminated partial run.
- Each step: ~12 s stabilization, then ~10 s sampled.

Initial light state before final calibration:
- state: on
- brightness: 255
- color_temp_kelvin: 2732

Final calibration results:

| Hue brightness | ALS count | min | max | mean | median | last |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| off | 41 | 0 | 0 | 0.00 | 0.00 | 0 |
| 1% | 41 | 0 | 0 | 0.00 | 0.00 | 0 |
| 5% | 41 | 0 | 0 | 0.00 | 0.00 | 0 |
| 10% | 41 | 1 | 1 | 1.00 | 1.00 | 1 |
| 25% | 41 | 7 | 8 | 7.76 | 8.00 | 7 |
| 50% | 41 | 32 | 33 | 32.59 | 33.00 | 32 |
| 75% | 41 | 74 | 75 | 74.44 | 74.00 | 74 |
| 100% | 41 | 135 | 136 | 135.10 | 135.00 | 135 |

Notes:
- The sensor/probe is working and stable: low jitter, mostly one-count variation per step.
- Values are raw LTR553 ALS values, not lux.
- Low end is compressed: 0–5% Hue reads 0, 10% reads 1.
- Upper indoor range in this setup is ~135 raw at Hue 100% / 3200K.
- Previous partial run with PC monitor on was discarded because it influenced ALS readings.
- The plafoniera was restored to the initial state after calibration.

Preliminary raw-light ranges used to derive interpolation anchors, pending validation with daylight/other real conditions:
- B0: <= 1 raw, very dark / low light
- B1: 2–12 raw, dim indoor
- B2: 13–45 raw, normal indoor
- B3: 46–95 raw, bright indoor
- B4: >= 96 raw, very bright indoor

Do not treat these thresholds as final until natural daylight and actual installed viewing conditions are sampled.

Current firmware tuning after physical visual checks:
- Status: confirmed working and acceptable after 2026-06-11 installed-device visual checks in the user's studio. Response debounce felt good; PWM motion was slowed once and accepted for now.
- ALS is sampled every 100 ms.
- EMA uses alpha 2/3.
- Bucket debounce/raw hysteresis was removed from the runtime path after interpolation made it redundant.
- The six DIM/AWAKE entries are physical CoreS3 Backlight Level anchors, not nominal 0-255 PWM values. Current DIM anchors are `20,20,21,21,22,22`; AWAKE anchors are `21,22,23,24,25,25`.
- The continuous interpolation raw anchors are 0, 12, 45, 70, 95, and 135.
- Tiny instantaneous target changes inside `POWER_SENTINEL_ALS_TARGET_DEADBAND=2` are ignored when accepting a new target, but curve endpoints are always eligible so the accepted target can still reach the real dark/bright endpoints.
- Eligible targets are debounced asymmetrically: `POWER_SENTINEL_ALS_BRIGHTENING_DEBOUNCE_MS=300` and `POWER_SENTINEL_ALS_DARKENING_DEBOUNCE_MS=1000`, matching the real-system pattern of accepting brightening faster than transient darkening/shadows.
- The remaining accepted brightness target is applied directly (`POWER_SENTINEL_ALS_TARGET_FILTER_SHIFT=0`); smoothness is handled by the PWM slew, while deadband/debounce suppress target jitter.
- Ambient brightness changes use a 1-point gamma-derived slew with a non-linear interval: 24 ms above 80, 34 ms from 64-79, 40 ms from 48-63, and 48 ms below 48. This preserves the gamma 2.2 relative weighting while slowing the visible PWM motion to roughly 3.1 seconds for a full DIM 96→32 transition after target acceptance.
- Display mode changes still use the normal 900 ms fade.
- `DisplayMode::Off` remains brightness 0 and is not woken by ALS.

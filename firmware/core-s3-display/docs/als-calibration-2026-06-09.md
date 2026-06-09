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

Preliminary bucket idea, pending validation with daylight/other real conditions:
- B0: <= 1 raw, very dark / low light
- B1: 2–12 raw, dim indoor
- B2: 13–45 raw, normal indoor
- B3: 46–95 raw, bright indoor
- B4: >= 96 raw, very bright indoor

Do not treat these thresholds as final until natural daylight and actual installed viewing conditions are sampled.

Current firmware tuning after physical visual checks:
- ALS is sampled every 100 ms.
- EMA uses alpha 2/3.
- Bucket debounce is 250 ms with raw hysteresis 2, but buckets are now classification/debug state only; brightness target comes from continuous ALS EMA interpolation.
- The 5 DIM/AWAKE brightness values are interpolation anchors, not direct output steps.
- The continuous interpolation raw anchors are 0, 12, 45, 95, and 135.
- Ambient brightness changes are applied with a 1-point / 30 ms slew limiter.
- Display mode changes still use the normal 900 ms fade.
- `DisplayMode::Off` remains brightness 0 and is not woken by ALS.

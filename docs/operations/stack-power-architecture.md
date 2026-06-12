# Stack power architecture notes

These notes capture the observed and schematic-backed power behavior of the current Power Sentinel hardware stack.

## Physical stack

Current order, bottom to top:

```text
DIN Base -> LLM Mate -> LLM Module -> CoreS3
```

The DIN Base physical switch must be ON for its battery/BAT rail to be visible through the stack.

## Relevant M5-Bus rails

From the DIN Base and LLM Mate schematics:

- M5-Bus pin 30: `BAT` / `BATTERY` / `BUS_BAT`
- M5-Bus pin 28: `5V` / `+5V` / `BUS_5V`
- M5-Bus pins 25, 27, 29: `HPWR`

## DIN Base power path

DIN Base components/docs:

- Battery: 1S LiPo, 500 mAh
- Charger: `TP4057`
- Buck converter: `SY8303AIC`
- External input: DC 9-24V

Observed/schematic behavior:

- Battery positive (`BAT+`) is connected to `BUS_BAT` / M5-Bus pin 30 through the DIN Base power switch path.
- `BUS_BAT` is an analog battery rail, not a digital fuel-gauge interface.
- The TP4057 handles charging and local charge/standby LEDs; its status pins are not exposed as a CoreS3-readable M5-Bus telemetry interface.
- The SY8303AIC buck generates `BUS_5V` from the external/high-power input path, not from the DIN battery.
- No BAT-to-5V boost converter is visible in the DIN Base schematic.

Implication: the DIN Base battery can feed `BAT`, but it does not create a 5V UPS rail for the whole stack.

## LLM Mate power path

LLM Mate schematic observations:

- Two `M5Stack_BUS` connectors expose/pass the same major rails, including `HPWR`, `+5V`, and `BATTERY`.
- USB-C `VBUS` feeds the local `+5V` net through `F1 6V/1A/PPTC`.
- The USB-C section includes Type-C CC resistors, ESD/protection parts, and a CH340N USB-serial circuit.
- A `B5819W` diode is visible in the local CH340/power area, but no BATTERY-to-+5V boost converter is visible.
- No inductor/controller path was found that would convert M5-Bus pin 30 `BATTERY` into `+5V` for the LLM Module.

Implication: LLM Mate can pass the BAT rail upward/downward, and can provide/use +5V when USB/external 5V is present, but it does not appear to boost DIN battery voltage into the LLM-side +5V rail.

## CoreS3 battery readings

With the DIN Base switch OFF:

- CoreS3 saw valid VBUS but an invalid/absent battery reading.

With the DIN Base switch ON:

- A temporary CoreS3 probe saw the BAT rail through AXP2101:
  - initial settled-in-progress reading: `battery_mv` about 4190 mV, `charging=1`, low SOC percentage;
  - later settled reading: `battery_level=100`, `battery_mv` about 4120 mV, `charging=0`.

Interpretation:

- CoreS3 can read the DIN battery rail through its PMIC/AXP2101 when the DIN switch is ON.
- The top-bar battery icon is local CoreS3/DIN BAT rail status, not UPS/NUT status and not Linux/LLM-module power status.

## Physical power-loss test

Short test performed on the assembled stack:

- USB power removed from the LLM Mate/LLM Module side.
- CoreS3 stayed powered.
- LLM Mate and LLM Module powered off.

Conclusion:

- The DIN battery acts as local backup for the CoreS3/display path.
- The DIN battery does not keep the Linux/LLM side alive in this stack.
- On LLM-side power loss, the CoreS3 should remain alive long enough to show serial/link stale/down, but backend/StackFlow/LAN/time updates disappear because the LLM Module is off.

## Practical design implications

For Power Sentinel:

- Keep the top-bar battery semantic as local CoreS3/DIN BAT status only.
- Treat LAN/time/link status as the authoritative indication that the LLM Linux side is alive.
- Do not rely on DIN Base battery as a full-stack UPS.
- If full-stack battery operation is desired, use a 5V/DC UPS or a different base/module with an explicit battery-to-5V power path. A hardware BAT-to-5V boost mod would need careful backfeed/current/protection design and is not part of the current project baseline.

StackChan comparison:

- M5Stack StackChan uses a different robot body/base with its own 550 mAh battery, AXP2101 power management, INA226 battery monitor, servo/LED power control, and different power architecture.
- StackChan showing battery-powered operation with related modules does not imply DIN Base can power the LLM Module from its 500 mAh BAT rail.

#!/usr/bin/env python3
"""Static guard for the clean Power Sentinel NUT Monitor baseline."""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "core-s3-display" / "src" / "main.cpp"
LEDCARDS_CPP = ROOT / "firmware" / "core-s3-display" / "src" / "ledcards-interface-page.cpp"
LEDCARDS_H = ROOT / "firmware" / "core-s3-display" / "src" / "ledcards-interface-page.h"
NUT_FIXTURE = ROOT / "assets" / "lvgl-spike" / "power-sentinel-nut-ledcards-interface-fixture.c"


def fail(message: str) -> int:
    print(f"FAIL core-s3-ui: {message}")
    return 1


def require(text: str, needles: list[str], context: str) -> int:
    for needle in needles:
        if needle not in text:
            return fail(f"{context} missing {needle!r}")
    return 0


def main() -> int:
    if not MAIN.exists() or not LEDCARDS_CPP.exists() or not LEDCARDS_H.exists():
        return fail("firmware NUT monitor sources are missing")
    main = MAIN.read_text(encoding="utf-8")
    ledcards = LEDCARDS_CPP.read_text(encoding="utf-8") + "\n" + LEDCARDS_H.read_text(encoding="utf-8")
    if require(main, [
        "nut-monitor-clean-baseline",
        "createLedcardsInterfaceUi(makeLedcardsInterfaceNutView())",
        "updateLedcardsInterfaceUi(makeLedcardsInterfaceNutView())",
        "POWER_SENTINEL_TRANSPORT_SERIAL",
        "Serial2.begin(POWER_SENTINEL_UART_BAUD, SERIAL_8N1, POWER_SENTINEL_UART_RX_PIN, POWER_SENTINEL_UART_TX_PIN)",
        "work_id\"] = \"sentinel\"",
        "POWER_SENTINEL_HTTP_FALLBACK",
        "POWER_SENTINEL_DISPLAY_STANDBY_MS",
        "POWER_SENTINEL_DISPLAY_DIM_TO_OFF_MS",
        "POWER_SENTINEL_DISPLAY_SNOOZE_MS",
        "POWER_SENTINEL_MOTION_WAKE",
        "DisplayMode::Dim",
        "manualDisplaySnoozeActive",
        "buildStateSignature",
        "state.nut.clientCount",
        "if (displayMode != DisplayMode::Off) refreshLedcardsUi();",
        "kLedcardsSleepLongPressMs = 3000",
        "psDisplaySetBrightness(0)",
    ], "main NUT monitor"):
        return 1
    forbidden = [
        "renderHome(", "renderProxmox(", "renderHa(", "renderM5s(", "tabview", "PS_ICON_HOME", "Zigbee2MqttState",
        "ProxmoxState", "WorkloadMetric", "HOME", "M5S", "Running workloads", "HA OK", "PVE OK",
    ]
    for needle in forbidden:
        if needle in main:
            return fail(f"clean NUT firmware still contains legacy multi-page marker {needle!r}")
    if require(ledcards, [
        "struct LedcardsInterfaceNutView",
        "createLedcardsInterfaceUi(const LedcardsInterfaceNutView &view)",
        "updateLedcardsInterfaceUi(const LedcardsInterfaceNutView &view)",
        "METRIC_BATTERY", "METRIC_TTE", "METRIC_LOAD", "METRIC_INPUT", "METRIC_NUT",
        "format_tte_full", "format_tte_minutes", "CRITICAL RUNTIME", "LOW BATTERY", "UNAVAILABLE",
        "OL CHRG LB", "avoid flapping between a red",
        "start_ring_transition", "on_tile_clicked",
    ], "Ledcards Interface"):
        return 1
    if "  }\n  if (view.lowBattery || view.batteryPercent < 20) return \"LOW BATTERY\";" in ledcards:
        return fail("online battery display must not let NUT LB override percent buckets")
    if not NUT_FIXTURE.exists():
        return fail("NUT Ledcards LVGL MCP fixture is missing")
    fixture = NUT_FIXTURE.read_text(encoding="utf-8")
    if require(fixture, ["NUT", "Runtime", "Battery", "Input", "Load", "STATE_LOW_BATTERY"], "NUT fixture"):
        return 1
    print("PASS core-s3-ui clean NUT monitor guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

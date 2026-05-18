#!/usr/bin/env python3
"""Static checks for the CoreS3 LVGL page structure.

This is a lightweight no-dependency guard for the embedded UI contract. It does
not replace the PlatformIO build; it catches accidental regressions in the tab
set and obvious stale page names before we flash hardware.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "core-s3-display" / "src" / "main.cpp"
EXPECTED_TABS = ["HOME", "NUT", "PVE", "HA", "M5S"]
FORBIDDEN_TABS = ["UPS", "Offline", "M5"]
REQUIRED_RENDERERS = ["renderHome", "renderNut", "renderProxmox", "renderHa", "renderM5s"]


def fail(message: str) -> int:
    print(f"FAIL core-s3-ui: {message}")
    return 1


def main() -> int:
    text = MAIN.read_text(encoding="utf-8")
    tabs = re.findall(r'lv_tabview_add_tab\(tabview,\s*"([^"]+)"\)', text)
    if tabs != EXPECTED_TABS:
        return fail(f"expected tabs {EXPECTED_TABS}, found {tabs}")
    for tab in FORBIDDEN_TABS:
        if tab in tabs:
            return fail(f"legacy tab {tab!r} still present")
    for renderer in REQUIRED_RENDERERS:
        if f"void {renderer}()" not in text:
            return fail(f"missing renderer {renderer}()")
    if text.count("addPercentBar(") < 5:
        return fail("V1a functional UI should render multiple bars for HOME/NUT metrics")
    if 'NET UNK' not in text:
        return fail("HOME must render NET UNK until a real network probe exists")
    if 'state.proxmox.available ? "OK" : "UNK"' in text:
        return fail("HOME NET status must not be derived from Proxmox reachability")
    if "state.ha.available && state.ha.mqtt" not in text:
        return fail("HOME HA status must require both HA API and MQTT in V1a")
    required_v1b = {
        "applyAppTheme()": "V1b should apply a global modern theme",
        "makeHeroCard(": "V1b should use a hero card for the HOME overview",
        "addMetricRow(": "V1b should use structured metric rows instead of plain line-only layout",
        "addStatusPillRow(": "V1b should render compact status pills for dense overview pages",
        "LV_OPA_60": "V1b should use subtle translucent/shadow styling",
        "lv_obj_set_style_text_font": "V1b should use deliberate font hierarchy",
    }
    for needle, message in required_v1b.items():
        if needle not in text:
            return fail(message)
    render_all_match = re.search(r"void renderAll\(\) \{(?P<body>.*?)\n\}", text, re.S)
    if not render_all_match:
        return fail("missing renderAll()")
    body = render_all_match.group("body")
    expected_order = [f"{name}();" for name in REQUIRED_RENDERERS]
    positions = [body.find(call) for call in expected_order]
    if any(pos < 0 for pos in positions):
        return fail(f"renderAll() does not call every renderer in {expected_order}")
    if positions != sorted(positions):
        return fail("renderAll() renderer order does not match tab order")
    print("PASS core-s3-ui tabs HOME/NUT/PVE/HA/M5S")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

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
SPIKE_DIR = ROOT / "assets" / "lvgl-spike"
SPIKE_HOME_FIXTURE = SPIKE_DIR / "power-sentinel-home-online.c"
SPIKE_RENDER_SCRIPT = SPIKE_DIR / "run-lvgl-mcp-render.mjs"
EXPECTED_TAB_LABEL_MARKERS = ["LV_SYMBOL_HOME", "LV_SYMBOL_CHARGE", "LV_SYMBOL_DRIVE", "LV_SYMBOL_WIFI", "LV_SYMBOL_SETTINGS"]
FORBIDDEN_TABS = ["UPS", "Offline"]
REQUIRED_RENDERERS = ["renderHome", "renderNut", "renderProxmox", "renderHa", "renderM5s"]


def fail(message: str) -> int:
    print(f"FAIL core-s3-ui: {message}")
    return 1


def main() -> int:
    text = MAIN.read_text(encoding="utf-8")
    tab_calls = re.findall(r'lv_tabview_add_tab\(tabview,\s*([^\)]+)\)', text)
    if len(tab_calls) != 5:
        return fail(f"expected 5 tab creation calls, found {len(tab_calls)}: {tab_calls}")
    for marker, call in zip(EXPECTED_TAB_LABEL_MARKERS, tab_calls):
        if marker not in call:
            return fail(f"expected sidebar icon tab marker {marker}, found call {call}")
    if "\\nHM" not in tab_calls[0] or "\\nNT" not in tab_calls[1] or "\\nPV" not in tab_calls[2]:
        return fail("sidebar HOME/NUT/PVE labels should use HM/NT/PV until better icons are available")
    for tab in FORBIDDEN_TABS:
        if f'lv_tabview_add_tab(tabview, "{tab}")' in text:
            return fail(f"legacy tab {tab!r} still present")
    if 'lv_tabview_set_tab_bar_position(tabview, LV_DIR_LEFT)' not in text or 'lv_tabview_set_tab_bar_size(tabview, 44)' not in text:
        return fail("CoreS3 navigation should use a compact left sidebar to preserve vertical space")
    required_horizontal_cards = [
        "PAGE_CARD_WIDTH",
        "PAGE_CARD_HEIGHT",
        "lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW)",
        "lv_obj_set_scroll_dir(tab, LV_DIR_HOR)",
        "lv_obj_set_scroll_snap_x(tab, LV_SCROLL_SNAP_CENTER)",
        "lv_obj_set_scroll_dir(tabContent, LV_DIR_VER)",
        "lv_obj_set_scroll_snap_y(tabContent, LV_SCROLL_SNAP_CENTER)",
        "lv_obj_set_scroll_dir(card, LV_DIR_VER)",
        "lv_obj_add_flag(card, LV_OBJ_FLAG_SCROLL_CHAIN_VER)",
    ]
    for needle in required_horizontal_cards:
        if needle not in text:
            return fail(f"CoreS3 cards should use horizontal per-tab carousel layout; missing {needle}")
    for renderer in REQUIRED_RENDERERS:
        if f"void {renderer}()" not in text:
            return fail(f"missing renderer {renderer}()")
    if text.count("addPercentBar(") < 5:
        return fail("V1a functional UI should render multiple bars for HOME/NUT metrics")
    if 'state.ha.available && state.ha.mqtt' not in text:
        return fail("HOME HA status must require both HA API and MQTT in V1a")
    if 'severityText()' not in text:
        return fail("HOME severity badge should render uppercase OK/WARN/CRITICAL text")
    if 'NetworkState' not in text or 'state.network.available' not in text or 'NET %s' not in text:
        return fail("HOME must render NET from the real network probe, not a placeholder")
    required_sleep = ["enterDisplaySleep", "wakeDisplay", "addHomeSleepButton", "SLEEP", "psDisplaySetBrightness(0)"]
    for needle in required_sleep:
        if needle not in text:
            return fail(f"HOME display sleep control missing {needle}")
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
    required_pve = ["cpuPercent", "ramPercent", "storagePercent", "zfsStatus", "smartStatus", "vmRunningCount", "lxcRunningCount", "workloadMetricCount", "running_items", "makeWorkloadMiniCard"]
    for needle in required_pve:
        if needle not in text:
            return fail(f"PVE read-only UI missing {needle}")
    required_pve_clarity = ["CPU", "RAM", "HDD", "Temp n/a", "Storage", "NUT monitor", "addStatusPillRow(card,", "PVE RO", "ramTotalGb", "diskTotalGb"]
    for needle in required_pve_clarity:
        if needle not in text:
            return fail(f"PVE UI lacks display clarity marker {needle}")
    for stale in ["CPU bar", "RAM bar", "Storage bar", "addMetricRow(card, \"usage\"", "addMetricRow(card, \"workloads\""]:
        if stale in text:
            return fail(f"PVE UI still contains redundant marker {stale}")
    if "Shutdown via NUT" in text:
        return fail("PVE tab should not show ambiguous standalone 'Shutdown via NUT'")
    required_ha_z2m = ["Zigbee2MqttState", "state.zigbee2mqtt.available", "coordinatorType", "deviceTotal", "Z2M", "Coordinator", "Updates %d", "Z2M devices:"]
    for needle in required_ha_z2m:
        if needle not in text:
            return fail(f"HA/Z2M UI missing {needle}")
    if "HA birth topic" in text or "Version %s" in text:
        return fail("HA tab should not waste display space on HA birth topic or Z2M version")
    required_shutdown = ["ShutdownState", "owner", "upsmon", "primary monitor", "nutClients", "clientSummary", "reachable_via_upsc", "connected_as_upsmon"]
    for needle in required_shutdown:
        if needle not in text:
            return fail(f"NUT shutdown/readiness UI missing {needle}")
    forbidden_shutdown = ["DRY-RUN", "dry-run"]
    for needle in forbidden_shutdown:
        if needle in text:
            return fail(f"NUT UI still contains ambiguous shutdown wording {needle}")

    if not SPIKE_RENDER_SCRIPT.exists():
        return fail("LVGL MCP visual render harness is missing")
    render_script = SPIKE_RENDER_SCRIPT.read_text(encoding="utf-8")
    for needle in ["--source", "--name", "lvgl_render_full", "lvgl_set_resolution"]:
        if needle not in render_script:
            return fail(f"LVGL MCP render harness missing {needle}")

    if not SPIKE_HOME_FIXTURE.exists():
        return fail("LVGL MCP HOME visual fixture is missing")
    fixture = SPIKE_HOME_FIXTURE.read_text(encoding="utf-8")
    required_fixture_strings = [
        "POWER SENTINEL",
        "HOME", "NUT", "PVE", "HA", "M5S",
        "GRID ONLINE",
        "battery / runtime",
        "load / input",
        "NUT OK", "PVE OK", "HA OK",
        "NET OK", "M5S OK",
        "SLEEP DISPLAY",
    ]
    for needle in required_fixture_strings:
        if needle not in fixture:
            return fail(f"LVGL MCP HOME fixture missing {needle!r}")

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
    print("PASS core-s3-ui sidebar + horizontal card carousel HOME/NUT/PVE/HA/M5S")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

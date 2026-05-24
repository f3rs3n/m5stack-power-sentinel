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
SPIKE_DASHBOARD_FIXTURE = SPIKE_DIR / "power-sentinel-dashboard-fixture.c"
SPIKE_RENDER_SCRIPT = SPIKE_DIR / "run-lvgl-mcp-render.mjs"
SPIKE_BATCH_RENDER_SCRIPT = SPIKE_DIR / "render-power-sentinel-tabs.mjs"
EXPECTED_TAB_LABEL_MARKERS = ["PS_ICON_HOME", "PS_ICON_NUT", "PS_ICON_SERVER", "PS_ICON_HOME_ASSISTANT", "PS_ICON_SETTINGS"]
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
            return fail(f"expected sidebar Nerd Font tab marker {marker}, found call {call}")
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
    required_pve = ["cpuPercent", "ramPercent", "ram_total_bytes", "ramTotalGb", "storagePercent", "storage_total_bytes", "storageTotalGb", "active_network_interfaces", "networkIface1", "zfsStatus", "smartStatus", "vmRunningCount", "lxcRunningCount", "workloadMetricCount", "running_items", "makeWorkloadMiniCard", "makeWorkloadInfoMiniCard"]
    for needle in required_pve:
        if needle not in text:
            return fail(f"PVE UI missing {needle}")
    required_pve_clarity = ["CPU", "RAM", "HDD", "Storage", "NUT monitor", "healthRow", "ONLINE", "OFFLINE", "ZFS online", "SMART ok", "ramTotalGb", "storageTotalGb", "diskTotalGb", '"LXC"']
    for needle in required_pve_clarity:
        if needle not in text:
            return fail(f"PVE UI lacks display clarity marker {needle}")
    for stale in ["Running workloads", "node / api", "CPU bar", "RAM bar", "Storage bar", "Temp n/a", '"temp / storage"', '"CT"', "PVE RO", "PVE OK", "addMetricRow(card, \"usage\"", "addMetricRow(card, \"workloads\""]:
        if stale in text:
            return fail(f"PVE UI still contains redundant marker {stale}")
    for needle in ["addCompactMetricWithBar(card, \"RAM\"", "PVE API %s", 'percent < 0 ? "--"']:
        if needle not in text:
            return fail(f"PVE polish missing {needle}")
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
    helper_script_path = SPIKE_DIR / "lvgl-mcp-render-helper.mjs"
    if not helper_script_path.exists():
        return fail("LVGL MCP reusable render helper is missing")
    render_harness_text = render_script + "\n" + helper_script_path.read_text(encoding="utf-8")
    for needle in ["--source", "--name", "--define", "lvgl_render_full", "lvgl_set_resolution", "taskkill"]:
        if needle not in render_harness_text:
            return fail(f"LVGL MCP render harness missing {needle}")

    if not SPIKE_BATCH_RENDER_SCRIPT.exists():
        return fail("LVGL MCP dashboard batch renderer is missing")
    batch_script = SPIKE_BATCH_RENDER_SCRIPT.read_text(encoding="utf-8")
    for needle in ["home-current", "nut-current", "pve-current", "ha-current", "m5s-current", "PS_ACTIVE_TAB"]:
        if needle not in batch_script:
            return fail(f"LVGL MCP dashboard batch renderer missing {needle}")

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

    if not SPIKE_DASHBOARD_FIXTURE.exists():
        return fail("LVGL MCP dashboard fixture is missing")
    dashboard_fixture = SPIKE_DASHBOARD_FIXTURE.read_text(encoding="utf-8")
    required_dashboard_fixture = [
        "#define PS_PAGE_CARD_WIDTH 252",
        "#define PS_PAGE_CARD_HEIGHT 220",
        "lv_tabview_set_tab_bar_position(tv, LV_DIR_LEFT)",
        "lv_tabview_set_tab_bar_size(tv, 44)",
        "force_sidebar_label_contrast(bar_obj)",
        "#include \"ps_ui_tab_12.c\"",
        "lv_obj_set_style_text_font(bar_obj, &ps_ui_tab_12, 0)",
        "lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW)",
        "lv_obj_set_scroll_dir(tab, LV_DIR_HOR)",
        "lv_obj_set_scroll_snap_x(tab, LV_SCROLL_SNAP_CENTER)",
        "PS_ICON_HOME", "PS_ICON_NUT", "PS_ICON_SERVER", "PS_ICON_HOME_ASSISTANT", "PS_ICON_SETTINGS",
        "lv_obj_set_style_pad_gap(card, 6, 0)",
        "lv_obj_set_style_pad_all(card, 8, 0)",
        "bar(parent, value, color, 10)",
        "lv_obj_set_width(left, total && total[0] ? 132 : lv_pct(100))",
        "lv_obj_set_width(right, 68)",
        "POWER SENTINEL", "Proxmox", "VM haos", "Home Assistant", "M5S",
    ]
    for needle in required_dashboard_fixture:
        if needle not in dashboard_fixture:
            return fail(f"LVGL MCP dashboard fixture missing {needle!r}")
    if "lv_obj_t *box = lv_obj_create(parent);" in dashboard_fixture:
        return fail("LVGL MCP dashboard fixture PVE metrics drifted from firmware: wrapper box reintroduced")
    font_file = ROOT / "firmware" / "core-s3-display" / "src" / "ps_ui_tab_12.c"
    if not font_file.exists():
        return fail("generated sidebar Nerd Font subset ps_ui_tab_12.c is missing")
    font_text = font_file.read_text(encoding="utf-8")
    for needle in ["const lv_font_t ps_ui_tab_12", "0xF013", "0xF015", "0xF233", "0xF06F8", "0xF07D0"]:
        if needle not in font_text:
            return fail(f"sidebar Nerd Font subset missing {needle}")
    if "ps_ui_tab_12.c" not in batch_script and "ps_ui_tab_12.c" not in (SPIKE_DIR / "render-via-doomtrain.sh").read_text(encoding="utf-8"):
        return fail("LVGL MCP render path must copy/include the generated sidebar font")

    render_all_match = re.search(r"void renderAll\(\) \{(?P<body>.*?)\n\}", text, re.S)
    if not render_all_match:
        return fail("missing renderAll()")
    if "renderActiveTab();" not in render_all_match.group("body"):
        return fail("renderAll() should render only the active tab on CoreS3 to keep LVGL heap bounded")
    render_tab_match = re.search(r"void renderTab\(uint32_t active\) \{(?P<body>.*?)\n\}", text, re.S)
    if not render_tab_match:
        return fail("missing lazy renderTab(uint32_t active)")
    body = render_tab_match.group("body")
    expected_order = [f"render{name.removeprefix('render')}();" for name in REQUIRED_RENDERERS]
    positions = [body.find(call) for call in expected_order]
    if any(pos < 0 for pos in positions):
        return fail(f"renderTab() does not dispatch every renderer in {expected_order}")
    if positions != sorted(positions):
        return fail("renderTab() dispatch order does not match tab order")
    for needle in ["cleanInactiveTabs(active)", "saveTabScroll(renderedTabIndex)", "restoreTabScroll(renderedTabIndex)", "lv_obj_scroll_to_x(tab, tabScrollX[index], LV_ANIM_OFF)", "lv_obj_add_event_cb(tabview, onTabChanged, LV_EVENT_VALUE_CHANGED", "forceSidebarLabelContrast(lv_tabview_get_tab_bar(tabview))", "lv_obj_set_style_shadow_width(card, 10", "lv_obj_set_style_shadow_width(card, 6"]:
        if needle not in text:
            return fail(f"CoreS3 LVGL heap guard missing {needle}")
    print("PASS core-s3-ui sidebar + horizontal card carousel HOME/NUT/PVE/HA/M5S")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

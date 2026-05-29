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
SPIKE_NUT_LEDCARDS_FIXTURE = SPIKE_DIR / "power-sentinel-nut-ledcards-interface-fixture.c"
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
    if any("\\n" in call for call in tab_calls):
        return fail("sidebar should be icon-only with the larger 18px tab icon font")
    if "ps_ui_tab_18" not in text:
        return fail("sidebar icon-only live UI should use the larger ps_ui_tab_18 font")
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
    required_pve_clarity = ["CPU", "RAM", "HDD", "Storage", "NUT armed", "NUT disarmed", "proxmox_nut_client", "proxmoxClientArmed", "healthRow", "ONLINE", "OFFLINE", "ZFS online", "SMART ok", "ramTotalGb", "storageTotalGb", "diskTotalGb", '"LXC"']
    for needle in required_pve_clarity:
        if needle not in text:
            return fail(f"PVE UI lacks display clarity marker {needle}")
    for stale in ["Running workloads", "node / api", "CPU bar", "RAM bar", "Storage bar", "Temp n/a", '"temp / storage"', '"CT"', "PVE RO", "PVE OK", "HA OK", "HA DOWN", "M5S OK", "M5S DOWN", "NUT monitor", "addMetricRow(card, \"usage\"", "addMetricRow(card, \"workloads\""]:
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
    required_shutdown = [
        "ShutdownState", "nut_upsmon", "nutClients", "clientSummary", "reachable_via_upsc", "connected_as_upsmon",
        "struct NutClientCard", "MAX_NUT_CLIENT_CARDS", "makeNutClientMiniCard", "PRIMARY", "SECONDARY",
        "DISARMED", "PAGE_CARD_HEIGHT - 8) / 2"
    ]
    for needle in required_shutdown:
        if needle not in text:
            return fail(f"NUT functional-ready UI missing {needle}")
    required_mini_nutify = [
        "makeStatusCard(nutTab, \"UPS\"", "Model %s", "Runtime", "Power", "makeStatusCard(nutTab, \"BATTERY\"",
        "batteryStatusBadge()", "batteryStatusColor()", "Battery Charge", "Battery Voltage", "CHARGING", "DISCHARGING",
        "POWER", "Power Usage", "System Load", "Input Voltage", "PROTECTION", "lv_obj_set_height(card, 44)",
        "Connected clients %d/%d", "state.shutdown.clientConnected", "state.shutdown.clientTotal", "makeNutClientMiniCard(protection, client)"
    ]
    for needle in required_mini_nutify:
        if needle not in text:
            return fail(f"Mini Nutify NUT tab missing dynamic semantic marker {needle}")
    forbidden_shutdown = [
        "DRY-RUN", "dry-run", "Status: %s (%s)", "mode %s", "owner %s", "NUT shutdown readiness", "primary monitor", "client list:",
        "NUT details", "NUT upsmon:", "clients %d / %d", "Capacity %s", "local NUT upsmon", "downstream client",
        "Battery Status %s"
    ]
    for needle in forbidden_shutdown:
        if needle in text:
            return fail(f"NUT UI still contains ambiguous/details wording {needle}")

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

    ledcards_interface_cpp = ROOT / "firmware" / "core-s3-display" / "src" / "ledcards-interface-page.cpp"
    ledcards_interface_h = ROOT / "firmware" / "core-s3-display" / "src" / "ledcards-interface-page.h"
    if not ledcards_interface_cpp.exists() or not ledcards_interface_h.exists():
        return fail("Ledcards Interface live page source/header are missing")
    ledcards_interface_text = ledcards_interface_cpp.read_text(encoding="utf-8")
    ledcards_interface_header = ledcards_interface_h.read_text(encoding="utf-8")
    for needle in [
        "struct LedcardsInterfaceNutView", "createLedcardsInterfaceUi(const LedcardsInterfaceNutView &view)", "updateLedcardsInterfaceUi(const LedcardsInterfaceNutView &view)",
        "LV_FONT_DECLARE(ps_icon_chart_32)", "LV_FONT_DECLARE(ps_font_ddin_condensed_bold_40)", "&ps_font_ddin_condensed_bold_40", "box(screen, x, y, 142, 46", "box(t, 7, 8, 5, 28", "label(t, m.value, 20, 8", "label(t, m.label, 76, 6", "label(t, m.unit, 78, 23", "hero_card(screen, kHeroCardX, kHeroCardY, hero, true)", "box(card, 7, 8, 7, 76", "label(card, hero.value, 31, 8", "label(card, hero.label, label_x, 8", "label(card, hero.unit, label_x, 38", "label(card, hero.stateText, 33, 71", "chart_icon(card, 251, 31)", "const int xs[4] = {166, 166, 12, 12}", "const int ys[4] = {124, 182, 182, 124}", "\\xF3\\xB1\\x95\\x8D", "lv_obj_set_pos(hit, x, y)", "lv_obj_set_size(hit, 34, 34)",
        "choose_hero_metric", "rotate_metric_to_hero", "slot 0 HERO, slot 1 top-right", "kHeroCooldownMs", "kTouchHeroOverrideMs = 60000", "on_tile_clicked", "touchHeroOverrideMetric", "format_tte_full", "format_tte_minutes", "compactTte", "metric_for(metricOrder[i], view, true)", "STATE_ON_BATTERY", "STATE_LOW_BATTERY",
        "STATE_STALE", "STATE_HIGH_LOAD", "STATE_INPUT_LOW", "METRIC_BATTERY", "METRIC_TTE", "METRIC_LOAD", "METRIC_INPUT", "METRIC_NUT", "m.label = \"Runtime\"",
        "CRITICAL RUNTIME", "SHORT RUNTIME", "CRITICAL RESERVE", "CRITICAL BATTERY", "LOW BATTERY", "HIGH LOAD", "OVERLOAD", "GRID OFFLINE", "LOW RESERVE", "UNAVAILABLE", "kPurple",
    ]:
        if needle not in ledcards_interface_text + "\n" + ledcards_interface_header:
            return fail(f"Ledcards Interface live-data adapter missing {needle}")
    mini_value_marker = "label(t, m.value, 20, 8, 58, &ps_font_ddin_condensed_bold_40, 0xf5f6f2)"
    mini_name_marker = "label(t, m.label, 76, 6, 55, &lv_font_montserrat_12, 0xc9d0c9)"
    mini_unit_marker = "label(t, m.unit, 78, 23, 53, &lv_font_montserrat_12, 0x87918c)"
    mini_value_idx = ledcards_interface_text.find(mini_value_marker)
    mini_name_idx = ledcards_interface_text.find(mini_name_marker)
    mini_unit_idx = ledcards_interface_text.find(mini_unit_marker)
    if mini_value_idx == -1 or mini_name_idx == -1 or mini_unit_idx == -1:
        return fail("Ledcards Interface mini-card missing value/unit foreground markers")
    if mini_value_idx < mini_unit_idx:
        return fail("Ledcards Interface mini-card value must be created after label/unit objects so it stays in foreground")
    for needle in ["lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE)", "lv_obj_add_event_cb(hit, on_tile_clicked, LV_EVENT_CLICKED", "start_ring_transition(lv_screen_active(), touchHeroOverrideMetric, lastRenderedView)"]:
        if needle not in ledcards_interface_text:
            return fail(f"Ledcards Interface mini-card touch-to-hero override missing {needle}")
    required_ring_animation = [
        "ringAnimationActive", "ringAnimationOverlay", "pendingAnimationView", "RingGhostAnim", "ringGhostAnimations[5]",
        "rotate_order_to_hero", "find_metric_slot_in_order", "slot_position",
        "segment_visual_length", "finalize_anim_path_lengths", "chain_duration_ms", "segmentLength[4]", "totalLength", "durationMs", "maxPathLength", "lv_anim_set_duration(&a, durationMs)", "lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out)", "lv_anim_set_exec_cb(&a, anim_set_chain_progress)", "lv_anim_set_completed_cb(&a, finish_ring_animation)",
        "place_ghost_between_slots", "place_hero_between_slots", "place_between", "mini-card ghosts never enter the hero area", "heroObj", "fromSlot == 0", "toSlot == 0", "baseOpa", "chainSteps", "ringDirection", "ring_direction_to_hero", "ring_step_slot", "ring_steps_to_hero",
        "tile(ringAnimationOverlay, start.x, start.y, metric_for(kind, overlayView, true), false)",
        "lv_obj_add_flag(ringAnimationOverlay, LV_OBJ_FLAG_CLICKABLE)",
    ]
    for needle in required_ring_animation:
        if needle not in ledcards_interface_text:
            return fail(f"Ledcards Interface ring animation contract missing {needle}")
    if "PS_NUT_HOME_STATE ==" in ledcards_interface_text:
        return fail("Ledcards Interface firmware page must no longer be selected by compile-time PS_NUT_HOME_STATE")
    for needle in ["LedcardsInterfaceNutView makeLedcardsInterfaceNutView()", "createLedcardsInterfaceUi(makeLedcardsInterfaceNutView())", "updateLedcardsInterfaceUi(makeLedcardsInterfaceNutView())"]:
        if needle not in text:
            return fail(f"main firmware missing Ledcards Interface live-data hook {needle}")
    for needle in ["kLedcardsSleepLongPressMs = 3000", "kDisplayWakeCooldownMs = 1000", "displaySleepWakeArmed", "ledcardsSleepPressStartMs", "enterDisplaySleep()", "wakeDisplay()", "POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY"]:
        if needle not in text:
            return fail(f"main firmware missing Ledcards Interface long-press sleep hook {needle}")

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
        "#include \"ps_ui_tab_18.c\"",
        "lv_obj_set_style_text_font(bar_obj, &ps_ui_tab_18, 0)",
        "lv_obj_set_flex_flow(tab, LV_FLEX_FLOW_ROW)",
        "lv_obj_set_scroll_dir(tab, LV_DIR_HOR)",
        "lv_obj_set_scroll_snap_x(tab, LV_SCROLL_SNAP_CENTER)",
        "PS_ICON_HOME", "PS_ICON_NUT", "PS_ICON_SERVER", "PS_ICON_HOME_ASSISTANT", "PS_ICON_SETTINGS",
        "lv_obj_set_style_pad_gap(card, 6, 0)",
        "lv_obj_set_style_pad_all(card, 8, 0)",
        "bar(parent, value, color, 10)",
        "lv_obj_set_width(left, total && total[0] ? 132 : lv_pct(100))",
        "lv_obj_set_width(right, 68)",
        "POWER SENTINEL", "PROXMOX", "VM haos", "HOME ASSISTANT", "M5S", "NUT disarmed",
        "UPS", "Model APC Back-UPS BX", "BATTERY", "CHARGING", "Battery Charge", "POWER", "Power Usage", "PROTECTION", "Connected clients 0/2",
    ]
    for needle in required_dashboard_fixture:
        if needle not in dashboard_fixture:
            return fail(f"LVGL MCP dashboard fixture missing {needle!r}")
    if "lv_obj_t *box = lv_obj_create(parent);" in dashboard_fixture:
        return fail("LVGL MCP dashboard fixture PVE metrics drifted from firmware: wrapper box reintroduced")

    if not SPIKE_NUT_LEDCARDS_FIXTURE.exists():
        return fail("LVGL MCP NUT Ledcards visual fixture is missing")
    nut_ledcards_fixture = SPIKE_NUT_LEDCARDS_FIXTURE.read_text(encoding="utf-8")
    fixture_value_marker = "label(t, value, 20, 8, 58, &ps_font_ddin_condensed_bold_40, 0xf5f6f2)"
    fixture_name_marker = "label(t, name, 76, 6, 55, &lv_font_montserrat_12, 0xc9d0c9)"
    fixture_unit_marker = "label(t, unit, 78, 23, 53, &lv_font_montserrat_12, 0x87918c)"
    fixture_value_idx = nut_ledcards_fixture.find(fixture_value_marker)
    fixture_name_idx = nut_ledcards_fixture.find(fixture_name_marker)
    fixture_unit_idx = nut_ledcards_fixture.find(fixture_unit_marker)
    if fixture_value_idx == -1 or fixture_name_idx == -1 or fixture_unit_idx == -1:
        return fail("LVGL MCP NUT Ledcards fixture missing mini-card value/unit foreground markers")
    if fixture_value_idx < fixture_unit_idx:
        return fail("LVGL MCP NUT Ledcards fixture mini-card value must be created after label/unit objects so it stays in foreground")
    if '"Runtime"' not in nut_ledcards_fixture or '"TTE"' in nut_ledcards_fixture:
        return fail("LVGL MCP NUT Ledcards fixture should label runtime as Runtime, not TTE")
    font_file = ROOT / "firmware" / "core-s3-display" / "src" / "ps_ui_tab_12.c"
    font_file_18 = ROOT / "firmware" / "core-s3-display" / "src" / "ps_ui_tab_18.c"
    if not font_file.exists() or not font_file_18.exists():
        return fail("generated sidebar Nerd Font subsets ps_ui_tab_12.c/ps_ui_tab_18.c are missing")
    font_text = font_file.read_text(encoding="utf-8")
    font_text_18 = font_file_18.read_text(encoding="utf-8")
    chart_icon_file = ROOT / "firmware" / "core-s3-display" / "src" / "ps_icon_chart_32.c"
    if not chart_icon_file.exists():
        return fail("generated chart button Nerd Font subset ps_icon_chart_32.c is missing")
    chart_icon_text = chart_icon_file.read_text(encoding="utf-8")
    for needle in ["const lv_font_t ps_icon_chart_32", "0xF154D"]:
        if needle not in chart_icon_text:
            return fail(f"chart button Nerd Font subset missing {needle}")
    for needle in ["const lv_font_t ps_ui_tab_12", "0xF013", "0xF015", "0xF233", "0xF06F8", "0xF07D0"]:
        if needle not in font_text:
            return fail(f"sidebar Nerd Font subset missing {needle}")
    for needle in ["const lv_font_t ps_ui_tab_18", "0xF013", "0xF015", "0xF233", "0xF06F8", "0xF07D0"]:
        if needle not in font_text_18:
            return fail(f"large sidebar Nerd Font subset missing {needle}")
    render_copy_text = batch_script + "\n" + (SPIKE_DIR / "render-via-doomtrain.sh").read_text(encoding="utf-8")
    if "ps_ui_tab_12.c" not in render_copy_text or "ps_ui_tab_18.c" not in render_copy_text:
        return fail("LVGL MCP render path must copy/include the generated sidebar fonts")

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

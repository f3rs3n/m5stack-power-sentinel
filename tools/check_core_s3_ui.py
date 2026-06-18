#!/usr/bin/env python3
"""Static guard for the clean Power Sentinel NUT Monitor baseline."""

from __future__ import annotations

import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "core-s3-display" / "src" / "main.cpp"
LEDCARDS_CPP = ROOT / "firmware" / "core-s3-display" / "src" / "ledcards-interface-page.cpp"
AMBIENT_PAGE_TRANSITION_H = ROOT / "firmware" / "core-s3-display" / "src" / "ambient-console-page-transition.h"
CORE_S3_TRANSPORT_DIAGNOSTICS_H = ROOT / "firmware" / "core-s3-display" / "src" / "core-s3-transport-diagnostics.h"
LEDCARDS_GRAPHICS_H = ROOT / "firmware" / "core-s3-display" / "src" / "ledcards-graphics.h"
LEDCARDS_H = ROOT / "firmware" / "core-s3-display" / "src" / "ledcards-interface-page.h"
NUT_PAGE_MODEL_H = ROOT / "firmware" / "core-s3-display" / "src" / "nut-ambient-page-model.h"
PROXMOX_PAGE_MODEL_H = ROOT / "firmware" / "core-s3-display" / "src" / "proxmox-ambient-page-model.h"
CONTEXT = ROOT / "CONTEXT.md"
NUT_AMBIENT_CONTRACT = ROOT / "docs" / "architecture" / "nut-ambient-console-contract.md"
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
    if not MAIN.exists() or not LEDCARDS_CPP.exists() or not AMBIENT_PAGE_TRANSITION_H.exists() or not CORE_S3_TRANSPORT_DIAGNOSTICS_H.exists() or not LEDCARDS_GRAPHICS_H.exists() or not LEDCARDS_H.exists() or not NUT_PAGE_MODEL_H.exists() or not PROXMOX_PAGE_MODEL_H.exists() or not CONTEXT.exists() or not NUT_AMBIENT_CONTRACT.exists():
        return fail("firmware NUT monitor sources are missing")
    main = MAIN.read_text(encoding="utf-8")
    ledcards = LEDCARDS_CPP.read_text(encoding="utf-8") + "\n" + AMBIENT_PAGE_TRANSITION_H.read_text(encoding="utf-8") + "\n" + CORE_S3_TRANSPORT_DIAGNOSTICS_H.read_text(encoding="utf-8") + "\n" + LEDCARDS_GRAPHICS_H.read_text(encoding="utf-8") + "\n" + LEDCARDS_H.read_text(encoding="utf-8") + "\n" + NUT_PAGE_MODEL_H.read_text(encoding="utf-8") + "\n" + PROXMOX_PAGE_MODEL_H.read_text(encoding="utf-8")
    if require(main, [
        "nut-monitor-clean-baseline",
        "createLedcardsInterfaceUi(makeLedcardsInterfaceNutView())",
        "updateLedcardsInterfaceUi(view)",
        "currentLinkOk != lastRenderedLinkOk",
        "POWER_SENTINEL_TRANSPORT_SERIAL",
        "POWER_SENTINEL_LEDCARDS_INTERFACE_ONLY",
        "applyLedcardsInterfaceVisualFixture()",
        "kLedcardsInterfaceVisualFixtureJson",
        "Serial2.begin(POWER_SENTINEL_UART_BAUD, SERIAL_8N1, POWER_SENTINEL_UART_RX_PIN, POWER_SENTINEL_UART_TX_PIN)",
        "Transport: StackFlow serial rx=",
        "work_id\"] = \"sentinel\"",
        "parseSummary(body, \"stackflow\")",
        "kLinkStatusTimeoutMs = 10000",
        "doc[\"module\"].as<JsonObjectConst>()",
        "doc[\"modules\"][\"proxmox\"].as<JsonObjectConst>()",
        "makeProxmoxAmbientView()",
        "currentPageIndex",
        "proxmoxPageAvailable() ? 2 : 1",
        "renderProxmoxAmbientUi(makeProxmoxAmbientView(), view)",
        "handleTopBarPageTap",
         "transitionLedcardsInterfacePageUi(previousPageIndex, currentPageIndex, view, makeProxmoxAmbientView())",
        "WiFi.status() == WL_CONNECTED",
        "initStatusWiFi()",
        "psBatteryLevel()",
        "POWER_SENTINEL_DISPLAY_STANDBY_MS",
        "POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS",
        "POWER_SENTINEL_DISPLAY_SNOOZE_MS",
        "POWER_SENTINEL_MOTION_WAKE",
        "POWER_SENTINEL_DISPLAY_STANDBY_MS < 60000UL",
        "POWER_SENTINEL_DISPLAY_NO_PAYLOAD_OFF_MS < 60000UL",
        "POWER_SENTINEL_DISPLAY_DIM_BRIGHTNESS == 0",
        "Display policy: standby=",
        "DisplayMode::Dim",
        "manualDisplaySnoozeActive",
        "buildStateSignature",
        "state.nut.clientCount",
        "CoreS3TransportDiagnostics",
        "coreS3TransportRecordFailure",
        "coreS3TransportRecordSuccess",
        "coreS3TransportShouldLogSuccess",
        "Transport diagnostic:",
        "CORE_S3_TRANSPORT_FAILURE_JSON_PARSE",
        "CORE_S3_TRANSPORT_FAILURE_STACKFLOW_ERROR",
        "CORE_S3_TRANSPORT_FAILURE_TIMEOUT",
        "if (displayMode != DisplayMode::Off) refreshLedcardsUi();",
        "lastPayloadAgeMs >= kDisplayNoPayloadOffMs",
        "missingPayloadDisplayOffActive",
        "wasMissingPayloadOff",
        "Automatic standby stops at DIM",
        "kLedcardsSleepLongPressMs = 3000",
        "psDisplaySetBrightness(0)",
    ], "main NUT monitor"):
        return 1
    config_example = (ROOT / "firmware" / "core-s3-display" / "include" / "power_sentinel_config.example.h").read_text(encoding="utf-8")
    if require(config_example, ["#define SUMMARY_POLL_MS 5000UL", "POWER_SENTINEL_WIFI_SSID"], "CoreS3 config example"):
        return 1
    context = CONTEXT.read_text(encoding="utf-8")
    if require(context, [
        "**Ambient Console Shell**:",
        "**Module Page**:",
        "**Page model**:",
        "page registry",
        "top bar",
        "transport status",
        "display mode",
        "rendering policy",
        "Hero Position",
    ], "Power Sentinel vocabulary"):
        return 1
    forbidden = [
        "renderHome(", "renderProxmox(", "renderHa(", "renderM5s(", "tabview", "PS_ICON_HOME", "Zigbee2MqttState",
        "WorkloadMetric", "HOME", "M5S", "Running workloads", "HA OK", "PVE OK",
        "POWER_SENTINEL_HTTP_FALLBACK", "#include <HTTPClient.h>", "fetchHttpSummary", "HTTPClient http",
        "POWER_SENTINEL_SUMMARY_URL", "parseSummary(body, \"http\")",
    ]
    for needle in forbidden:
        if needle in main:
            return fail(f"clean NUT firmware still contains legacy multi-page marker {needle!r}")
    if require(ledcards, [
        "struct LedcardsInterfaceNutView",
        "struct NutAmbientPageModel",
        "telemetryState",
        "hasMissingMetrics",
        "shutdownRelevant",
        "makeNutAmbientPageModel(view)",
        "ps_icon_status_14",
        "moduleLanConnected", "wifiConnected", "linkOk", "moduleTimeHhmm", "localBatteryPercent",
        "nf-md-lan_connect", "nf-md-lan_disconnect",
        "nf-md-battery_charging", "nf-md-battery_alert", "nf-md-battery_low",
        "createLedcardsInterfaceUi(const LedcardsInterfaceNutView &view)",
        "updateLedcardsInterfaceUi(const LedcardsInterfaceNutView &view)",
        "METRIC_BATTERY", "METRIC_TTE", "METRIC_LOAD", "METRIC_INPUT", "METRIC_NUT",
        "compactValue", "visualClass", "nutAmbientFormatRuntimeFull", "nutAmbientFormatRuntimeMinutes",
        "CRITICAL RUNTIME", "LOW BATTERY", "UNAVAILABLE",
        "ledcardsVisualClassColor", "card_for_metric", "hero_state_for_card",
        "NutAmbientHeroPolicyInput", "acceptNutAmbientHeroMetric", "makeNutAmbientTouchHeroOverride",
        "start_ring_transition", "on_tile_clicked",
    ], "Ledcards Interface"):
        return 1
    if require(ledcards, [
        "struct LedcardsAmbientCardRender",
        "char label[12];",
        "char unit[8];",
        "char stateText[20];",
        "char transportStatus[32];",
        "ledcardsAmbientCopy(m.label, sizeof(m.label), card.label);",
        "ledcardsAmbientCopy(m.unit, sizeof(m.unit), compactTte ? card.compactUnit : card.unit);",
        "ledcardsAmbientCopy(m.stateText, sizeof(m.stateText), card.stateText);",
        "ledcardsAmbientCopy(metric.label, sizeof(metric.label), card.label);",
        "struct LedcardsRingSlotPosition",
        "ledcardsRingSlotPosition",
        "kLedcardsRingSlotCount",
        "struct AmbientConsolePageTransitionState",
        "ambientConsoleStartPageTransition",
        "AMBIENT_PAGE_TRANSITION_FORWARD",
        "AMBIENT_PAGE_TRANSITION_REVERSE",
        "transitionLedcardsInterfacePageUi",
        "Page body below it slides as one coherent module surface",
        "coreS3TransportFormatStatus",
    ], "Ledcards metric text ownership"):
        return 1
    if "const char *label;" in ledcards or "const char *unit;" in ledcards or "const char *stateText;" in ledcards:
        return fail("Ledcards MetricRender must not keep pointers into temporary page-model cards")
    if "  }\n  if (view.lowBattery || view.batteryPercent < 20) return \"LOW BATTERY\";" in ledcards:
        return fail("online battery display must not let NUT LB override percent buckets")
    if require(ledcards, [
        "struct ProxmoxAmbientPageModel",
        "makeProxmoxAmbientPageModel",
        "renderProxmoxAmbientUi",
        "renderProxmoxAmbientUnavailableUi",
        "render_proxmox_ambient",
        "proxmox_metric_for_card",
        "proxmox_hero_metric",
        "heroCardIndex",
        "heroDisplayValue",
        "proxmoxReducedCardCount",
        "proxmoxReducedCardAt",
        "proxmoxAmbientShouldAnimateHeroTransition",
        "proxmoxAmbientRotateSlotOrderToHero",
        "ensure_proxmox_slot_order",
        "makeProxmoxAmbientTouchHeroOverride",
        "acceptProxmoxAmbientHeroCard",
        "on_proxmox_tile_clicked",
        "add_tile_hit(cardTile, on_proxmox_tile_clicked",
        "start_proxmox_ring_transition",
        "hasLastRenderedProxmoxModel",
        "CPU",
        "cpuPercent",
        "cpuCondition",
        "CPU WARN",
        "CPU CRIT",
        "RAM",
        "ramPercent",
        "ramCondition",
        "RAM WARN",
        "RAM CRIT",
        "Guests",
        "Storage",
        "storagePercent",
        "storageCondition",
        "STOR WARN",
        "STOR CRIT",
        "Network",
        "PLACEHOLDER",
    ], "Proxmox five-card Ambient Page skeleton"):
        return 1
    proxmox_page_model = PROXMOX_PAGE_MODEL_H.read_text(encoding="utf-8")
    forbidden_proxmox_page_model = ["\"API\"", "\"Nodes\"", "\"Signal\"", "watched", "nodeCount"]
    for needle in forbidden_proxmox_page_model:
        if needle in proxmox_page_model:
            return fail(f"Proxmox page model still requires legacy page vocabulary {needle!r}")
    contract = NUT_AMBIENT_CONTRACT.read_text(encoding="utf-8")
    if require(contract, [
        "first stable Power Sentinel Ambient Console adapter shape",
        "modules.nut", "Top-level `ups`", "Top-level `nut`",
        "stackflow_safe", "module.lan_connected", "module.time_hhmm",
        "NutAmbientPageModel", "acceptNutAmbientHeroMetric", "makeNutAmbientTouchHeroOverride",
        "Proxmox page rendering", "Overview page rendering", "Telemetry History", "Controls or remediation flows",
    ], "NUT Ambient Console contract"):
        return 1
    forbidden_ledcards = [
        "view.offline ? \"STALE\" : \"LIVE\"",
        "top_status(screen, view, hero.accent)",
        "if (view.batteryPercent >= 0) bw =",
        "box(screen, 148, 8, 5, 5",
    ]
    for needle in forbidden_ledcards:
        if needle in ledcards:
            return fail(f"Ledcards top bar still contains obsolete marker {needle!r}")
    if not NUT_FIXTURE.exists():
        return fail("NUT Ledcards LVGL MCP fixture is missing")
    fixture = NUT_FIXTURE.read_text(encoding="utf-8")
    if require(fixture, ["NUT", "Runtime", "Battery", "Input", "Load", "STATE_LOW_BATTERY", "ps_icon_status_14"], "NUT fixture"):
        return 1
    print("PASS core-s3-ui clean NUT monitor guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

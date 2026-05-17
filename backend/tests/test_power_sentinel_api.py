import importlib.util
import json
import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "bin" / "power-sentinel-api.py"


def load_module():
    spec = importlib.util.spec_from_file_location("power_sentinel_api", MODULE_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_build_summary_has_stable_v1_contract():
    api = load_module()

    summary = api.build_summary(
        now=1_770_000_000,
        health={
            "overall_ok": True,
            "system": {"temperature_c": 45.4, "ram_available_mb": 500, "root_disk_free_gb": 20.5},
            "apis": {"stackflow": True, "openai": True},
            "chat_smoke": {"ok": True},
        },
        ups=None,
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
    )

    assert summary["schema"] == "power-sentinel.summary.v1"
    assert summary["timestamp"] == "2026-02-02T02:40:00Z"
    assert summary["severity"] == "warn"
    assert summary["ups"]["available"] is False
    assert summary["ups"]["status"] == "MOCK"
    assert summary["ups"]["stale"] is True
    assert summary["homeassistant"] == {"available": True, "severity": "ok", "mqtt": True}
    assert summary["proxmox"] == {"available": True, "severity": "ok", "shutdown_state": "disarmed"}
    assert summary["m5stack"]["temperature_c"] == 45.4
    assert summary["m5stack"]["stackflow_ok"] is True
    assert summary["m5stack"]["openai_ok"] is True
    assert summary["m5stack"]["chat_smoke_ok"] is True
    assert "UPS data is mock/unavailable" in summary["problems"]


def test_parse_upsc_derives_battery_status_and_numeric_values():
    api = load_module()

    ups = api.parse_upsc_output(
        """
ups.status: OB LB DISCHRG
battery.charge: 42
battery.runtime: 900
ups.load: 31
input.voltage: 228.5
output.voltage: 230.0
device.mfr: TestCorp
device.model: TestUPS 900
""".strip()
    )

    assert ups["available"] is True
    assert ups["status"] == "OB LB DISCHRG"
    assert ups["status_label"] == "Low battery"
    assert ups["on_battery"] is True
    assert ups["low_battery"] is True
    assert ups["battery_percent"] == 42
    assert ups["runtime_seconds"] == 900
    assert ups["load_percent"] == 31
    assert ups["input_voltage"] == 228.5
    assert ups["output_voltage"] == 230.0
    assert ups["device"]["manufacturer"] == "TestCorp"


def test_http_json_response_is_valid_for_summary_endpoint():
    api = load_module()
    body, status, content_type = api.route_request("/api/v1/summary")

    assert status == 200
    assert content_type == "application/json"
    payload = json.loads(body.decode("utf-8"))
    assert payload["schema"] == "power-sentinel.summary.v1"
    assert "ups" in payload
    assert "m5stack" in payload


def test_health_endpoint_reports_service_name_and_ok_boolean():
    api = load_module()
    body, status, _ = api.route_request("/api/v1/health")

    assert status == 200
    payload = json.loads(body.decode("utf-8"))
    assert payload["schema"] == "power-sentinel.health.v1"
    assert payload["service"] == "power-sentinel-api"
    assert isinstance(payload["ok"], bool)

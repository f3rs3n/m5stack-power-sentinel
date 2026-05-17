import importlib.util
import json
import pathlib
import sys


ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "bin" / "power-sentinel-stackflow-unit.py"


def load_module():
    spec = importlib.util.spec_from_file_location("power_sentinel_stackflow_unit", MODULE_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def param_frame(param0: str, param1: str) -> bytes:
    raw0 = param0.encode("utf-8")
    assert len(raw0) < 256
    return bytes([len(raw0)]) + raw0 + param1.encode("utf-8")


def test_parse_stackflow_param_frame():
    unit = load_module()
    callback = "ipc:///tmp/llm/8000.sock"
    body = '{"request_id":"r1","work_id":"sentinel","action":"summary"}'

    assert unit.parse_stackflow_param_frame(param_frame(callback, body)) == (callback, body)


def test_parse_request_extracts_action_and_ids():
    unit = load_module()
    callback = "ipc:///tmp/llm/8000.sock"
    body = '{"request_id":"r1","work_id":"sentinel","action":"summary","object":"None","data":"None"}'

    req = unit.parse_request(b"summary", param_frame(callback, body))

    assert req.callback_url == callback
    assert req.action == "summary"
    assert req.request_id == "r1"
    assert req.work_id == "sentinel"


def test_stackflow_response_shape():
    unit = load_module()
    req = unit.StackFlowRequest(
        callback_url="ipc:///tmp/llm/8000.sock",
        raw_json="{}",
        action="ping",
        request_id="r2",
        work_id="sentinel",
        object_name="None",
        data="None",
    )

    raw = unit.stackflow_response(req, object_name="power-sentinel.pong.v1", data={"ok": True})
    payload = json.loads(raw)

    assert payload["request_id"] == "r2"
    assert payload["work_id"] == "sentinel"
    assert payload["object"] == "power-sentinel.pong.v1"
    assert payload["data"] == {"ok": True}
    assert payload["error"] == {"code": 0, "message": ""}
    assert isinstance(payload["created"], int)


def test_summary_action_wraps_api_payload(monkeypatch):
    unit = load_module()
    req = unit.StackFlowRequest(
        callback_url="ipc:///tmp/llm/8000.sock",
        raw_json="{}",
        action="summary",
        request_id="r3",
        work_id="sentinel",
        object_name="None",
        data="None",
    )
    monkeypatch.setattr(unit, "fetch_summary", lambda api_url, timeout: {"schema": "power-sentinel.summary.v1", "severity": "ok"})

    payload = json.loads(unit.handle_request(req, "http://127.0.0.1:8088/api/v1/summary"))

    assert payload["object"] == "power-sentinel.summary.v1"
    assert payload["data"]["schema"] == "power-sentinel.summary.v1"
    assert payload["error"]["code"] == 0


def test_summary_action_reports_fetch_error(monkeypatch):
    unit = load_module()
    req = unit.StackFlowRequest(
        callback_url="ipc:///tmp/llm/8000.sock",
        raw_json="{}",
        action="summary",
        request_id="r4",
        work_id="sentinel",
        object_name="None",
        data="None",
    )

    def broken(api_url, timeout):
        raise RuntimeError("api down")

    monkeypatch.setattr(unit, "fetch_summary", broken)

    payload = json.loads(unit.handle_request(req, "http://127.0.0.1:8088/api/v1/summary"))

    assert payload["object"] == "power-sentinel.error.v1"
    assert payload["error"]["code"] == -9
    assert payload["data"]["ok"] is False
    assert "api down" in payload["data"]["message"]


def test_unknown_action_returns_stackflow_error():
    unit = load_module()
    req = unit.StackFlowRequest(
        callback_url="ipc:///tmp/llm/8000.sock",
        raw_json="{}",
        action="dance",
        request_id="r5",
        work_id="sentinel",
        object_name="None",
        data="None",
    )

    payload = json.loads(unit.handle_request(req, "unused"))

    assert payload["error"]["code"] == -7
    assert payload["data"]["error"] == "unknown_action"

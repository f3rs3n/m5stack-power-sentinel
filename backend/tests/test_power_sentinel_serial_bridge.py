import importlib.util
import json
import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "bin" / "power-sentinel-serial-bridge.py"


def load_module():
    spec = importlib.util.spec_from_file_location("power_sentinel_serial_bridge", MODULE_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_ping_pong_preserves_token():
    bridge = load_module()

    reply = bridge.handle_request("PS1 PING 12345", lambda: b"{}")

    assert reply is not None
    assert reply.body == b"PS1 PONG 12345\n"
    assert reply.log_label == "PONG"


def test_get_summary_is_length_prefixed_compact_json():
    bridge = load_module()
    payload = {"schema": "power-sentinel.summary.v1", "severity": "ok"}
    raw = bridge.compact_json(payload)

    reply = bridge.handle_request("PS1 GET summary", lambda: raw)

    assert reply is not None
    header, body, trailing = reply.body.split(b"\n")
    assert header == f"PS1 OK {len(raw)}".encode("ascii")
    assert json.loads(body.decode("utf-8")) == payload
    assert trailing == b""


def test_get_summary_reports_fetch_errors_without_crashing():
    bridge = load_module()

    def broken_fetcher():
        raise RuntimeError("boom")

    reply = bridge.handle_request("PS1 GET summary", broken_fetcher)

    assert reply is not None
    header, body, _ = reply.body.split(b"\n")
    assert header.startswith(b"PS1 ERR summary_exception ")
    payload = json.loads(body.decode("utf-8"))
    assert payload["schema"] == "power-sentinel.serial-error.v1"
    assert payload["ok"] is False
    assert payload["error"] == "summary_exception"
    assert "boom" in payload["message"]


def test_unknown_command_gets_small_error():
    bridge = load_module()

    reply = bridge.handle_request("HELLO", lambda: b"{}")

    assert reply is not None
    assert reply.body == b"PS1 ERR unknown_command 0\n"

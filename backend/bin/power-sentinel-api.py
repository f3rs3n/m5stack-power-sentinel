#!/usr/bin/env python3
"""Power Sentinel local JSON API for M5Stack LLM Module.

V0 intentionally works before the UPS USB OTG cable is attached: UPS data is
mock/unavailable, while M5Stack health and simple TCP reachability checks are
filled from local probes when available.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import socket
import subprocess
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import urlparse

SCHEMA_SUMMARY = "power-sentinel.summary.v1"
SCHEMA_HEALTH = "power-sentinel.health.v1"
DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 8088
HEALTHCHECK = os.environ.get("POWER_SENTINEL_HEALTHCHECK", "/usr/local/bin/m5stack-healthcheck")
UPSC = os.environ.get("POWER_SENTINEL_UPSC", "/usr/bin/upsc")
UPS_NAME = os.environ.get("POWER_SENTINEL_UPS", "homelab_ups@localhost")
HA_HOST = os.environ.get("POWER_SENTINEL_HA_HOST", "192.168.2.200")
MQTT_HOST = os.environ.get("POWER_SENTINEL_MQTT_HOST", HA_HOST)
PROXMOX_HOST = os.environ.get("POWER_SENTINEL_PROXMOX_HOST", "192.168.2.1")


def iso_utc(ts: float | int | None = None) -> str:
    if ts is None:
        ts = time.time()
    return dt.datetime.fromtimestamp(float(ts), tz=dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def tcp_check(host: str, port: int, timeout: float = 0.75) -> bool:
    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def run_json_command(cmd: list[str], timeout: float = 8.0) -> dict[str, Any] | None:
    try:
        cp = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout, check=False)
    except (OSError, subprocess.SubprocessError):
        return None
    if not cp.stdout.strip():
        return None
    try:
        return json.loads(cp.stdout)
    except json.JSONDecodeError:
        return None


def load_m5stack_health() -> dict[str, Any] | None:
    if not os.path.exists(HEALTHCHECK):
        return None
    return run_json_command([HEALTHCHECK, "--json"])


def as_int(value: str | None) -> int | None:
    if value is None or value == "":
        return None
    try:
        return int(float(value))
    except ValueError:
        return None


def as_float(value: str | None) -> float | None:
    if value is None or value == "":
        return None
    try:
        return float(value)
    except ValueError:
        return None


def parse_key_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in text.splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        values[key.strip()] = value.strip()
    return values


def ups_status_label(raw: str) -> str:
    tokens = set(raw.split())
    if "LB" in tokens:
        return "Low battery"
    if "OB" in tokens:
        return "On battery"
    if "OL" in tokens:
        return "Online"
    if raw:
        return raw
    return "Unknown"


def parse_upsc_output(text: str) -> dict[str, Any]:
    raw = parse_key_values(text)
    status = raw.get("ups.status", "UNKNOWN")
    tokens = set(status.split())
    return {
        "available": True,
        "status": status,
        "status_label": ups_status_label(status),
        "on_battery": "OB" in tokens,
        "low_battery": "LB" in tokens,
        "battery_percent": as_int(raw.get("battery.charge")),
        "runtime_seconds": as_int(raw.get("battery.runtime")),
        "load_percent": as_int(raw.get("ups.load")),
        "input_voltage": as_float(raw.get("input.voltage")),
        "output_voltage": as_float(raw.get("output.voltage")),
        "stale": False,
        "age_seconds": 0,
        "device": {
            "manufacturer": raw.get("device.mfr"),
            "model": raw.get("device.model"),
            "serial": raw.get("device.serial"),
        },
        "raw": raw,
    }


def load_ups() -> dict[str, Any] | None:
    if not os.path.exists(UPSC):
        return None
    try:
        cp = subprocess.run([UPSC, UPS_NAME], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=3, check=False)
    except (OSError, subprocess.SubprocessError):
        return None
    if cp.returncode != 0 or not cp.stdout.strip():
        return None
    return parse_upsc_output(cp.stdout)


def mock_ups() -> dict[str, Any]:
    return {
        "available": False,
        "status": "MOCK",
        "status_label": "UPS unavailable/mock",
        "on_battery": False,
        "low_battery": False,
        "battery_percent": None,
        "runtime_seconds": None,
        "load_percent": None,
        "input_voltage": None,
        "output_voltage": None,
        "stale": True,
        "age_seconds": None,
        "device": {"manufacturer": None, "model": None, "serial": None},
        "raw": {},
    }


def health_value(health: dict[str, Any] | None, *path: str, default: Any = None) -> Any:
    cur: Any = health or {}
    for key in path:
        if not isinstance(cur, dict) or key not in cur:
            return default
        cur = cur[key]
    return cur


def first_health_value(health: dict[str, Any] | None, paths: list[tuple[str, ...]], default: Any = None) -> Any:
    for path in paths:
        value = health_value(health, *path, default=None)
        if value is not None:
            return value
    return default


def derive_severity(ups: dict[str, Any], m5_ok: bool, checks: dict[str, bool], problems: list[str]) -> str:
    if ups.get("low_battery"):
        return "critical"
    if not m5_ok:
        return "critical"
    if problems:
        return "warn"
    if ups.get("on_battery"):
        return "warn"
    if not checks.get("homeassistant", False) or not checks.get("proxmox", False):
        return "warn"
    return "ok"


def build_summary(
    now: float | int | None = None,
    health: dict[str, Any] | None = None,
    ups: dict[str, Any] | None = None,
    checks: dict[str, bool] | None = None,
) -> dict[str, Any]:
    if now is None:
        now = time.time()
    if checks is None:
        checks = {
            "homeassistant": tcp_check(HA_HOST, 8123),
            "mqtt": tcp_check(MQTT_HOST, 1883),
            "proxmox": tcp_check(PROXMOX_HOST, 8006),
        }
    if health is None:
        health = load_m5stack_health()
    if ups is None:
        ups = load_ups() or mock_ups()

    m5_overall = bool(health_value(health, "overall_ok", default=False))
    problems: list[str] = []
    if not ups.get("available"):
        problems.append("UPS data is mock/unavailable")
    if ups.get("low_battery"):
        problems.append("UPS low battery")
    elif ups.get("on_battery"):
        problems.append("UPS on battery")
    if health is None:
        problems.append("M5Stack healthcheck unavailable")
    elif not m5_overall:
        problems.append("M5Stack healthcheck reports failure")
    if not checks.get("homeassistant", False):
        problems.append("Home Assistant API unreachable")
    if not checks.get("mqtt", False):
        problems.append("MQTT broker unreachable")
    if not checks.get("proxmox", False):
        problems.append("Proxmox API unreachable")

    severity = derive_severity(ups, m5_overall, checks, problems)
    return {
        "schema": SCHEMA_SUMMARY,
        "timestamp": iso_utc(now),
        "severity": severity,
        "ups": {
            "available": bool(ups.get("available")),
            "status": ups.get("status", "UNKNOWN"),
            "status_label": ups.get("status_label", "Unknown"),
            "on_battery": bool(ups.get("on_battery")),
            "low_battery": bool(ups.get("low_battery")),
            "battery_percent": ups.get("battery_percent"),
            "runtime_seconds": ups.get("runtime_seconds"),
            "load_percent": ups.get("load_percent"),
            "input_voltage": ups.get("input_voltage"),
            "output_voltage": ups.get("output_voltage"),
            "stale": bool(ups.get("stale", False)),
            "age_seconds": ups.get("age_seconds"),
        },
        "homeassistant": {"available": bool(checks.get("homeassistant")), "severity": "ok" if checks.get("homeassistant") else "warn", "mqtt": bool(checks.get("mqtt"))},
        "proxmox": {"available": bool(checks.get("proxmox")), "severity": "ok" if checks.get("proxmox") else "warn", "shutdown_state": "disarmed"},
        "m5stack": {
            "available": health is not None,
            "severity": "ok" if m5_overall else "critical",
            "temperature_c": health_value(health, "system", "temperature_c"),
            "ram_available_mb": first_health_value(health, [("system", "linux_mem", "available_mb"), ("system", "ram_available_mb")]),
            "disk_free_gb": first_health_value(health, [("system", "root_disk", "free_gb"), ("system", "root_disk_free_gb")]),
            "stackflow_ok": bool(first_health_value(health, [("ports", "stackflow_10001", "ok"), ("stackflow", "lsmode_ok"), ("apis", "stackflow")], default=False)),
            "openai_ok": bool(first_health_value(health, [("ports", "openai_8000", "ok"), ("openai", "models", "ok"), ("apis", "openai")], default=False)),
            "chat_smoke_ok": bool(health_value(health, "chat_smoke", "ok", default=False)),
        },
        "problems": problems,
    }


def build_health(now: float | int | None = None) -> dict[str, Any]:
    summary = build_summary(now=now)
    ok = summary["severity"] in ("ok", "warn")
    return {
        "schema": SCHEMA_HEALTH,
        "service": "power-sentinel-api",
        "timestamp": summary["timestamp"],
        "ok": bool(ok),
        "summary_severity": summary["severity"],
        "problem_count": len(summary["problems"]),
        "problems": summary["problems"],
    }


def json_response(payload: dict[str, Any], status: int = 200) -> tuple[bytes, int, str]:
    return json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8") + b"\n", status, "application/json"


def route_request(path: str) -> tuple[bytes, int, str]:
    parsed = urlparse(path)
    if parsed.path in ("/", "/api/v1/summary"):
        return json_response(build_summary())
    if parsed.path == "/api/v1/health":
        return json_response(build_health())
    if parsed.path == "/api/v1/ups":
        return json_response({"schema": "power-sentinel.ups.v1", "timestamp": iso_utc(), "ups": load_ups() or mock_ups()})
    return json_response({"error": "not_found", "path": parsed.path}, status=404)


class Handler(BaseHTTPRequestHandler):
    server_version = "PowerSentinelAPI/0.1"

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        body, status, content_type = route_request(self.path)
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format: str, *args: Any) -> None:
        sys.stderr.write("%s - - [%s] %s\n" % (self.address_string(), self.log_date_time_string(), format % args))


def serve(host: str, port: int) -> None:
    httpd = ThreadingHTTPServer((host, port), Handler)
    print(f"power-sentinel-api listening on http://{host}:{port}", flush=True)
    httpd.serve_forever()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Power Sentinel local JSON API")
    parser.add_argument("--host", default=os.environ.get("POWER_SENTINEL_BIND", DEFAULT_HOST))
    parser.add_argument("--port", type=int, default=int(os.environ.get("POWER_SENTINEL_PORT", DEFAULT_PORT)))
    parser.add_argument("--once", choices=["summary", "health", "ups"], help="print one JSON payload and exit")
    args = parser.parse_args(argv)
    if args.once == "summary":
        print(json.dumps(build_summary(), indent=2, sort_keys=True))
        return 0
    if args.once == "health":
        print(json.dumps(build_health(), indent=2, sort_keys=True))
        return 0
    if args.once == "ups":
        body, _, _ = route_request("/api/v1/ups")
        print(body.decode().strip())
        return 0
    serve(args.host, args.port)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

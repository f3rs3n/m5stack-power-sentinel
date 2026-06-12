#!/usr/bin/env python3
"""Power Sentinel API, reframed around modular dashboard pages.

Current clean baseline: NUT Monitor is the only implemented/enabled runtime
module. Proxmox and Home Assistant are declared as future page modules so the
installer and contract already have stable names, but they emit no telemetry
until their module backends are reintroduced.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import subprocess
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Callable, NamedTuple
from urllib.parse import parse_qs, urlparse

SCHEMA_SUMMARY = "power-sentinel.summary.v1"
SCHEMA_HEALTH = "power-sentinel.health.v1"
DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 8088
POWER_SENTINEL_CONFIG = os.environ.get("POWER_SENTINEL_CONFIG", "/etc/power-sentinel.json")
UPSC = os.environ.get("POWER_SENTINEL_UPSC", "/usr/bin/upsc")
UPS_NAME = os.environ.get("POWER_SENTINEL_UPS", "homelab_ups@localhost")
NUT_CLIENTS_FILE = os.environ.get("POWER_SENTINEL_NUT_CLIENTS_FILE", "/etc/power-sentinel-nut-clients.json")


class ModuleSpec(NamedTuple):
    name: str
    page: str
    status: str
    summary_fn: Callable[[dict[str, Any]], dict[str, Any]] | None = None


def iso_utc(ts: float | int | None = None) -> str:
    if ts is None:
        ts = time.time()
    return dt.datetime.fromtimestamp(float(ts), tz=dt.timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def local_time_hhmm(ts: float | int | None = None) -> str:
    if ts is None:
        ts = time.time()
    return dt.datetime.fromtimestamp(float(ts)).strftime("%H:%M")


def module_lan_status() -> dict[str, Any]:
    code, out, _err = run_text_command(["ip", "-4", "-o", "addr", "show", "scope", "global"], timeout=1.0)
    if code != 0:
        return {"lan_connected": False, "lan_ip": None}
    fallback_ip: str | None = None
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 4:
            continue
        iface = parts[1].rstrip(":")
        try:
            inet_index = parts.index("inet")
        except ValueError:
            continue
        if inet_index + 1 >= len(parts):
            continue
        ip = parts[inet_index + 1].split("/", 1)[0]
        if not ip or ip.startswith("127."):
            continue
        if fallback_ip is None:
            fallback_ip = ip
        lowered = iface.lower()
        if not (lowered.startswith("docker") or lowered.startswith("br-") or lowered in {"lo"}):
            return {"lan_connected": True, "lan_ip": ip}
    return {"lan_connected": fallback_ip is not None, "lan_ip": fallback_ip}


def load_json_file(path: str) -> dict[str, Any]:
    try:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, json.JSONDecodeError):
        return {}
    return data if isinstance(data, dict) else {}


def enabled_modules(config: dict[str, Any]) -> set[str]:
    raw = os.environ.get("POWER_SENTINEL_MODULES")
    if raw:
        return {item.strip().lower() for item in raw.split(",") if item.strip()}
    modules = config.get("modules")
    if isinstance(modules, list):
        return {str(item).lower() for item in modules}
    if isinstance(modules, dict):
        return {str(name).lower() for name, enabled in modules.items() if enabled}
    return {"nut"}


def run_text_command(cmd: list[str], timeout: float = 3.0) -> tuple[int, str, str]:
    try:
        cp = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout, check=False)
    except (OSError, subprocess.SubprocessError) as exc:
        return 127, "", str(exc)
    return cp.returncode, cp.stdout, cp.stderr


def parse_key_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in text.splitlines():
        if ":" not in line:
            continue
        key, value = line.split(":", 1)
        values[key.strip()] = value.strip()
    return values


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


def ups_status_label(raw: str) -> str:
    tokens = set(raw.split())
    if "OB" in tokens and "LB" in tokens:
        return "Low battery"
    if "OB" in tokens:
        return "On battery"
    if "OL" in tokens:
        return "Online"
    return raw or "Unknown"


def parse_upsc_output(text: str) -> dict[str, Any]:
    raw = parse_key_values(text)
    status = raw.get("ups.status", "UNKNOWN")
    tokens = set(status.split())
    load_percent = as_int(raw.get("ups.load"))
    realpower_nominal_w = as_int(raw.get("ups.realpower.nominal"))
    load_w = None
    if load_percent is not None and realpower_nominal_w is not None:
        load_w = int(round(realpower_nominal_w * load_percent / 100.0))
    manufacturer = raw.get("ups.mfr") or raw.get("device.mfr")
    model = raw.get("ups.model") or raw.get("device.model")
    serial = raw.get("ups.serial") or raw.get("device.serial")
    on_battery = "OB" in tokens
    low_battery = "LB" in tokens
    return {
        "available": True,
        "stale": False,
        "name": UPS_NAME,
        "status": status,
        "status_label": ups_status_label(status),
        "on_battery": on_battery,
        "low_battery": low_battery,
        "charging": "CHRG" in tokens,
        "would_shutdown": on_battery and low_battery,
        "model": " ".join(part for part in [manufacturer, model] if part) or model or "unknown",
        "serial": serial,
        "battery_percent": as_int(raw.get("battery.charge")),
        "battery_voltage": as_float(raw.get("battery.voltage")),
        "runtime_seconds": as_int(raw.get("battery.runtime")),
        "load_percent": load_percent,
        "realpower_nominal_w": realpower_nominal_w,
        "load_w": load_w,
        "input_voltage": as_float(raw.get("input.voltage")),
        "output_voltage": as_float(raw.get("output.voltage")),
        "beeper_status": raw.get("ups.beeper.status") or "unknown",
        "transfer_reason": raw.get("input.transfer.reason") or raw.get("ups.test.result") or "unknown",
        "driver": raw.get("driver.name") or raw.get("driver.version.data") or "unknown",
        "raw_keys": sorted(raw.keys()),
        "age_seconds": 0,
    }


def unavailable_ups(reason: str) -> dict[str, Any]:
    return {
        "available": False,
        "stale": True,
        "name": UPS_NAME,
        "status": "UNAVAILABLE",
        "status_label": "Unavailable",
        "on_battery": False,
        "low_battery": False,
        "charging": False,
        "would_shutdown": False,
        "model": "unknown",
        "serial": None,
        "battery_percent": None,
        "battery_voltage": None,
        "runtime_seconds": None,
        "load_percent": None,
        "realpower_nominal_w": None,
        "load_w": None,
        "input_voltage": None,
        "output_voltage": None,
        "beeper_status": "unknown",
        "transfer_reason": "unknown",
        "driver": "unknown",
        "age_seconds": None,
        "problem": reason,
    }


def systemd_active(unit: str) -> bool | None:
    code, out, _err = run_text_command(["systemctl", "is-active", unit], timeout=1.5)
    text = out.strip()
    if code == 0 and text == "active":
        return True
    if text in {"inactive", "failed", "activating", "deactivating"}:
        return False
    return None


def load_nut_clients(path: str = NUT_CLIENTS_FILE) -> list[dict[str, Any]]:
    data = load_json_file(path)
    clients = data.get("clients")
    if not isinstance(clients, list):
        return []
    out = []
    for item in clients:
        if isinstance(item, dict):
            out.append({
                "name": str(item.get("name") or item.get("host") or "unknown"),
                "host": str(item.get("host") or ""),
                "role": str(item.get("role") or "secondary"),
                "enabled": bool(item.get("enabled", True)),
            })
    return out


def local_nut_client(services: dict[str, bool | None]) -> dict[str, Any]:
    return {
        "name": "power-sentinel",
        "host": "localhost",
        "role": "primary",
        "enabled": services.get("monitor_active") is not False,
    }


def build_nut_summary(_config: dict[str, Any]) -> dict[str, Any]:
    code, out, err = run_text_command([UPSC, UPS_NAME], timeout=3.5)
    ups = parse_upsc_output(out) if code == 0 and out.strip() else unavailable_ups(err.strip() or f"upsc exited {code}")
    services = {
        "driver_active": systemd_active("nut-driver.service"),
        "server_active": systemd_active("nut-server.service"),
        "monitor_active": systemd_active("nut-monitor.service"),
    }
    clients = [local_nut_client(services), *load_nut_clients()]
    server_ok = services["server_active"] is not False and services["driver_active"] is not False
    severity = "critical" if ups["would_shutdown"] else ("warn" if not ups["available"] or not server_ok or ups["on_battery"] or ups["low_battery"] else "ok")
    return {
        "enabled": True,
        "page": "NUT",
        "severity": severity,
        "ups": ups,
        "nut": {
            "mode": "netserver",
            **services,
            "client_count": len([c for c in clients if c.get("enabled")]),
            "clients": clients,
            "shutdown_owner": "upsmon",
            "would_shutdown": ups["would_shutdown"],
            "shutdown_rule": "OB LB only; OL LB is warning/no-shutdown",
        },
    }


def module_placeholder(spec: ModuleSpec, enabled: bool) -> dict[str, Any]:
    return {
        "enabled": enabled,
        "page": spec.page,
        "severity": "unknown" if enabled else "disabled",
        "status": spec.status,
        "implemented": False,
    }


MODULES: dict[str, ModuleSpec] = {
    "nut": ModuleSpec("nut", "NUT", "implemented", build_nut_summary),
    "proxmox": ModuleSpec("proxmox", "PROXMOX", "placeholder"),
    "ha": ModuleSpec("ha", "HA", "placeholder"),
}


def build_summary(config: dict[str, Any] | None = None, *, stackflow_safe: bool = False) -> dict[str, Any]:
    config = config if config is not None else load_json_file(POWER_SENTINEL_CONFIG)
    enabled = enabled_modules(config)
    modules: dict[str, Any] = {}
    problems: list[str] = []
    severities: list[str] = []
    for name, spec in MODULES.items():
        is_enabled = name in enabled
        if is_enabled and spec.summary_fn:
            modules[name] = spec.summary_fn(config)
            severities.append(str(modules[name].get("severity", "unknown")))
        else:
            modules[name] = module_placeholder(spec, is_enabled)
            if is_enabled:
                problems.append(f"module {name} is enabled but not implemented in this clean baseline")
                severities.append("warn")
    if "critical" in severities:
        severity = "critical"
    elif "warn" in severities:
        severity = "warn"
    elif severities:
        severity = "ok"
    else:
        severity = "unknown"
    nut = modules.get("nut", {})
    summary = {
        "schema": SCHEMA_SUMMARY,
        "timestamp": iso_utc(),
        "product": "Power Sentinel",
        "profile": "nut-monitor-clean-baseline",
        "severity": severity,
        "stackflow_safe": bool(stackflow_safe),
        "enabled_modules": sorted(enabled),
        "available_modules": list(MODULES.keys()),
        "pages": [MODULES[name].page for name in MODULES if name in enabled],
        "module": {**module_lan_status(), "time_hhmm": local_time_hhmm()},
        "modules": modules,
        # Compatibility aliases for the current CoreS3 NUT monitor firmware.
        "ups": nut.get("ups", unavailable_ups("NUT module disabled")),
        "nut": nut.get("nut", {"client_count": 0, "would_shutdown": False}),
        "problems": problems,
    }
    return summary


class Handler(BaseHTTPRequestHandler):
    server_version = "PowerSentinel/clean-nut"

    def log_message(self, format: str, *args: object) -> None:  # noqa: A002 - stdlib signature
        return

    def send_json(self, payload: dict[str, Any], status: int = 200) -> None:
        body = (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802 - BaseHTTPRequestHandler API
        parsed = urlparse(self.path)
        query = parse_qs(parsed.query)
        if parsed.path == "/api/v1/summary":
            self.send_json(build_summary(stackflow_safe=query.get("stackflow_safe", ["0"])[0] in {"1", "true", "yes"}))
            return
        if parsed.path == "/api/v1/health":
            summary = build_summary(stackflow_safe=True)
            self.send_json({"schema": SCHEMA_HEALTH, "ok": summary["severity"] != "critical", "severity": summary["severity"], "timestamp": iso_utc()})
            return
        self.send_json({"error": "not_found", "path": parsed.path}, status=404)


def serve(host: str, port: int) -> None:
    httpd = ThreadingHTTPServer((host, port), Handler)
    print(f"power-sentinel API clean NUT baseline listening on http://{host}:{port}", flush=True)
    httpd.serve_forever()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--summary", action="store_true", help="print one summary JSON and exit")
    args = parser.parse_args(argv)
    if args.summary:
        print(json.dumps(build_summary(), indent=2, sort_keys=True))
        return 0
    serve(args.host, args.port)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

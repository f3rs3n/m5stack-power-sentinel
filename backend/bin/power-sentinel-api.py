#!/usr/bin/env python3
"""Power Sentinel API, reframed around modular dashboard pages.

Current baseline: NUT Monitor is enabled by default, Proxmox has an initial
read-only API adapter, and Home Assistant remains a placeholder page module.
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
from urllib.request import Request, urlopen

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


class ModuleEvaluation(NamedTuple):
    payload: dict[str, Any]
    condition: str | None = None
    problem: str | None = None


class ModuleRuntimeResult(NamedTuple):
    modules: dict[str, Any]
    condition: str
    severity: str
    available_modules: list[str]
    pages: list[str]
    problems: list[str]
    ups: dict[str, Any]
    nut: dict[str, Any]


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


def severity_for_condition(condition: str) -> str:
    if condition == "healthy":
        return "ok"
    if condition == "critical":
        return "critical"
    if condition in {"stale", "warning", "unavailable"}:
        return "warn"
    return "unknown"


def summary_condition(conditions: list[str]) -> str:
    if "critical" in conditions:
        return "critical"
    if "warning" in conditions:
        return "warning"
    if "stale" in conditions:
        return "stale"
    if "unavailable" in conditions:
        return "unavailable"
    if "healthy" in conditions:
        return "healthy"
    return "unavailable"


def nut_condition(ups: dict[str, Any], services: dict[str, bool | None]) -> str:
    if ups["would_shutdown"]:
        return "critical"
    server_ok = services["server_active"] is not False and services["driver_active"] is not False
    if ups.get("stale"):
        return "stale" if ups.get("available") else "unavailable"
    if not server_ok or ups["on_battery"] or ups["low_battery"]:
        return "warning"
    return "healthy"


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
    condition = nut_condition(ups, services)
    severity = severity_for_condition(condition)
    return {
        "enabled": True,
        "page": "NUT",
        "condition": condition,
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


def proxmox_signal(kind: str, condition: str, summary: str, context: dict[str, Any] | None = None) -> dict[str, Any]:
    signal: dict[str, Any] = {"kind": kind, "condition": condition, "summary": summary}
    if context:
        signal["context"] = context
    return signal


def proxmox_condition_from_signals(signals: list[dict[str, Any]]) -> str:
    if not signals:
        return "healthy"
    return summary_condition([str(signal.get("condition", "unavailable")) for signal in signals])


def proxmox_base_payload(condition: str, status: str, signals: list[dict[str, Any]]) -> dict[str, Any]:
    return {
        "enabled": True,
        "implemented": True,
        "page": "PROXMOX",
        "condition": condition,
        "severity": severity_for_condition(condition),
        "status": status,
        "signals": signals,
    }


def proxmox_config(config: dict[str, Any]) -> dict[str, Any]:
    raw = config.get("proxmox")
    return raw if isinstance(raw, dict) else {}


def missing_proxmox_config_fields(config: dict[str, Any]) -> list[str]:
    return [field for field in ("api_url", "token_id", "token_secret") if not str(config.get(field) or "").strip()]


def proxmox_watched_guests(config: dict[str, Any]) -> list[int]:
    raw = config.get("watched_guests", [])
    if not isinstance(raw, list):
        return []
    guests: list[int] = []
    for item in raw:
        try:
            guests.append(int(item))
        except (TypeError, ValueError):
            continue
    return guests


def proxmox_api_get(config: dict[str, Any], path: str) -> dict[str, Any]:
    api_url = str(config["api_url"]).rstrip("/")
    token_id = str(config["token_id"])
    token_secret = str(config["token_secret"])
    url = f"{api_url}/api2/json{path}"
    request = Request(url, headers={"Authorization": f"PVEAPIToken={token_id}={token_secret}"})
    with urlopen(request, timeout=4.0) as response:  # noqa: S310 - configured local Proxmox URL
        payload = json.loads(response.read(512 * 1024).decode("utf-8"))
    if not isinstance(payload, dict):
        raise ValueError("Proxmox API returned non-object JSON")
    return payload


def proxmox_api_error_summary(exc: Exception) -> str:
    text = str(exc).splitlines()[0] if str(exc) else type(exc).__name__
    for marker in ("PVEAPIToken=", "token_secret", "SECRET"):
        if marker in text:
            return type(exc).__name__
    return text[:120]


def proxmox_percent(value: Any, maximum: Any | None = None) -> int | None:
    try:
        raw = float(value)
        if maximum is not None:
            max_value = float(maximum)
            if max_value > 0:
                raw = raw / max_value
        if raw <= 1.0:
            raw *= 100.0
        return int(round(raw))
    except (TypeError, ValueError):
        return None


def parse_proxmox_nodes(payload: dict[str, Any]) -> list[dict[str, Any]]:
    data = payload.get("data")
    if not isinstance(data, list):
        return []
    nodes: list[dict[str, Any]] = []
    for item in data:
        if not isinstance(item, dict):
            continue
        status = str(item.get("status") or "unknown")
        condition = "healthy" if status == "online" else "critical"
        node = {
            "name": str(item.get("node") or item.get("name") or "unknown"),
            "condition": condition,
            "status": status,
        }
        cpu = proxmox_percent(item.get("cpu"))
        mem = proxmox_percent(item.get("mem"), item.get("maxmem"))
        if cpu is not None:
            node["cpu_percent"] = cpu
        if mem is not None:
            node["memory_percent"] = mem
        nodes.append(node)
    return nodes


def proxmox_node_signal(node: dict[str, Any]) -> dict[str, Any] | None:
    if node.get("condition") != "critical":
        return None
    name = str(node.get("name") or "unknown")
    status = str(node.get("status") or "unknown")
    if status == "offline":
        return proxmox_signal("node_down", "critical", f"Proxmox node {name} is offline", {"node": name, "status": status})
    return proxmox_signal("node_degraded", "critical", f"Proxmox node {name} is {status}", {"node": name, "status": status})


def proxmox_node_signals(nodes: list[dict[str, Any]]) -> list[dict[str, Any]]:
    signals: list[dict[str, Any]] = []
    for node in nodes:
        signal = proxmox_node_signal(node)
        if signal is not None:
            signals.append(signal)
    return signals


def parse_proxmox_storage(node_name: str, payload: dict[str, Any]) -> list[dict[str, Any]]:
    data = payload.get("data")
    if not isinstance(data, list):
        return []
    storage_items: list[dict[str, Any]] = []
    for item in data:
        if not isinstance(item, dict):
            continue
        if item.get("enabled") in {0, "0", False} or item.get("active") in {0, "0", False}:
            continue
        used_percent = proxmox_percent(item.get("used"), item.get("total"))
        if used_percent is None:
            continue
        condition = "critical" if used_percent >= 95 else ("warning" if used_percent >= 85 else "healthy")
        storage_items.append({
            "node": node_name,
            "name": str(item.get("storage") or item.get("name") or "unknown"),
            "condition": condition,
            "type": str(item.get("type") or "unknown"),
            "used_percent": used_percent,
        })
    return storage_items


def collect_proxmox_storage(config: dict[str, Any], nodes: list[dict[str, Any]]) -> list[dict[str, Any]]:
    storage_items: list[dict[str, Any]] = []
    for node in nodes:
        if node.get("status") != "online":
            continue
        node_name = str(node.get("name") or "unknown")
        storage_items.extend(parse_proxmox_storage(node_name, proxmox_api_get(config, f"/nodes/{node_name}/storage")))
    return storage_items


def proxmox_storage_signals(storage_items: list[dict[str, Any]]) -> list[dict[str, Any]]:
    signals: list[dict[str, Any]] = []
    for item in storage_items:
        condition = str(item.get("condition") or "healthy")
        if condition not in {"warning", "critical"}:
            continue
        kind = "storage_critical" if condition == "critical" else "storage_warning"
        node = str(item.get("node") or "unknown")
        name = str(item.get("name") or "unknown")
        used_percent = item.get("used_percent")
        signals.append(proxmox_signal(
            kind,
            condition,
            f"Proxmox storage {node}/{name} is {used_percent}% used",
            {"node": node, "storage": name, "used_percent": used_percent},
        ))
    return signals


def parse_proxmox_qemu_guests(node_name: str, payload: dict[str, Any]) -> list[dict[str, Any]]:
    data = payload.get("data")
    if not isinstance(data, list):
        return []
    guests: list[dict[str, Any]] = []
    for item in data:
        if not isinstance(item, dict):
            continue
        vmid = as_int(str(item.get("vmid")))
        if vmid is None:
            continue
        status = str(item.get("status") or "unknown")
        guests.append({
            "vmid": vmid,
            "name": str(item.get("name")) if item.get("name") is not None else None,
            "node": node_name,
            "condition": "healthy" if status == "running" else "critical",
            "status": status,
        })
    return guests


def collect_proxmox_watched_guests(config: dict[str, Any], nodes: list[dict[str, Any]], watched_vmids: list[int]) -> list[dict[str, Any]]:
    if not watched_vmids:
        return []
    all_guests: dict[int, dict[str, Any]] = {}
    for node in nodes:
        if node.get("status") != "online":
            continue
        node_name = str(node.get("name") or "unknown")
        for guest in parse_proxmox_qemu_guests(node_name, proxmox_api_get(config, f"/nodes/{node_name}/qemu")):
            all_guests[int(guest["vmid"])] = guest
    watched_guests: list[dict[str, Any]] = []
    for vmid in watched_vmids:
        watched_guests.append(all_guests.get(vmid) or {
            "vmid": vmid,
            "name": None,
            "node": None,
            "condition": "critical",
            "status": "missing",
        })
    return watched_guests


def proxmox_watched_guest_signals(guests: list[dict[str, Any]]) -> list[dict[str, Any]]:
    signals: list[dict[str, Any]] = []
    for guest in guests:
        if guest.get("condition") != "critical":
            continue
        vmid = guest.get("vmid")
        status = str(guest.get("status") or "unknown")
        signals.append(proxmox_signal(
            "watched_guest_down",
            "critical",
            f"Watched guest {vmid} is {status}",
            {"vmid": vmid, "name": guest.get("name"), "node": guest.get("node"), "status": status},
        ))
    return signals


def proxmox_environment_name(payload: dict[str, Any]) -> str:
    data = payload.get("data")
    if isinstance(data, list):
        for item in data:
            if isinstance(item, dict) and item.get("type") == "cluster" and item.get("name"):
                return str(item["name"])
    return "proxmox"




def proxmox_config_environment(config: dict[str, Any], watched_vmids: list[int]) -> dict[str, Any]:
    return {
        "api_url": str(config["api_url"]),
        "watched_guest_count": len(watched_vmids),
    }


def proxmox_observed_environment(
    config: dict[str, Any],
    cluster_payload: dict[str, Any],
    nodes: list[dict[str, Any]],
    storage: list[dict[str, Any]],
    watched_vmids: list[int],
    watched_guests: list[dict[str, Any]],
) -> dict[str, Any]:
    environment = proxmox_config_environment(config, watched_vmids)
    environment.update({
        "name": proxmox_environment_name(cluster_payload),
        "node_count": len(nodes),
        "online_node_count": len([node for node in nodes if node.get("status") == "online"]),
        "running_watched_guest_count": len([guest for guest in watched_guests if guest.get("status") == "running"]),
        "storage_count": len(storage),
    })
    return environment


def proxmox_empty_payload(status: str, signals: list[dict[str, Any]], environment: dict[str, Any] | None = None) -> dict[str, Any]:
    condition = proxmox_condition_from_signals(signals)
    payload = proxmox_base_payload(condition, status, signals)
    if environment is not None:
        payload["environment"] = environment
    payload["nodes"] = []
    payload["watched_guests"] = []
    return payload


def build_proxmox_summary(config: dict[str, Any]) -> dict[str, Any]:
    module_config = proxmox_config(config)
    missing = missing_proxmox_config_fields(module_config)
    watched_vmids = proxmox_watched_guests(module_config)
    if missing:
        return proxmox_empty_payload(
            "unconfigured",
            [proxmox_signal("unconfigured", "unavailable", "missing Proxmox config", {"missing_fields": missing})],
        )

    environment = proxmox_config_environment(module_config, watched_vmids)
    if str(module_config.get("token_secret")) == "CHANGE_ME":
        return proxmox_empty_payload(
            "not_observed",
            [proxmox_signal("api_unavailable", "unavailable", "Proxmox API polling not configured with a real token")],
            environment,
        )

    try:
        cluster_payload = proxmox_api_get(module_config, "/cluster/status")
        nodes = parse_proxmox_nodes(proxmox_api_get(module_config, "/nodes"))
        storage = collect_proxmox_storage(module_config, nodes)
        watched_guests = collect_proxmox_watched_guests(module_config, nodes, watched_vmids)
    except Exception as exc:
        return proxmox_empty_payload(
            "api_unavailable",
            [proxmox_signal("api_unavailable", "unavailable", proxmox_api_error_summary(exc))],
            environment,
        )

    signals = [*proxmox_node_signals(nodes), *proxmox_storage_signals(storage), *proxmox_watched_guest_signals(watched_guests)]
    condition = proxmox_condition_from_signals(signals)
    payload = proxmox_base_payload(condition, "observed", signals)
    payload.update({
        "observed_at": iso_utc(),
        "age_seconds": 0,
        "environment": proxmox_observed_environment(module_config, cluster_payload, nodes, storage, watched_vmids, watched_guests),
        "nodes": nodes,
        "storage": storage,
        "watched_guests": watched_guests,
    })
    return payload

def module_placeholder(spec: ModuleSpec, enabled: bool) -> dict[str, Any]:
    payload = {
        "enabled": enabled,
        "page": spec.page,
        "severity": "unknown" if enabled else "disabled",
        "status": spec.status,
        "implemented": False,
    }
    if enabled:
        payload["condition"] = "unavailable"
    return payload


class ModuleRuntime:
    def __init__(self, specs: dict[str, ModuleSpec]) -> None:
        self.specs = specs

    def evaluate_module(self, spec: ModuleSpec, enabled: set[str], config: dict[str, Any]) -> ModuleEvaluation:
        is_enabled = spec.name in enabled
        if is_enabled and spec.summary_fn:
            payload = spec.summary_fn(config)
            return ModuleEvaluation(payload=payload, condition=str(payload.get("condition", "unavailable")))

        payload = module_placeholder(spec, is_enabled)
        if not is_enabled:
            return ModuleEvaluation(payload=payload)

        return ModuleEvaluation(
            payload=payload,
            condition="unavailable",
            problem=f"module {spec.name} is enabled but not implemented in this clean baseline",
        )

    def compatibility_aliases(self, modules: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
        nut = modules.get("nut", {})
        return (
            nut.get("ups", unavailable_ups("NUT module disabled")),
            nut.get("nut", {"client_count": 0, "would_shutdown": False}),
        )

    def build(self, config: dict[str, Any], enabled: set[str]) -> ModuleRuntimeResult:
        modules: dict[str, Any] = {}
        conditions: list[str] = []
        problems: list[str] = []
        pages: list[str] = []
        for name, spec in self.specs.items():
            evaluation = self.evaluate_module(spec, enabled, config)
            modules[name] = evaluation.payload
            if name in enabled:
                pages.append(spec.page)
            if evaluation.condition is not None:
                conditions.append(evaluation.condition)
            if evaluation.problem is not None:
                problems.append(evaluation.problem)

        condition = summary_condition(conditions)
        ups_alias, nut_alias = self.compatibility_aliases(modules)
        return ModuleRuntimeResult(
            modules=modules,
            condition=condition,
            severity=severity_for_condition(condition),
            available_modules=list(self.specs.keys()),
            pages=pages,
            problems=problems,
            ups=ups_alias,
            nut=nut_alias,
        )


MODULES: dict[str, ModuleSpec] = {
    "nut": ModuleSpec("nut", "NUT", "implemented", build_nut_summary),
    "proxmox": ModuleSpec("proxmox", "PROXMOX", "implemented", build_proxmox_summary),
    "ha": ModuleSpec("ha", "HA", "placeholder"),
}


def build_summary(config: dict[str, Any] | None = None, *, stackflow_safe: bool = False) -> dict[str, Any]:
    config = config if config is not None else load_json_file(POWER_SENTINEL_CONFIG)
    enabled = enabled_modules(config)
    runtime = ModuleRuntime(MODULES).build(config, enabled)
    summary = {
        "schema": SCHEMA_SUMMARY,
        "timestamp": iso_utc(),
        "product": "Power Sentinel",
        "profile": "nut-monitor-clean-baseline",
        "condition": runtime.condition,
        "severity": runtime.severity,
        "stackflow_safe": bool(stackflow_safe),
        "enabled_modules": sorted(enabled),
        "available_modules": runtime.available_modules,
        "pages": runtime.pages,
        "module": {**module_lan_status(), "time_hhmm": local_time_hhmm()},
        "modules": runtime.modules,
        # Compatibility aliases for the current CoreS3 NUT monitor firmware.
        "ups": runtime.ups,
        "nut": runtime.nut,
        "problems": runtime.problems,
    }
    return summary


def build_health_payload(summary: dict[str, Any]) -> dict[str, Any]:
    condition = str(summary.get("condition", "unavailable"))
    severity = str(summary.get("severity", severity_for_condition(condition)))
    return {
        "schema": SCHEMA_HEALTH,
        "ok": condition != "critical",
        "condition": condition,
        "severity": severity,
        "timestamp": iso_utc(),
    }


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
            self.send_json(build_health_payload(summary))
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

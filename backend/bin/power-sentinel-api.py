#!/usr/bin/env python3
"""Power Sentinel local JSON API for M5Stack LLM Module.

The API works before the UPS USB OTG cable is attached: UPS data is reported as
explicitly unavailable until NUT/upsc can provide live values. No plausible UPS
sample values are emitted by the backend.
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import socket
import ssl
import subprocess
import sys
import tempfile
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any
from urllib.parse import parse_qs, urlparse
from urllib.request import Request, urlopen

SCHEMA_SUMMARY = "power-sentinel.summary.v1"
SCHEMA_HEALTH = "power-sentinel.health.v1"
DEFAULT_HOST = "0.0.0.0"
DEFAULT_PORT = 8088
HEALTHCHECK = os.environ.get("POWER_SENTINEL_HEALTHCHECK", "/usr/local/bin/m5stack-healthcheck")
UPSC = os.environ.get("POWER_SENTINEL_UPSC", "/usr/bin/upsc")
UPS_NAME = os.environ.get("POWER_SENTINEL_UPS", "homelab_ups@localhost")
HA_HOST = os.environ.get("POWER_SENTINEL_HA_HOST", "192.168.2.200")
MQTT_HOST = os.environ.get("POWER_SENTINEL_MQTT_HOST", HA_HOST)
PROXMOX_HOST = os.environ.get("POWER_SENTINEL_PROXMOX_HOST", "192.168.2.99")
PROXMOX_PORT = int(os.environ.get("POWER_SENTINEL_PROXMOX_PORT", "8006"))
PROXMOX_NODE = os.environ.get("POWER_SENTINEL_PROXMOX_NODE", "pve")
PROXMOX_TOKEN_ID = os.environ.get("POWER_SENTINEL_PROXMOX_TOKEN_ID")
PROXMOX_TOKEN_SECRET = os.environ.get("POWER_SENTINEL_PROXMOX_TOKEN_SECRET")
PROXMOX_VERIFY_SSL = os.environ.get("POWER_SENTINEL_PROXMOX_VERIFY_SSL", "0").lower() in ("1", "true", "yes")
POWER_SENTINEL_CONFIG = os.environ.get("POWER_SENTINEL_CONFIG", "/etc/power-sentinel.json")
MQTT_FALLBACK_CONFIG = os.environ.get("POWER_SENTINEL_MQTT_FALLBACK_CONFIG", "/etc/m5stack-ha-publish.json")
ZIGBEE2MQTT_BASE_TOPIC = os.environ.get("POWER_SENTINEL_Z2M_BASE_TOPIC", "zigbee2mqtt")
NETWORK_PROBE_HOST = os.environ.get("POWER_SENTINEL_NETWORK_PROBE_HOST", "1.1.1.1")
NETWORK_PROBE_PORT = int(os.environ.get("POWER_SENTINEL_NETWORK_PROBE_PORT", "53"))
NUT_CLIENTS_FILE = os.environ.get("POWER_SENTINEL_NUT_CLIENTS_FILE", "/etc/power-sentinel-nut-clients.json")


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


def has_default_route(path: str = "/proc/net/route") -> bool:
    try:
        with open(path, "r", encoding="utf-8") as fh:
            next(fh, None)
            for line in fh:
                fields = line.split()
                if len(fields) >= 4 and fields[1] == "00000000" and int(fields[3], 16) & 0x2:
                    return True
    except (OSError, ValueError):
        return False
    return False


def load_network_status() -> dict[str, Any]:
    default_route = has_default_route()
    target = f"{NETWORK_PROBE_HOST}:{NETWORK_PROBE_PORT}"
    available = default_route and tcp_check(NETWORK_PROBE_HOST, NETWORK_PROBE_PORT, timeout=0.75)
    return {
        "available": available,
        "severity": "ok" if available else "warn",
        "default_route": default_route,
        "probe": "tcp",
        "target": target,
    }


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


def health_bool_or_unknown(health: dict[str, Any] | None, *path: str) -> bool | None:
    value = health_value(health, *path, default=None)
    if value is None:
        return None
    if isinstance(value, dict) and value.get("not_run"):
        return None
    return bool(value)


def load_m5stack_health() -> dict[str, Any] | None:
    if not os.path.exists(HEALTHCHECK):
        return None
    return run_json_command([HEALTHCHECK, "--json"])


def load_site_config(path: str = POWER_SENTINEL_CONFIG) -> dict[str, Any]:
    try:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, json.JSONDecodeError):
        return {}
    return data if isinstance(data, dict) else {}


def proxmox_config(config: dict[str, Any] | None = None) -> dict[str, Any]:
    pve = (config or load_site_config()).get("proxmox", {})
    if not isinstance(pve, dict):
        pve = {}
    return {
        "host": os.environ.get("POWER_SENTINEL_PROXMOX_HOST") or pve.get("host") or PROXMOX_HOST,
        "port": int(os.environ.get("POWER_SENTINEL_PROXMOX_PORT") or pve.get("port") or PROXMOX_PORT),
        "node": os.environ.get("POWER_SENTINEL_PROXMOX_NODE") or pve.get("node") or PROXMOX_NODE,
        "token_id": os.environ.get("POWER_SENTINEL_PROXMOX_TOKEN_ID") or pve.get("token_id") or pve.get("api_token_id") or PROXMOX_TOKEN_ID,
        "token_secret": os.environ.get("POWER_SENTINEL_PROXMOX_TOKEN_SECRET") or pve.get("token_secret") or pve.get("api_token_secret") or PROXMOX_TOKEN_SECRET,
        "verify_ssl": bool(pve.get("verify_ssl", PROXMOX_VERIFY_SSL)),
    }


def mqtt_config(config: dict[str, Any] | None = None) -> dict[str, Any]:
    site = config or load_site_config()
    mqtt = site.get("mqtt", {}) if isinstance(site, dict) else {}
    if not isinstance(mqtt, dict):
        mqtt = {}
    fallback_path = mqtt.get("config_file") or MQTT_FALLBACK_CONFIG
    fallback = load_site_config(fallback_path) if fallback_path else {}
    return {
        "host": os.environ.get("POWER_SENTINEL_MQTT_HOST") or mqtt.get("host") or fallback.get("host") or MQTT_HOST,
        "port": int(os.environ.get("POWER_SENTINEL_MQTT_PORT") or mqtt.get("port") or fallback.get("port") or 1883),
        "username": os.environ.get("POWER_SENTINEL_MQTT_USERNAME") or mqtt.get("username") or fallback.get("username"),
        "password": os.environ.get("POWER_SENTINEL_MQTT_PASSWORD") or mqtt.get("password") or fallback.get("password"),
    }


def homeassistant_config(config: dict[str, Any] | None = None) -> dict[str, Any]:
    site = config or load_site_config()
    ha = site.get("homeassistant", {}) if isinstance(site, dict) else {}
    if not isinstance(ha, dict):
        ha = {}
    return {
        "host": os.environ.get("POWER_SENTINEL_HA_HOST") or ha.get("host") or HA_HOST,
        "port": int(os.environ.get("POWER_SENTINEL_HA_PORT") or ha.get("port") or 8123),
        "token": os.environ.get("POWER_SENTINEL_HA_TOKEN") or ha.get("token") or ha.get("long_lived_token"),
        "verify_ssl": bool(ha.get("verify_ssl", False)),
        "scheme": os.environ.get("POWER_SENTINEL_HA_SCHEME") or ha.get("scheme") or "http",
    }


def zigbee2mqtt_config(config: dict[str, Any] | None = None) -> dict[str, Any]:
    z2m = (config or load_site_config()).get("zigbee2mqtt", {})
    if not isinstance(z2m, dict):
        z2m = {}
    return {"base_topic": os.environ.get("POWER_SENTINEL_Z2M_BASE_TOPIC") or z2m.get("base_topic") or ZIGBEE2MQTT_BASE_TOPIC}


def mosquitto_config_lines(cfg: dict[str, Any]) -> str:
    lines = [f"-h {cfg.get('host')}", f"-p {int(cfg.get('port', 1883))}"]
    if cfg.get("username"):
        lines.append(f"-u {cfg['username']}")
    if cfg.get("password"):
        lines.append(f"-P {cfg['password']}")
    return "\n".join(lines) + "\n"


def mosquitto_sub_command(topic: str, timeout: int = 3) -> list[str]:
    return ["mosquitto_sub", "-C", "1", "-W", str(timeout), "-t", topic]


def run_mosquitto_sub(cfg: dict[str, Any], topic: str, timeout: int = 3) -> str | None:
    with tempfile.TemporaryDirectory(prefix="power-sentinel-mqtt-") as td:
        config_path = os.path.join(td, "mosquitto_sub")
        with open(config_path, "w", encoding="utf-8") as fh:
            fh.write(mosquitto_config_lines(cfg))
        os.chmod(config_path, 0o600)
        env = os.environ.copy()
        env["XDG_CONFIG_HOME"] = td
        try:
            cp = subprocess.run(mosquitto_sub_command(topic, timeout=timeout), stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout + 2, check=False, env=env)
        except (OSError, subprocess.SubprocessError):
            return None
    if cp.returncode != 0 or not cp.stdout.strip():
        return None
    return cp.stdout.strip()


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
    load_percent = as_int(raw.get("ups.load"))
    realpower_nominal_w = as_int(raw.get("ups.realpower.nominal"))
    load_w = None
    if load_percent is not None and realpower_nominal_w is not None:
        load_w = int(round(realpower_nominal_w * load_percent / 100.0))
    manufacturer = raw.get("ups.mfr") or raw.get("device.mfr")
    model = raw.get("ups.model") or raw.get("device.model")
    serial = raw.get("ups.serial") or raw.get("device.serial")
    return {
        "available": True,
        "status": status,
        "status_label": ups_status_label(status),
        "on_battery": "OB" in tokens,
        "low_battery": "LB" in tokens,
        "battery_percent": as_int(raw.get("battery.charge")),
        "runtime_seconds": as_int(raw.get("battery.runtime")),
        "load_percent": load_percent,
        "input_voltage": as_float(raw.get("input.voltage")),
        "output_voltage": as_float(raw.get("output.voltage")),
        "battery_voltage": as_float(raw.get("battery.voltage")),
        "realpower_nominal_w": realpower_nominal_w,
        "load_w": load_w,
        "beeper_status": raw.get("ups.beeper.status"),
        "transfer_reason": raw.get("input.transfer.reason"),
        "driver": raw.get("driver.name"),
        "firmware": raw.get("ups.firmware"),
        "manufacturer": manufacturer,
        "model": model,
        "serial": serial,
        "stale": False,
        "age_seconds": 0,
        "device": {
            "manufacturer": manufacturer,
            "model": model,
            "serial": serial,
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


def systemd_is_active(service: str) -> bool | None:
    try:
        cp = subprocess.run(["systemctl", "is-active", service], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, timeout=0.75, check=False)
    except (OSError, subprocess.SubprocessError):
        return None
    return cp.stdout.strip() == "active"


def nut_mode(path: str = "/etc/nut/nut.conf") -> str | None:
    try:
        with open(path, "r", encoding="utf-8") as fh:
            for line in fh:
                stripped = line.strip()
                if stripped.startswith("MODE="):
                    return stripped.split("=", 1)[1].strip().strip('"\'') or None
    except OSError:
        return None
    return None


def load_nut_status() -> dict[str, Any]:
    monitor_active = systemd_is_active("nut-monitor.service")
    return {
        "server_active": systemd_is_active("nut-server.service"),
        "driver_active": systemd_is_active("nut-driver.service"),
        "monitor_active": monitor_active,
        "mode": nut_mode(),
        "client_count": None,
        "clients": [],
        "shutdown_state": "unknown" if monitor_active is None else ("armed" if monitor_active else "disarmed"),
    }


def client_list_mentions_target(clients: list[Any], target: str | None) -> bool:
    needle = str(target or "").strip().lower()
    if not needle:
        return False
    for client in clients:
        if needle in str(client).lower():
            return True
    return False


def summarize_nut_client(client: dict[str, Any], nut: dict[str, Any]) -> dict[str, Any]:
    clients = nut.get("clients", []) if isinstance(nut.get("clients"), list) else []
    name = str(client.get("name") or client.get("hostname") or client.get("host") or "unknown")
    host = str(client.get("host") or "")
    role = str(client.get("role") or "secondary")
    matchers = [name, host, str(client.get("hostname") or "")]
    connected = any(client_list_mentions_target(clients, matcher) for matcher in matchers)
    package_installed = client.get("package_installed")
    upsc_reachable = client.get("upsc_reachable")
    config_present = client.get("config_present")
    monitor_active = client.get("monitor_active")
    configured = bool(config_present) or connected or bool(monitor_active)
    armed = bool(monitor_active and connected)
    if armed:
        state = "armed"
    elif connected:
        state = "connected_as_upsmon"
    elif upsc_reachable:
        state = "reachable_via_upsc"
    elif configured:
        state = "configured_not_connected"
    else:
        state = "not_configured"
    return {
        "name": name,
        "host": host or None,
        "role": role,
        "package_installed": package_installed,
        "reachable_via_upsc": upsc_reachable,
        "configured": configured,
        "connected_as_upsmon": connected,
        "armed": armed,
        "state": state,
    }


def load_nut_clients(path: str = NUT_CLIENTS_FILE) -> list[dict[str, Any]]:
    try:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, json.JSONDecodeError):
        return []
    if isinstance(data, dict):
        raw_clients = data.get("clients", [])
    elif isinstance(data, list):
        raw_clients = data
    else:
        raw_clients = []
    return [client for client in raw_clients if isinstance(client, dict)]


def summarize_nut_clients(clients: list[dict[str, Any]], nut: dict[str, Any]) -> list[dict[str, Any]]:
    return [summarize_nut_client(client, nut) for client in clients]


def nut_client_summary(clients: list[dict[str, Any]]) -> dict[str, int]:
    return {
        "total": len(clients),
        "secondary_total": sum(1 for client in clients if client.get("role") == "secondary"),
        "connected": sum(1 for client in clients if client.get("connected_as_upsmon")),
        "armed": sum(1 for client in clients if client.get("armed")),
    }


def summarize_standard_nut_shutdown(ups: dict[str, Any], nut: dict[str, Any], clients: list[dict[str, Any]] | None = None) -> dict[str, Any]:
    raw = ups.get("raw", {}) if isinstance(ups.get("raw"), dict) else {}
    monitor_active = bool(nut.get("monitor_active"))
    primary_ready = bool(ups.get("available") and nut.get("server_active") and nut.get("driver_active") and nut.get("mode") == "netserver")
    raw_clients = clients or []
    summarized_clients = summarize_nut_clients(raw_clients, nut)
    summary = nut_client_summary(summarized_clients)
    secondary_ready = any(client.get("role") == "secondary" and (client.get("connected_as_upsmon") or client.get("armed")) for client in summarized_clients)
    if ups.get("low_battery"):
        would_shutdown = True
        reason = "UPS low battery; standard NUT upsmon would initiate shutdown"
    elif ups.get("on_battery"):
        would_shutdown = False
        reason = "UPS on battery, waiting for NUT LOWBATT/FSD"
    elif ups.get("available"):
        would_shutdown = False
        reason = "UPS online"
    else:
        would_shutdown = False
        reason = "UPS unavailable"
    return {
        "armed": monitor_active,
        "real_shutdown_owner": "upsmon",
        "primary_ready": primary_ready,
        "primary_monitor_active": monitor_active,
        "secondary_ready": secondary_ready,
        "nut_clients": summarized_clients,
        "nut_client_summary": summary,
        "would_shutdown": would_shutdown,
        "reason": reason,
        "thresholds": {
            "battery_charge_low_percent": as_int(raw.get("battery.charge.low")),
            "battery_runtime_low_seconds": as_int(raw.get("battery.runtime.low")),
        },
        "next_step": "Configure and test upsmon clients before enabling real shutdown" if not monitor_active else "Standard NUT upsmon is active",
    }


def unavailable_ups() -> dict[str, Any]:
    return {
        "available": False,
        "status": "UNAVAILABLE",
        "status_label": "UPS unavailable",
        "on_battery": False,
        "low_battery": False,
        "battery_percent": None,
        "runtime_seconds": None,
        "load_percent": None,
        "input_voltage": None,
        "output_voltage": None,
        "battery_voltage": None,
        "realpower_nominal_w": None,
        "load_w": None,
        "beeper_status": None,
        "transfer_reason": None,
        "driver": None,
        "firmware": None,
        "manufacturer": None,
        "model": None,
        "serial": None,
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


def derive_severity(ups: dict[str, Any], m5_ok: bool, checks: dict[str, bool], problems: list[str], pve: dict[str, Any] | None = None) -> str:
    if ups.get("low_battery"):
        return "critical"
    if pve and pve.get("severity") == "critical":
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


def lightweight_m5stack_health(stackflow_ok: bool = False) -> dict[str, Any]:
    """Fast, non-recursive health snapshot for calls already inside llm_sys.

    The normal healthcheck probes StackFlow port 10001. That is correct for HTTP
    clients, but deadlocks/timeouts when a custom StackFlow unit calls the API
    while llm_sys is waiting for that same unit's response. This lightweight
    variant avoids any StackFlow/OpenAI socket calls and is safe for
    /api/v1/summary?stackflow_safe=1.
    """
    mem_available_mb = None
    try:
        with open("/proc/meminfo", "r", encoding="utf-8") as fh:
            for line in fh:
                if line.startswith("MemAvailable:"):
                    mem_available_mb = round(int(line.split()[1]) / 1024, 1)
                    break
    except OSError:
        pass

    disk_free_gb = None
    try:
        st = os.statvfs("/")
        disk_free_gb = round((st.f_bavail * st.f_frsize) / (1024**3), 2)
    except OSError:
        pass

    temperature_c = None
    for path in ("/sys/class/thermal/thermal_zone0/temp", "/sys/class/hwmon/hwmon0/temp1_input"):
        try:
            raw = open(path, "r", encoding="utf-8").read().strip()
            temperature_c = round(float(raw) / 1000.0, 1)
            break
        except (OSError, ValueError):
            continue

    return {
        "overall_ok": True,
        "system": {
            "temperature_c": temperature_c,
            "linux_mem": {"available_mb": mem_available_mb},
            "root_disk": {"free_gb": disk_free_gb},
        },
        "ports": {
            "stackflow_10001": {"ok": bool(stackflow_ok)},
            "openai_8000": {"ok": tcp_check("127.0.0.1", 8000, timeout=0.2)},
        },
        "chat_smoke": {"ok": False, "not_run": True},
    }


def unavailable_proxmox(reason: str = "not configured") -> dict[str, Any]:
    return {
        "available": False,
        "severity": "warn",
        "node": None,
        "node_status": "unavailable",
        "api_latency_ms": None,
        "cpu_percent": None,
        "ram_percent": None,
        "cpu_temp_c": None,
        "storage_percent": None,
        "zfs": {"status": "UNKNOWN", "pools": []},
        "smart": {"status": "UNKNOWN", "failing_count": None, "warning_count": None},
        "vm": {"running_count": 0, "running_names": [], "running_items": []},
        "lxc": {"running_count": 0, "running_names": [], "running_items": []},
        "shutdown_state": "disarmed",
        "problems": [f"Proxmox {reason}"],
    }


def percent_ratio(used: Any, total: Any) -> int | None:
    try:
        used_f = float(used)
        total_f = float(total)
    except (TypeError, ValueError):
        return None
    if total_f <= 0:
        return None
    return int(round(used_f * 100.0 / total_f))


def workload_metric_summary(items: list[dict[str, Any]], limit: int = 6) -> dict[str, Any]:
    names: list[str] = []
    running_items: list[dict[str, Any]] = []
    for item in items:
        if item.get("status") != "running":
            continue
        name = str(item.get("name") or item.get("vmid") or item.get("id") or "unknown")
        names.append(name)
        if len(running_items) >= limit:
            continue
        cpu_percent = None
        if item.get("cpu") is not None:
            try:
                cpu_percent = int(round(float(item.get("cpu", 0)) * 100))
            except (TypeError, ValueError):
                cpu_percent = None
        ram_total = item.get("maxmem") or item.get("mem_total") or item.get("memory_total")
        disk_used = item.get("disk") or item.get("disk_used")
        disk_total = item.get("maxdisk") or item.get("disk_total")
        running_items.append({
            "name": name,
            "cpu_percent": cpu_percent,
            "ram_percent": percent_ratio(item.get("mem"), ram_total),
            "ram_total_bytes": ram_total,
            "disk_percent": percent_ratio(disk_used, disk_total),
            "disk_total_bytes": disk_total,
        })
    return {
        "running_count": len(names),
        "running_names": names[:limit],
        "running_items": running_items,
        "more": max(0, len(names) - limit),
    }


def zfs_summary(pools: list[dict[str, Any]]) -> dict[str, Any]:
    normalized: list[dict[str, Any]] = []
    status = "UNKNOWN"
    worst_bad = False
    for pool in pools or []:
        health = str(pool.get("health") or pool.get("state") or "UNKNOWN").upper()
        cap = pool.get("capacity") or pool.get("cap")
        if cap is None:
            cap = percent_ratio(pool.get("alloc"), pool.get("size"))
        normalized.append({"name": pool.get("name") or pool.get("pool") or "unknown", "health": health, "capacity_percent": cap})
        if health not in ("ONLINE", "OK"):
            worst_bad = True
        elif status == "UNKNOWN":
            status = health
    if worst_bad:
        for pool in normalized:
            if pool["health"] not in ("ONLINE", "OK"):
                status = pool["health"]
                break
    return {"status": status, "pools": normalized}


def smart_summary(disks: list[dict[str, Any]]) -> dict[str, Any]:
    failing = 0
    warning = 0
    for disk in disks or []:
        health = str(disk.get("health") or disk.get("smart_health") or disk.get("status") or "").upper()
        if any(token in health for token in ("FAIL", "BAD", "CRIT")):
            failing += 1
        elif health and health not in ("OK", "PASSED", "PASS", "UNKNOWN"):
            warning += 1
    if failing:
        status = "FAIL"
    elif warning:
        status = "WARN"
    elif disks:
        status = "OK"
    else:
        status = "UNKNOWN"
    return {"status": status, "failing_count": failing, "warning_count": warning}


def summarize_proxmox_data(
    node: str,
    latency_ms: int | None,
    node_status: dict[str, Any],
    qemu: list[dict[str, Any]],
    lxc: list[dict[str, Any]],
    zfs: list[dict[str, Any]],
    disks: list[dict[str, Any]],
) -> dict[str, Any]:
    cpu_percent = None
    if node_status.get("cpu") is not None:
        cpu_percent = int(round(float(node_status.get("cpu", 0)) * 100))
    memory = node_status.get("memory") or {}
    ram_percent = percent_ratio(
        node_status.get("mem", memory.get("used")),
        node_status.get("maxmem", memory.get("total")),
    )
    rootfs = node_status.get("rootfs") or {}
    storage_percent = percent_ratio(rootfs.get("used"), rootfs.get("total"))
    zfs_info = zfs_summary(zfs)
    smart_info = smart_summary(disks)
    problems: list[str] = []
    node_state = str(node_status.get("status") or "online")
    if node_state.lower() not in ("online", "ok"):
        problems.append("Proxmox node down")
    if zfs_info["status"] not in ("ONLINE", "OK", "UNKNOWN"):
        problems.append(f"ZFS {zfs_info['status']}")
    if smart_info["status"] == "FAIL":
        problems.append("SMART disk health failure")
    elif smart_info["status"] == "WARN":
        problems.append("SMART disk health warning")
    cpu_temp_c = node_status.get("cpu_temp_c") or node_status.get("temperature_c")
    if cpu_temp_c is not None:
        try:
            temp = float(cpu_temp_c)
            if temp >= 80:
                problems.append("PVE CPU temperature critical")
            elif temp >= 70:
                problems.append("PVE CPU temperature warning")
        except (TypeError, ValueError):
            pass
    critical = any(p.startswith(("Proxmox node", "ZFS", "SMART disk health failure", "PVE CPU temperature critical")) for p in problems)
    return {
        "available": True,
        "severity": "critical" if critical else ("warn" if problems else "ok"),
        "node": node,
        "node_status": node_state,
        "api_latency_ms": latency_ms,
        "cpu_percent": cpu_percent,
        "ram_percent": ram_percent,
        "cpu_temp_c": cpu_temp_c,
        "storage_percent": storage_percent,
        "zfs": zfs_info,
        "smart": smart_info,
        "vm": workload_metric_summary(qemu),
        "lxc": workload_metric_summary(lxc),
        "shutdown_state": "disarmed",
        "problems": problems,
    }


def proxmox_api_get(cfg: dict[str, Any], path: str, timeout: float = 2.5) -> Any:
    token_id = cfg.get("token_id")
    token_secret = cfg.get("token_secret")
    if not token_id or not token_secret:
        raise RuntimeError("API token not configured")
    url = f"https://{cfg['host']}:{cfg['port']}/api2/json{path}"
    req = Request(url, headers={"Authorization": f"PVEAPIToken={token_id}={token_secret}"})
    context = None if cfg.get("verify_ssl") else ssl._create_unverified_context()
    with urlopen(req, timeout=timeout, context=context) as resp:  # noqa: S310 - local homelab endpoint, optional TLS verification
        payload = json.loads(resp.read().decode("utf-8"))
    return payload.get("data")


def load_proxmox() -> dict[str, Any]:
    cfg = proxmox_config()
    if not cfg.get("token_id") or not cfg.get("token_secret"):
        return unavailable_proxmox("API token not configured")
    node = cfg.get("node") or PROXMOX_NODE
    started = time.monotonic()
    try:
        node_status = proxmox_api_get(cfg, f"/nodes/{node}/status") or {}
        qemu = proxmox_api_get(cfg, f"/nodes/{node}/qemu") or []
        lxc = proxmox_api_get(cfg, f"/nodes/{node}/lxc") or []
        try:
            zfs = proxmox_api_get(cfg, f"/nodes/{node}/disks/zfs") or []
        except Exception:
            zfs = []
        try:
            disks = proxmox_api_get(cfg, f"/nodes/{node}/disks/list") or []
        except Exception:
            disks = []
    except Exception as exc:
        return unavailable_proxmox(f"API read failed: {type(exc).__name__}")
    latency_ms = int(round((time.monotonic() - started) * 1000))
    return summarize_proxmox_data(node=node, latency_ms=latency_ms, node_status=node_status, qemu=qemu, lxc=lxc, zfs=zfs, disks=disks)


def parse_mqtt_json(payload: str | None) -> Any:
    if not payload:
        return None
    try:
        return json.loads(payload)
    except json.JSONDecodeError:
        return payload


def summarize_zigbee2mqtt_payloads(base_topic: str, state_payload: str | None, info_payload: str | None, devices_payload: str | None) -> dict[str, Any]:
    state_obj = parse_mqtt_json(state_payload)
    if isinstance(state_obj, dict):
        state = str(state_obj.get("state") or state_obj.get("status") or "unknown")
    elif isinstance(state_obj, str):
        state = state_obj.strip() or "unknown"
    else:
        state = "unknown"
    info = parse_mqtt_json(info_payload)
    if not isinstance(info, dict):
        info = {}
    devices = parse_mqtt_json(devices_payload)
    if not isinstance(devices, list):
        devices = []
    coordinator = info.get("coordinator") if isinstance(info.get("coordinator"), dict) else {}
    meta = coordinator.get("meta") if isinstance(coordinator.get("meta"), dict) else {}
    firmware = meta.get("revision") or meta.get("fw_revision") or meta.get("version") or coordinator.get("firmware")
    interviewed = 0
    disabled = 0
    for item in devices:
        if not isinstance(item, dict):
            continue
        if item.get("disabled"):
            disabled += 1
        if item.get("interview_completed") or str(item.get("type", "")).lower() == "coordinator":
            interviewed += 1
    problems: list[str] = []
    if state.lower() != "online":
        problems.append(f"Zigbee2MQTT {state}")
    if not coordinator:
        problems.append("Zigbee2MQTT coordinator info unavailable")
    available = state.lower() == "online"
    advanced = info.get("config", {}).get("advanced", {}) if isinstance(info.get("config"), dict) else {}
    return {
        "available": available,
        "severity": "warn" if problems else "ok",
        "state": state,
        "base_topic": base_topic,
        "version": info.get("version"),
        "coordinator": {
            "available": bool(coordinator),
            "type": coordinator.get("type"),
            "ieee_address": coordinator.get("ieee_address"),
            "firmware": firmware,
        },
        "network": {"channel": advanced.get("channel"), "pan_id": advanced.get("pan_id")},
        "devices": {"total": len(devices), "interviewed": interviewed, "disabled": disabled},
        "problems": problems,
    }


def ha_api_get(cfg: dict[str, Any], path: str, timeout: float = 2.5) -> Any:
    token = cfg.get("token")
    if not token:
        raise RuntimeError("HA token not configured")
    scheme = str(cfg.get("scheme") or "http")
    host = str(cfg.get("host") or HA_HOST)
    port = int(cfg.get("port") or 8123)
    url = f"{scheme}://{host}:{port}{path}"
    req = Request(url, headers={"Authorization": f"Bearer {token}", "Accept": "application/json"})
    context = None if cfg.get("verify_ssl") else ssl._create_unverified_context()
    with urlopen(req, timeout=timeout, context=context) as resp:  # noqa: S310 - local homelab endpoint, optional TLS verification
        return json.loads(resp.read().decode("utf-8"))


def summarize_homeassistant_updates(states: list[dict[str, Any]] | None, error: str | None = None) -> dict[str, Any]:
    if not states:
        return {"available": False, "available_count": 0, "items": [], "problems": [] if error in (None, "HA token not configured") else [f"HA updates {error}"]}
    items: list[dict[str, Any]] = []
    for state in states:
        if not isinstance(state, dict):
            continue
        entity_id = str(state.get("entity_id") or "")
        if not entity_id.startswith("update."):
            continue
        if str(state.get("state") or "").lower() != "on":
            continue
        attrs = state.get("attributes") if isinstance(state.get("attributes"), dict) else {}
        friendly = attrs.get("friendly_name") or entity_id.removeprefix("update.").replace("_", " ")
        items.append({
            "entity_id": entity_id,
            "name": str(friendly),
            "installed_version": attrs.get("installed_version"),
            "latest_version": attrs.get("latest_version"),
        })
    return {"available": True, "available_count": len(items), "items": items[:5], "problems": []}


def load_homeassistant_updates() -> dict[str, Any]:
    cfg = homeassistant_config()
    if not cfg.get("token"):
        return summarize_homeassistant_updates(None, "HA token not configured")
    try:
        states = ha_api_get(cfg, "/api/states")
    except Exception as exc:
        return summarize_homeassistant_updates(None, f"read failed: {type(exc).__name__}")
    if not isinstance(states, list):
        return summarize_homeassistant_updates(None, "unexpected payload")
    return summarize_homeassistant_updates(states)


def load_mqtt_status() -> dict[str, Any]:
    cfg = mqtt_config()
    ok = tcp_check(str(cfg.get("host")), int(cfg.get("port", 1883)))
    return {"available": ok, "severity": "ok" if ok else "critical", "broker": f"{cfg.get('host')}:{cfg.get('port')}"}


def load_homeassistant_mqtt_status() -> dict[str, Any]:
    cfg = mqtt_config()
    payload = run_mosquitto_sub(cfg, "homeassistant/status", timeout=2)
    status = "unknown"
    if payload:
        status = payload.strip().lower()
    return {"status": status, "source": "mqtt", "status_topic": "homeassistant/status"}


def load_zigbee2mqtt() -> dict[str, Any]:
    cfg = mqtt_config()
    zcfg = zigbee2mqtt_config()
    base = str(zcfg.get("base_topic") or ZIGBEE2MQTT_BASE_TOPIC).strip("/")
    state = run_mosquitto_sub(cfg, f"{base}/bridge/state", timeout=3)
    info = run_mosquitto_sub(cfg, f"{base}/bridge/info", timeout=4)
    devices = run_mosquitto_sub(cfg, f"{base}/bridge/devices", timeout=4)
    return summarize_zigbee2mqtt_payloads(base, state, info, devices)


def build_summary(
    now: float | int | None = None,
    health: dict[str, Any] | None = None,
    ups: dict[str, Any] | None = None,
    checks: dict[str, bool] | None = None,
    nut: dict[str, Any] | None = None,
    pve: dict[str, Any] | None = None,
    network: dict[str, Any] | None = None,
    mqtt: dict[str, Any] | None = None,
    homeassistant_mqtt: dict[str, Any] | None = None,
    homeassistant_updates: dict[str, Any] | None = None,
    zigbee2mqtt: dict[str, Any] | None = None,
    shutdown: dict[str, Any] | None = None,
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
        ups = load_ups() or unavailable_ups()
    if nut is None:
        nut = load_nut_status()
    if pve is None:
        pve = load_proxmox()
    if network is None:
        network = load_network_status()
    if mqtt is None:
        mqtt = load_mqtt_status()
    if homeassistant_mqtt is None:
        homeassistant_mqtt = load_homeassistant_mqtt_status()
    if homeassistant_updates is None:
        homeassistant_updates = load_homeassistant_updates()
    if zigbee2mqtt is None:
        zigbee2mqtt = load_zigbee2mqtt()
    if shutdown is None:
        nut_clients = load_nut_clients()
        shutdown = summarize_standard_nut_shutdown(ups, nut, nut_clients)

    m5_overall = bool(health_value(health, "overall_ok", default=False))
    problems: list[str] = []
    if not ups.get("available"):
        problems.append("UPS data unavailable")
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
    if not network.get("available", False):
        problems.append("Internet/network probe failed")
    if not mqtt.get("available", False):
        problems.append("MQTT broker unavailable")
    ha_mqtt_status = str(homeassistant_mqtt.get("status", "unknown")).lower()
    if ha_mqtt_status == "offline":
        problems.append("Home Assistant MQTT status offline")
    for problem in zigbee2mqtt.get("problems", []):
        if problem not in problems:
            problems.append(problem)
    for problem in pve.get("problems", []):
        if problem not in problems:
            problems.append(problem)

    severity = derive_severity(ups, m5_overall, checks, problems, pve=pve)
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
            "battery_voltage": ups.get("battery_voltage"),
            "realpower_nominal_w": ups.get("realpower_nominal_w"),
            "load_w": ups.get("load_w"),
            "beeper_status": ups.get("beeper_status"),
            "transfer_reason": ups.get("transfer_reason"),
            "driver": ups.get("driver"),
            "model": ups.get("model"),
            "stale": bool(ups.get("stale", False)),
            "age_seconds": ups.get("age_seconds"),
        },
        "homeassistant": {
            "available": bool(checks.get("homeassistant")),
            "severity": "ok" if checks.get("homeassistant") and ha_mqtt_status != "offline" else "warn",
            "mqtt": bool(checks.get("mqtt")),
            "status": homeassistant_mqtt.get("status", "unknown"),
            "source": homeassistant_mqtt.get("source", "mqtt"),
            "status_topic": homeassistant_mqtt.get("status_topic", "homeassistant/status"),
            "updates": {
                "available": bool(homeassistant_updates.get("available", False)),
                "available_count": int(homeassistant_updates.get("available_count") or 0),
                "items": homeassistant_updates.get("items", []),
            },
        },
        "mqtt": mqtt,
        "zigbee2mqtt": zigbee2mqtt,
        "nut": {
            "server_active": nut.get("server_active"),
            "driver_active": nut.get("driver_active"),
            "monitor_active": nut.get("monitor_active"),
            "mode": nut.get("mode"),
            "client_count": nut.get("client_count"),
            "clients": nut.get("clients", []),
            "shutdown_state": nut.get("shutdown_state", "unknown"),
        },
        "proxmox": pve,
        "shutdown": shutdown,
        "network": {
            "available": bool(network.get("available")),
            "severity": network.get("severity", "ok" if network.get("available") else "warn"),
            "default_route": bool(network.get("default_route")),
            "probe": network.get("probe", "tcp"),
            "target": network.get("target"),
        },
        "m5stack": {
            "available": health is not None,
            "severity": "ok" if m5_overall else "critical",
            "temperature_c": health_value(health, "system", "temperature_c"),
            "ram_available_mb": first_health_value(health, [("system", "linux_mem", "available_mb"), ("system", "ram_available_mb")]),
            "disk_free_gb": first_health_value(health, [("system", "root_disk", "free_gb"), ("system", "root_disk_free_gb")]),
            "stackflow_ok": bool(first_health_value(health, [("ports", "stackflow_10001", "ok"), ("stackflow", "lsmode_ok"), ("apis", "stackflow")], default=False)),
            "openai_ok": bool(first_health_value(health, [("ports", "openai_8000", "ok"), ("openai", "models", "ok"), ("apis", "openai")], default=False)),
            "chat_smoke_ok": None if health_value(health, "chat_smoke", "not_run", default=False) else health_bool_or_unknown(health, "chat_smoke", "ok"),
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
        query = parse_qs(parsed.query)
        if query.get("stackflow_safe", ["0"])[0] in ("1", "true", "yes"):
            return json_response(build_summary(health=lightweight_m5stack_health(stackflow_ok=True)))
        return json_response(build_summary())
    if parsed.path == "/api/v1/health":
        return json_response(build_health())
    if parsed.path == "/api/v1/ups":
        return json_response({"schema": "power-sentinel.ups.v1", "timestamp": iso_utc(), "ups": load_ups() or unavailable_ups()})
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

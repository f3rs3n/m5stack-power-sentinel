#!/usr/bin/env python3
import argparse
import json
import os
import subprocess
import sys
import tempfile
from datetime import datetime, timezone

DEFAULT_HOST = "192.168.2.200"
DEFAULT_PORT = 1883
DEFAULT_PREFIX = "m5stack/llm"
DISCOVERY_PREFIX = "homeassistant"
CLIENT_ID = "m5stack-llm-health"
HEALTHCHECK = "/usr/local/bin/m5stack-healthcheck"


def mosquitto_config_lines(host, port, username=None, password=None):
    lines = [f"-h {host}", f"-p {int(port)}"]
    if username is not None:
        lines.append(f"-u {username}")
    if password is not None:
        lines.append(f"-P {password}")
    return "\n".join(lines) + "\n"


def mosquitto_pub_command(topic, retain=True):
    cmd = ["mosquitto_pub", "-t", topic]
    if retain:
        cmd.append("-r")
    cmd.append("-s")
    return cmd


def mosquitto_publish(host, port, topic, payload, retain=True, username=None, password=None):
    if isinstance(payload, str):
        payload = payload.encode("utf-8")
    with tempfile.TemporaryDirectory(prefix="m5stack-mqtt-") as td:
        config_path = os.path.join(td, "mosquitto_pub")
        with open(config_path, "w", encoding="utf-8") as f:
            f.write(mosquitto_config_lines(host, port, username, password))
        os.chmod(config_path, 0o600)
        env = os.environ.copy()
        env["XDG_CONFIG_HOME"] = td
        cp = subprocess.run(mosquitto_pub_command(topic, retain=retain), input=payload, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env, timeout=20, check=False)
    if cp.returncode != 0:
        err = cp.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"mosquitto_pub failed rc={cp.returncode}: {err}")


def derive_summary(data):
    problems = []
    for name, state in sorted(data.get("services", {}).items()):
        if state != "active":
            problems.append(f"service {name} is {state}")
    for name, port in sorted(data.get("ports", {}).items()):
        if not port.get("ok"):
            problems.append(f"port {name} is down")
    if not data.get("stackflow", {}).get("lsmode_ok", False):
        problems.append("StackFlow lsmode failed")
    if not data.get("openai", {}).get("models", {}).get("ok", False):
        problems.append("OpenAI models endpoint failed")
    for note in data.get("notes", []):
        problems.append(str(note))

    led = data.get("led", {})
    r = int(led.get("R") or 0)
    g = int(led.get("G") or 0)
    b = int(led.get("B") or 0)
    if r == 0 and g == 0 and b == 0:
        led_color = "off"
    elif g >= r and g >= b and g > 0:
        led_color = "green"
    elif r >= g and r >= b and r > 0:
        led_color = "red"
    elif b >= r and b >= g and b > 0:
        led_color = "blue"
    else:
        led_color = "unknown"

    data["summary"] = {
        "status": "ok" if data.get("overall_ok") else "fail",
        "problem_count": len(problems),
        "problems": problems,
        "led_color": led_color,
    }
    return data


def run_healthcheck(include_chat=False):
    cmd = [HEALTHCHECK, "--json"]
    if include_chat:
        cmd.append("--chat")
    cp = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=120)
    if cp.returncode not in (0, 2):
        raise RuntimeError(f"healthcheck failed rc={cp.returncode}: {cp.stderr.strip()}")
    data = json.loads(cp.stdout)
    data["published_at"] = datetime.now(timezone.utc).isoformat()
    return derive_summary(data)


def sensor_config(component, object_id, name, state_topic, availability_topic, **kwargs):
    cfg = {
        "name": name,
        "unique_id": object_id,
        "state_topic": state_topic,
        "availability_topic": availability_topic,
        "device": {
            "identifiers": ["m5stack_llm_module"],
            "name": "M5Stack LLM Module",
            "manufacturer": "M5Stack",
            "model": "CoreS3 + LLM Kit",
        },
    }
    cfg.update(kwargs)
    return cfg


def discovery_payloads(prefix):
    state_topic = f"{prefix}/state"
    chat_topic = f"{prefix}/chat_state"
    availability_topic = f"{prefix}/availability"
    attrs_topic = state_topic
    items = []

    items.append((
        f"{DISCOVERY_PREFIX}/binary_sensor/m5stack_llm_overall_ok/config",
        sensor_config(
            "binary_sensor", "m5stack_llm_overall_ok", "M5Stack LLM Overall OK", state_topic, availability_topic,
            payload_on="ON", payload_off="OFF",
            value_template="{{ 'ON' if value_json.overall_ok else 'OFF' }}",
            json_attributes_topic=attrs_topic,
        ),
    ))
    items.append((
        f"{DISCOVERY_PREFIX}/binary_sensor/m5stack_llm_openai_api/config",
        sensor_config(
            "binary_sensor", "m5stack_llm_openai_api", "M5Stack LLM OpenAI API", state_topic, availability_topic,
            payload_on="ON", payload_off="OFF",
            value_template="{{ 'ON' if value_json.ports.openai_8000.ok else 'OFF' }}",
        ),
    ))
    items.append((
        f"{DISCOVERY_PREFIX}/binary_sensor/m5stack_llm_stackflow_api/config",
        sensor_config(
            "binary_sensor", "m5stack_llm_stackflow_api", "M5Stack LLM StackFlow API", state_topic, availability_topic,
            payload_on="ON", payload_off="OFF",
            value_template="{{ 'ON' if value_json.ports.stackflow_10001.ok else 'OFF' }}",
        ),
    ))
    items.append((
        f"{DISCOVERY_PREFIX}/binary_sensor/m5stack_llm_chat_smoke_ok/config",
        sensor_config(
            "binary_sensor", "m5stack_llm_chat_smoke_ok", "M5Stack LLM Chat Smoke OK", chat_topic, availability_topic,
            payload_on="ON", payload_off="OFF",
            value_template="{{ 'ON' if value_json.ok else 'OFF' }}",
            icon="mdi:robot-happy",
        ),
    ))

    sensors = [
        ("health_status", "M5Stack LLM Health Status", "{{ value_json.summary.status }}", None, None),
        ("problem_count", "M5Stack LLM Problem Count", "{{ value_json.summary.problem_count }}", None, None),
        ("last_update", "M5Stack LLM Last Update", "{{ value_json.published_at }}", "timestamp", None),
        ("temperature", "M5Stack LLM Temperature", "{{ value_json.system.temperature_c }}", "temperature", "°C"),
        ("stackflow_mem", "M5Stack LLM StackFlow Memory", "{{ value_json.system.mem_percent }}", None, "%"),
        ("linux_ram_available", "M5Stack LLM RAM Available", "{{ value_json.system.linux_mem.available_mb }}", "data_size", "MB"),
        ("root_disk_free", "M5Stack LLM Disk Free", "{{ value_json.system.root_disk.free_gb }}", "data_size", "GB"),
    ]
    for oid, name, tmpl, devclass, unit in sensors:
        extra = {"value_template": tmpl}
        if devclass:
            extra["device_class"] = devclass
        if unit:
            extra["unit_of_measurement"] = unit
        if oid in ("temperature", "stackflow_mem", "linux_ram_available", "root_disk_free", "problem_count"):
            extra["state_class"] = "measurement"
        if oid == "health_status":
            extra["json_attributes_topic"] = attrs_topic
            extra["json_attributes_template"] = "{{ {'problems': value_json.summary.problems} | tojson }}"
        items.append((
            f"{DISCOVERY_PREFIX}/sensor/m5stack_llm_{oid}/config",
            sensor_config("sensor", f"m5stack_llm_{oid}", name, state_topic, availability_topic, **extra),
        ))

    items.append((
        f"{DISCOVERY_PREFIX}/sensor/m5stack_llm_led_color/config",
        sensor_config(
            "sensor", "m5stack_llm_led_color", "M5Stack LLM LED Color", state_topic, availability_topic,
            value_template="{{ value_json.summary.led_color | default('unknown') }}",
            json_attributes_topic=attrs_topic,
            json_attributes_template="{{ {'rgb': [value_json.led.R, value_json.led.G, value_json.led.B]} | tojson }}",
            icon="mdi:led-on",
        ),
    ))

    items.append((
        f"{DISCOVERY_PREFIX}/sensor/m5stack_llm_chat_smoke/config",
        sensor_config(
            "sensor", "m5stack_llm_chat_smoke", "M5Stack LLM Chat Smoke", chat_topic, availability_topic,
            value_template="{{ value_json.content | default('not_run') }}",
            json_attributes_topic=chat_topic,
            icon="mdi:robot",
        ),
    ))
    return items


def load_config(path):
    if not path or not os.path.exists(path):
        return {}
    with open(path) as f:
        return json.load(f)


def main():
    ap = argparse.ArgumentParser(description="Publish M5Stack LLM healthcheck to MQTT/Home Assistant")
    ap.add_argument("--config", default="/etc/m5stack-ha-publish.json")
    ap.add_argument("--host", default=None)
    ap.add_argument("--port", type=int, default=None)
    ap.add_argument("--prefix", default=None)
    ap.add_argument("--username", default=None)
    ap.add_argument("--chat", action="store_true", help="include slower chat smoke test")
    ap.add_argument("--no-discovery", action="store_true")
    args = ap.parse_args()

    cfg = load_config(args.config)
    host = args.host or cfg.get("host") or DEFAULT_HOST
    port = args.port or int(cfg.get("port", DEFAULT_PORT))
    prefix = args.prefix or cfg.get("prefix") or DEFAULT_PREFIX
    username = args.username if args.username is not None else cfg.get("username")
    password = cfg.get("password")

    data = run_healthcheck(include_chat=args.chat)
    state_topic = f"{prefix}/state"
    chat_topic = f"{prefix}/chat_state"
    availability_topic = f"{prefix}/availability"

    if not args.no_discovery:
        for topic, payload in discovery_payloads(prefix):
            mosquitto_publish(host, port, topic, json.dumps(payload, separators=(",", ":")), retain=True, username=username, password=password)
    mosquitto_publish(host, port, availability_topic, "online", retain=True, username=username, password=password)
    mosquitto_publish(host, port, state_topic, json.dumps(data, separators=(",", ":")), retain=True, username=username, password=password)
    if args.chat:
        chat_payload = {
            "timestamp": data.get("timestamp"),
            "published_at": data.get("published_at"),
            "overall_ok": data.get("overall_ok"),
            "ok": bool(data.get("openai", {}).get("chat_smoke_content")),
            "content": data.get("openai", {}).get("chat_smoke_content", ""),
            "model_ids": data.get("openai", {}).get("model_ids", []),
        }
        mosquitto_publish(host, port, chat_topic, json.dumps(chat_payload, separators=(",", ":")), retain=True, username=username, password=password)

    print(f"published {state_topic} overall_ok={data.get('overall_ok')} host={host}:{port}")
    return 0 if data.get("overall_ok") else 2


if __name__ == "__main__":
    raise SystemExit(main())

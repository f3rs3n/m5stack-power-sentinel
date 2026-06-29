#!/usr/bin/env python3
"""Healthcheck and self-repair for Power Sentinel StackFlow routing.

The HTTP backend can be healthy while llm_sys/StackFlow routing to the CoreS3 is
stuck after a reboot. This probe checks the same native StackFlow TCP path used
by the CoreS3 and, in repair mode, restarts the minimal routing path when HTTP is
healthy but StackFlow does not return a Power Sentinel summary.

It also reports whether the CoreS3 firmware has recently issued its own
StackFlow summary request. That is a separate signal from the synthetic TCP probe:
API and StackFlow can be healthy while the display is no longer polling or no
response is being delivered to the CoreS3 UART path.
"""
from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path
from urllib.request import urlopen

DEFAULT_API_URL = "http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1"
DEFAULT_STATE_PATH = Path("/run/power-sentinel-stackflow-healthcheck.last")
DEFAULT_CORE_S3_POLL_STATE_PATH = Path("/run/power-sentinel-core-s3-poll.json")
DEFAULT_CORE_S3_MAX_AGE_SECONDS = 120


def check_http(api_url: str, timeout: float) -> tuple[bool, str]:
    try:
        with urlopen(api_url, timeout=timeout) as response:  # noqa: S310 - local appliance URL
            body = json.loads(response.read(256 * 1024).decode("utf-8"))
        if body.get("schema") != "power-sentinel.summary.v1":
            return False, "HTTP summary schema mismatch"
        return True, f"HTTP summary OK condition={body.get('condition')}"
    except Exception as exc:
        return False, f"HTTP summary failed: {type(exc).__name__}: {exc}"


def check_stackflow(host: str, port: int, timeout: float) -> tuple[bool, str]:
    payload = {
        "request_id": f"hc-{int(time.time())}",
        "work_id": "sentinel",
        "action": "summary",
    }
    raw = b""
    try:
        with socket.create_connection((host, port), timeout=timeout) as sock:
            sock.settimeout(timeout)
            sock.sendall(json.dumps(payload, separators=(",", ":")).encode("utf-8") + b"\n")
            deadline = time.monotonic() + timeout
            chunks: list[bytes] = []
            while time.monotonic() < deadline:
                try:
                    chunk = sock.recv(4096)
                except socket.timeout:
                    break
                if not chunk:
                    break
                chunks.append(chunk)
                raw = b"".join(chunks)
                if b"power-sentinel.summary.v1" in raw:
                    return True, "StackFlow summary OK"
                if len(raw) > 256 * 1024:
                    break
        preview = raw[:240].decode("utf-8", errors="replace") if raw else "<empty>"
        return False, f"StackFlow summary missing/timeout preview={preview!r}"
    except Exception as exc:
        return False, f"StackFlow probe failed: {type(exc).__name__}: {exc}"


def check_core_s3_poll(path: Path, max_age_seconds: int, now: float | None = None) -> tuple[bool, str]:
    timestamp = time.time() if now is None else now
    try:
        body = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return False, "CoreS3 poll never seen"
    except Exception as exc:
        return False, f"CoreS3 poll state unreadable: {type(exc).__name__}: {exc}"
    if body.get("schema") != "power-sentinel.core-s3-poll.v1":
        return False, "CoreS3 poll state schema mismatch"
    try:
        last_seen = float(body["last_seen_epoch"])
    except Exception:
        return False, "CoreS3 poll state missing last_seen_epoch"
    age = max(0, int(timestamp - last_seen))
    ok_count = int(body.get("ok_count") or 0)
    fail_count = int(body.get("fail_count") or 0)
    state = "fresh" if age <= max_age_seconds else "stale"
    return state == "fresh", f"CoreS3 poll {state} age={age}s ok={ok_count} fail={fail_count}"


def cooldown_allows(path: Path, cooldown_seconds: int) -> bool:
    try:
        last = float(path.read_text(encoding="utf-8").strip())
    except Exception:
        return True
    return (time.time() - last) >= cooldown_seconds


def write_cooldown(path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(str(time.time()), encoding="utf-8")


def repair(state_path: Path, cooldown_seconds: int) -> tuple[bool, str]:
    if not cooldown_allows(state_path, cooldown_seconds):
        return False, f"repair suppressed by {cooldown_seconds}s cooldown"
    write_cooldown(state_path)
    cmd = ["systemctl", "restart", "llm-sys.service", "power-sentinel-stackflow-unit.service"]
    proc = subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
    if proc.returncode != 0:
        return False, f"repair command failed rc={proc.returncode}: {proc.stdout.strip()}"
    return True, "restarted llm-sys.service and power-sentinel-stackflow-unit.service"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--api-url", default=os.environ.get("POWER_SENTINEL_API_URL", DEFAULT_API_URL))
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=10001)
    parser.add_argument("--timeout", type=float, default=5.0)
    parser.add_argument("--repair", action="store_true")
    parser.add_argument("--cooldown-seconds", type=int, default=180)
    parser.add_argument("--state-path", type=Path, default=DEFAULT_STATE_PATH)
    parser.add_argument("--core-s3-poll-state-path", type=Path, default=DEFAULT_CORE_S3_POLL_STATE_PATH)
    parser.add_argument("--core-s3-max-age-seconds", type=int, default=DEFAULT_CORE_S3_MAX_AGE_SECONDS)
    parser.add_argument("--require-core-s3-fresh", action="store_true")
    args = parser.parse_args(argv)

    http_ok, http_msg = check_http(args.api_url, args.timeout)
    print(http_msg)
    if not http_ok:
        return 2

    sf_ok, sf_msg = check_stackflow(args.host, args.port, args.timeout)
    print(sf_msg)

    core_s3_ok, core_s3_msg = check_core_s3_poll(args.core_s3_poll_state_path, args.core_s3_max_age_seconds)
    print(core_s3_msg)
    if args.require_core_s3_fresh and not core_s3_ok:
        return 4

    if sf_ok:
        return 0

    if not args.repair:
        return 1

    repaired, repair_msg = repair(args.state_path, args.cooldown_seconds)
    print(repair_msg)
    return 1 if repaired else 3


if __name__ == "__main__":
    raise SystemExit(main())

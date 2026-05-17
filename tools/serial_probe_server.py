#!/usr/bin/env python3
"""Probe LLM Module UARTs for CoreS3 internal serial traffic.

Safe defaults:
- skips /dev/ttyS0 because it is the Linux console on the M5Stack LLM Module;
- does not modify systemd, getty, or pin configuration;
- listens for newline-delimited text and replies to PS1 PING/GET probes.

Usage on LLM Module:
  python3 tools/serial_probe_server.py --port /dev/ttyS1
  python3 tools/serial_probe_server.py --scan
"""
from __future__ import annotations

import argparse
import json
import os
import select
import sys
import time
from pathlib import Path

try:
    import serial  # type: ignore
except ImportError:  # pragma: no cover - runtime guidance
    print("pyserial is required: apt-get install -y python3-serial or pip install pyserial", file=sys.stderr)
    raise

DEFAULT_PORTS = [f"/dev/ttyS{i}" for i in range(1, 6)]

SAMPLE_SUMMARY = {
    "schema": "power-sentinel.summary.v1",
    "transport": "uart-probe",
    "severity": "warn",
    "ups": {"available": False, "status": "MOCK", "battery_percent": -1},
    "problems": ["UART probe mode; not real power data"],
}


def open_port(path: str, baud: int) -> serial.Serial:
    return serial.Serial(path, baudrate=baud, timeout=0, write_timeout=1)


def handle_line(ser: serial.Serial, port: str, line: bytes) -> None:
    text = line.decode("utf-8", "replace").strip()
    if not text:
        return
    print(f"{port} RX {text}", flush=True)
    if text.startswith("PS1 PING"):
        token = text.removeprefix("PS1 PING").strip()
        reply = f"PS1 PONG {token}\n".encode()
    elif text == "PS1 GET summary":
        payload = json.dumps(SAMPLE_SUMMARY, separators=(",", ":")).encode()
        reply = f"PS1 OK {len(payload)}\n".encode() + payload + b"\n"
    else:
        reply = f"PS1 ECHO {text}\n".encode()
    ser.write(reply)
    print(f"{port} TX {reply!r}", flush=True)


def serve_one(port: str, baud: int) -> None:
    if port == "/dev/ttyS0":
        raise SystemExit("Refusing to probe /dev/ttyS0 because it is the Linux console")
    with open_port(port, baud) as ser:
        print(f"Listening on {port} @ {baud}. Ctrl-C to stop.", flush=True)
        buf = b""
        while True:
            r, _, _ = select.select([ser.fileno()], [], [], 0.5)
            if not r:
                continue
            chunk = ser.read(4096)
            if not chunk:
                continue
            buf += chunk
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                handle_line(ser, port, line)


def scan(ports: list[str], baud: int, seconds: int) -> None:
    opened: list[tuple[str, serial.Serial, bytes]] = []
    for port in ports:
        if port == "/dev/ttyS0" or not Path(port).exists():
            continue
        try:
            ser = open_port(port, baud)
        except Exception as exc:
            print(f"{port}: open failed: {exc}", flush=True)
            continue
        opened.append((port, ser, b""))
        print(f"{port}: listening", flush=True)

    deadline = time.time() + seconds
    try:
        while time.time() < deadline:
            if not opened:
                time.sleep(0.5)
                continue
            r, _, _ = select.select([ser.fileno() for _, ser, _ in opened], [], [], 0.5)
            new_opened = []
            for port, ser, buf in opened:
                if ser.fileno() in r:
                    chunk = ser.read(4096)
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        handle_line(ser, port, line)
                new_opened.append((port, ser, buf))
            opened = new_opened
    finally:
        for _, ser, _ in opened:
            ser.close()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/ttyS1")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--scan", action="store_true", help="listen on /dev/ttyS1..ttyS5 concurrently")
    parser.add_argument("--seconds", type=int, default=60)
    args = parser.parse_args()

    if args.scan:
        scan(DEFAULT_PORTS, args.baud, args.seconds)
    else:
        serve_one(args.port, args.baud)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

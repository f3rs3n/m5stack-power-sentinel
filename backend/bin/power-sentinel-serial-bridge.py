#!/usr/bin/env python3
"""Power Sentinel CoreS3 serial bridge for the M5Stack LLM Module.

Protocol v1 is line-oriented for requests and length-prefixed for JSON replies:

  CoreS3 -> LLM Module: PS1 PING <token>\n
  LLM Module -> CoreS3: PS1 PONG <token>\n
  CoreS3 -> LLM Module: PS1 GET summary\n
  LLM Module -> CoreS3: PS1 OK <json-bytes>\n<json>\n
This script intentionally uses only Python stdlib. The stock LLM Module image may
not have pyserial installed, so termios/fcntl/os are used directly.
"""
from __future__ import annotations

import argparse
import json
import os
import select
import sys
import termios
import time
import urllib.error
import urllib.request
from collections.abc import Callable
from typing import NamedTuple
from typing import Any

DEFAULT_PORT = "/dev/ttyS1"
DEFAULT_BAUD = 115200
DEFAULT_SUMMARY_URL = "http://127.0.0.1:8088/api/v1/summary"
MAX_LINE_BYTES = 512

BAUD_RATES = {
    9600: termios.B9600,
    19200: termios.B19200,
    38400: termios.B38400,
    57600: termios.B57600,
    115200: termios.B115200,
    230400: getattr(termios, "B230400", termios.B115200),
    460800: getattr(termios, "B460800", termios.B115200),
    921600: getattr(termios, "B921600", termios.B115200),
}


class Reply(NamedTuple):
    body: bytes
    log_label: str


class SerialPort:
    def __init__(self, path: str, baud: int) -> None:
        if path == "/dev/ttyS0":
            raise ValueError("Refusing to use /dev/ttyS0; it is the LLM Module Linux console")
        if baud not in BAUD_RATES:
            raise ValueError(f"Unsupported baud rate: {baud}")
        self.path = path
        self.baud = baud
        self.fd: int | None = None

    def __enter__(self) -> "SerialPort":
        self.fd = os.open(self.path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        self._configure()
        return self

    def __exit__(self, exc_type: object, exc: object, tb: object) -> None:
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None

    def fileno(self) -> int:
        if self.fd is None:
            raise RuntimeError("serial port is not open")
        return self.fd

    def read(self, size: int = 4096) -> bytes:
        try:
            return os.read(self.fileno(), size)
        except BlockingIOError:
            return b""

    def write(self, data: bytes) -> None:
        view = memoryview(data)
        while view:
            try:
                written = os.write(self.fileno(), view)
            except BlockingIOError:
                select.select([], [self.fileno()], [], 1.0)
                continue
            view = view[written:]
        termios.tcdrain(self.fileno())

    def _configure(self) -> None:
        fd = self.fileno()
        attrs = termios.tcgetattr(fd)
        baud_const = BAUD_RATES[self.baud]

        # iflag, oflag, cflag, lflag, ispeed, ospeed, cc
        attrs[0] = 0
        attrs[1] = 0
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[3] = 0
        attrs[4] = baud_const
        attrs[5] = baud_const
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attrs)
        termios.tcflush(fd, termios.TCIOFLUSH)


def compact_json(payload: Any) -> bytes:
    return json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")


def error_payload(code: str, message: str) -> bytes:
    payload = {
        "schema": "power-sentinel.serial-error.v1",
        "ok": False,
        "error": code,
        "message": message,
        "timestamp_unix": int(time.time()),
    }
    return compact_json(payload)


def fetch_summary(url: str, timeout: float) -> bytes:
    req = urllib.request.Request(url, headers={"Accept": "application/json"})
    with urllib.request.urlopen(req, timeout=timeout) as response:
        data = response.read()
    # Validate and normalize compact JSON before sending over the slow UART.
    parsed = json.loads(data.decode("utf-8"))
    return compact_json(parsed)


def ok_reply(payload: bytes) -> Reply:
    return Reply(body=f"PS1 OK {len(payload)}\n".encode("ascii") + payload + b"\n", log_label=f"OK {len(payload)}")


def err_reply(code: str, message: str) -> Reply:
    payload = error_payload(code, message)
    return Reply(body=f"PS1 ERR {code} {len(payload)}\n".encode("ascii") + payload + b"\n", log_label=f"ERR {code} {len(payload)}")


def normalize_request_line(line: str) -> str:
    """Return the PS1 command inside a possibly noisy UART line.

    The CoreS3/LLM stacked UART can occasionally deliver a leading junk byte
    before an otherwise valid command after resets or bus glitches, e.g.
    ``�PS1 GET summary``. Treat that as recoverable instead of replying with
    unknown_command, while still ignoring arbitrary non-PS1 noise.
    """
    text = line.strip()
    marker = text.find("PS1 ")
    if marker > 0:
        text = text[marker:]
    return text


def handle_request(line: str, summary_fetcher: Callable[[], bytes]) -> Reply | None:
    text = normalize_request_line(line)
    if not text:
        return None
    if text.startswith("PS1 PING"):
        token = text.removeprefix("PS1 PING").strip()
        suffix = f" {token}" if token else ""
        return Reply(body=f"PS1 PONG{suffix}\n".encode("utf-8"), log_label="PONG")
    if text == "PS1 GET summary":
        try:
            return ok_reply(summary_fetcher())
        except urllib.error.URLError as exc:
            return err_reply("summary_fetch_failed", str(exc))
        except (json.JSONDecodeError, UnicodeDecodeError) as exc:
            return err_reply("summary_invalid_json", str(exc))
        except Exception as exc:  # keep bridge alive; report fault to CoreS3
            return err_reply("summary_exception", f"{type(exc).__name__}: {exc}")
    if text == "PS1 GET health":
        payload = compact_json({"schema": "power-sentinel.serial-health.v1", "ok": True, "timestamp_unix": int(time.time())})
        return ok_reply(payload)
    return Reply(body=f"PS1 ERR unknown_command 0\n".encode("ascii"), log_label="ERR unknown_command")


def serve(port: str, baud: int, summary_url: str, timeout: float) -> None:
    fetcher = lambda: fetch_summary(summary_url, timeout)
    with SerialPort(port, baud) as ser:
        print(f"power-sentinel-serial-bridge listening on {port} @ {baud}; summary={summary_url}", flush=True)
        buf = b""
        while True:
            readable, _, _ = select.select([ser.fileno()], [], [], 0.5)
            if not readable:
                continue
            chunk = ser.read(4096)
            if not chunk:
                continue
            buf += chunk
            if len(buf) > MAX_LINE_BYTES and b"\n" not in buf:
                print(f"RX line too long; dropping {len(buf)} buffered bytes", file=sys.stderr, flush=True)
                buf = b""
                ser.write(b"PS1 ERR line_too_long 0\n")
                continue
            while b"\n" in buf:
                raw, buf = buf.split(b"\n", 1)
                line = raw.decode("utf-8", "replace").strip()
                if not line:
                    continue
                print(f"RX {line}", flush=True)
                reply = handle_request(line, fetcher)
                if reply is None:
                    continue
                ser.write(reply.body)
                print(f"TX {reply.log_label}", flush=True)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Power Sentinel CoreS3 serial bridge")
    parser.add_argument("--port", default=os.environ.get("POWER_SENTINEL_SERIAL_PORT", DEFAULT_PORT))
    parser.add_argument("--baud", type=int, default=int(os.environ.get("POWER_SENTINEL_SERIAL_BAUD", DEFAULT_BAUD)))
    parser.add_argument("--summary-url", default=os.environ.get("POWER_SENTINEL_SUMMARY_URL", DEFAULT_SUMMARY_URL))
    parser.add_argument("--timeout", type=float, default=float(os.environ.get("POWER_SENTINEL_SUMMARY_TIMEOUT", "3.0")))
    parser.add_argument("--test-fetch", action="store_true", help="fetch and print one compact summary payload, then exit")
    args = parser.parse_args(argv)

    if args.test_fetch:
        payload = fetch_summary(args.summary_url, args.timeout)
        print(payload.decode("utf-8"))
        return 0

    serve(args.port, args.baud, args.summary_url, args.timeout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

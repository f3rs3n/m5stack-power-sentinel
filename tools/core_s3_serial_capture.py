#!/usr/bin/env python3
"""Bounded CoreS3 serial capture helper for non-interactive live tests.

`pio device monitor` needs a TTY and fails inside Hermes tool sessions. This
script uses pyserial directly, does not reset the CoreS3 by default, and exits
cleanly after a time, line, or idle limit.
"""

from __future__ import annotations

import argparse
import pathlib
import sys
import time
from typing import Callable, TextIO


DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUD = 115200
DEFAULT_DURATION_SECONDS = 15.0
DEFAULT_IDLE_TIMEOUT_SECONDS = 5.0
DEFAULT_READ_TIMEOUT_SECONDS = 0.25


class CaptureResult:
    def __init__(self, *, lines: int, timed_out: bool, idle_timed_out: bool):
        self.lines = lines
        self.timed_out = timed_out
        self.idle_timed_out = idle_timed_out


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Capture CoreS3 serial logs without requiring an interactive TTY."
    )
    parser.add_argument(
        "--port",
        default=DEFAULT_PORT,
        help=f"serial device path (default: {DEFAULT_PORT})",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=DEFAULT_BAUD,
        help=f"serial baud rate (default: {DEFAULT_BAUD})",
    )
    parser.add_argument(
        "--duration",
        type=float,
        default=DEFAULT_DURATION_SECONDS,
        help=f"maximum capture duration in seconds; 0 disables the time limit (default: {DEFAULT_DURATION_SECONDS:g})",
    )
    parser.add_argument(
        "--idle-timeout",
        type=float,
        default=DEFAULT_IDLE_TIMEOUT_SECONDS,
        help=f"stop after this many seconds without bytes; 0 disables idle timeout (default: {DEFAULT_IDLE_TIMEOUT_SECONDS:g})",
    )
    parser.add_argument(
        "--max-lines",
        type=int,
        default=0,
        help="stop after this many decoded lines; 0 disables the line limit",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        help="optional file path to write the captured serial lines",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="write only to --output; suppress captured serial lines on stdout",
    )
    parser.add_argument(
        "--read-timeout",
        type=float,
        default=DEFAULT_READ_TIMEOUT_SECONDS,
        help=f"pyserial read timeout in seconds (default: {DEFAULT_READ_TIMEOUT_SECONDS:g})",
    )
    return parser.parse_args(argv)


def decode_line(raw: bytes) -> str:
    return raw.decode("utf-8", errors="replace").rstrip("\r\n")


def open_serial(port: str, baud: int, read_timeout: float):
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise SystemExit(
            "pyserial is required. Run with the PlatformIO venv Python, e.g. "
            "/home/martino/.platformio/penv/bin/python tools/core_s3_serial_capture.py"
        ) from exc
    return serial.Serial(port=port, baudrate=baud, timeout=read_timeout)


def write_line(line: str, streams: list[TextIO]) -> None:
    for stream in streams:
        print(line, file=stream, flush=True)


def capture_serial_stream(
    serial_obj,
    stdout: TextIO,
    *,
    output_file: TextIO | None = None,
    quiet: bool = False,
    max_lines: int = 0,
    duration: float = DEFAULT_DURATION_SECONDS,
    idle_timeout: float = DEFAULT_IDLE_TIMEOUT_SECONDS,
    monotonic: Callable[[], float] = time.monotonic,
    sleep: Callable[[float], None] = time.sleep,
) -> CaptureResult:
    streams: list[TextIO] = [] if quiet else [stdout]
    if output_file is not None:
        streams.append(output_file)

    start = monotonic()
    last_data = start
    lines = 0
    timed_out = False
    idle_timed_out = False

    while True:
        now = monotonic()
        if duration > 0 and now - start >= duration:
            timed_out = True
            break
        if max_lines > 0 and lines >= max_lines:
            break

        raw = serial_obj.readline()
        if raw:
            write_line(decode_line(raw), streams)
            lines += 1
            last_data = monotonic()
            continue

        if idle_timeout > 0 and monotonic() - last_data >= idle_timeout:
            idle_timed_out = True
            break
        sleep(min(DEFAULT_READ_TIMEOUT_SECONDS, 0.05))

    return CaptureResult(lines=lines, timed_out=timed_out, idle_timed_out=idle_timed_out)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    if args.quiet and args.output is None:
        print("--quiet requires --output", file=sys.stderr)
        return 2

    output_handle: TextIO | None = None
    try:
        if args.output is not None:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            output_handle = args.output.open("w", encoding="utf-8")
        with open_serial(args.port, args.baud, args.read_timeout) as serial_obj:
            result = capture_serial_stream(
                serial_obj,
                sys.stdout,
                output_file=output_handle,
                quiet=args.quiet,
                max_lines=args.max_lines,
                duration=args.duration,
                idle_timeout=args.idle_timeout,
            )
    except OSError as exc:
        print(f"failed to capture {args.port}: {exc}", file=sys.stderr)
        return 1
    finally:
        if output_handle is not None:
            output_handle.close()

    reason = "line limit"
    if result.timed_out:
        reason = "duration"
    elif result.idle_timed_out:
        reason = "idle timeout"
    print(f"capture complete: lines={result.lines} reason={reason}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

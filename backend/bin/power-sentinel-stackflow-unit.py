#!/usr/bin/env python3
"""Power Sentinel StackFlow custom unit.

This daemon lets the M5Stack CoreS3 talk to Power Sentinel through the vendor
`llm_sys` router instead of competing for `/dev/ttyS1`.

StackFlow routing discovered from upstream `m5stack/StackFlow`:
- Core/TCP sends JSON with work_id prefix, e.g. `sentinel`.
- `llm_sys` routes non-sys work_ids to ZeroMQ RPC socket `ipc:///tmp/rpc.<unit>`.
- The RPC payload is a tiny two-param frame: [len(param0)][param0][param1]
  where param0 is the callback PUSH URL (`ipc:///tmp/llm/<com_id>.sock`) and
  param1 is the original JSON request.
- The custom unit must PUSH the real response JSON to param0. The RPC return
  itself is only used to unblock the REQ/REP handshake.
"""

from __future__ import annotations

import argparse
import json
import os
import signal
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from urllib.request import urlopen

try:
    import zmq  # type: ignore[import-not-found]
except ModuleNotFoundError:  # pragma: no cover - exercised on target if dep missing
    zmq = None  # type: ignore[assignment]

DEFAULT_UNIT = "sentinel"
DEFAULT_API_URL = "http://127.0.0.1:8088/api/v1/summary?stackflow_safe=1"
DEFAULT_BIND_DIR = "/tmp"
DEFAULT_TIMEOUT = 4.0
DEFAULT_CORE_S3_POLL_STATE_PATH = Path("/run/power-sentinel-core-s3-poll.json")


@dataclass(frozen=True)
class StackFlowRequest:
    callback_url: str
    raw_json: str
    action: str
    request_id: str
    work_id: str
    object_name: str
    data: Any


def parse_stackflow_param_frame(frame: bytes) -> tuple[str, str]:
    """Parse StackFlow pzmq_data::set_param(param0, param1)."""
    if not frame:
        raise ValueError("empty StackFlow param frame")
    param0_len = frame[0]
    if len(frame) < 1 + param0_len:
        raise ValueError("truncated StackFlow param frame")
    param0 = frame[1 : 1 + param0_len].decode("utf-8", errors="replace")
    param1 = frame[1 + param0_len :].decode("utf-8", errors="replace")
    return param0, param1


def parse_request(action_frame: bytes, payload_frame: bytes) -> StackFlowRequest:
    action = action_frame.decode("utf-8", errors="replace")
    callback_url, raw_json = parse_stackflow_param_frame(payload_frame)
    try:
        body = json.loads(raw_json)
    except json.JSONDecodeError as exc:
        raise ValueError(f"invalid JSON request: {exc}") from exc
    return StackFlowRequest(
        callback_url=callback_url,
        raw_json=raw_json,
        action=action or str(body.get("action") or ""),
        request_id=str(body.get("request_id") or "0"),
        work_id=str(body.get("work_id") or DEFAULT_UNIT),
        object_name=str(body.get("object") or "None"),
        data=body.get("data"),
    )


def stackflow_response(
    request: StackFlowRequest | None,
    *,
    object_name: str = "None",
    data: Any = "None",
    code: int = 0,
    message: str = "",
    work_id: str | None = None,
) -> bytes:
    now = int(time.time())
    payload = {
        "request_id": request.request_id if request else "0",
        "work_id": work_id or (request.work_id if request else DEFAULT_UNIT),
        "created": now,
        "object": object_name,
        "data": data,
        "error": {"code": code, "message": message},
    }
    return (json.dumps(payload, separators=(",", ":")) + "\n").encode("utf-8")


def fetch_summary(api_url: str, timeout: float = DEFAULT_TIMEOUT) -> dict[str, Any]:
    with urlopen(api_url, timeout=timeout) as response:  # noqa: S310 - local trusted appliance URL
        raw = response.read(256 * 1024)
    body = json.loads(raw.decode("utf-8"))
    if not isinstance(body, dict):
        raise ValueError("summary API returned non-object JSON")
    return body


def handle_request(request: StackFlowRequest, api_url: str, timeout: float = DEFAULT_TIMEOUT) -> bytes:
    action = request.action.lower()
    if action == "ping":
        return stackflow_response(request, object_name="power-sentinel.pong.v1", data={"ok": True})
    if action in {"summary", "health"}:
        try:
            summary = fetch_summary(api_url, timeout=timeout)
        except Exception as exc:  # keep unit responsive even if API/NUT is down
            return stackflow_response(
                request,
                object_name="power-sentinel.error.v1",
                data={"ok": False, "error": type(exc).__name__, "message": str(exc)},
                code=-9,
                message="summary fetch failed",
            )
        return stackflow_response(request, object_name="power-sentinel.summary.v1", data=summary)
    return stackflow_response(
        request,
        object_name="power-sentinel.error.v1",
        data={"ok": False, "error": "unknown_action", "action": request.action},
        code=-7,
        message="Unknown Operation",
    )


def is_core_s3_poll_request(request: StackFlowRequest) -> bool:
    return request.action.lower() in {"summary", "health"} and request.request_id.startswith("ps-nut-")


def _read_core_s3_poll_state(path: Path) -> dict[str, Any]:
    try:
        body = json.loads(path.read_text(encoding="utf-8"))
    except Exception:
        return {}
    return body if isinstance(body, dict) else {}


def record_core_s3_poll(path: Path, request: StackFlowRequest, *, ok: bool, message: str, now: float | None = None) -> None:
    timestamp = time.time() if now is None else now
    previous = _read_core_s3_poll_state(path)
    ok_count = int(previous.get("ok_count") or 0)
    fail_count = int(previous.get("fail_count") or 0)
    state: dict[str, Any] = {
        "schema": "power-sentinel.core-s3-poll.v1",
        "last_request_id": request.request_id,
        "last_seen_epoch": timestamp,
        "last_message": message,
        "ok_count": ok_count + (1 if ok else 0),
        "fail_count": fail_count + (0 if ok else 1),
    }
    if previous.get("last_success_epoch") is not None:
        state["last_success_epoch"] = previous["last_success_epoch"]
    if previous.get("last_failure_epoch") is not None:
        state["last_failure_epoch"] = previous["last_failure_epoch"]
    if previous.get("last_error") is not None:
        state["last_error"] = previous["last_error"]
    if ok:
        state["last_success_epoch"] = timestamp
        state.pop("last_error", None)
    else:
        state["last_failure_epoch"] = timestamp
        state["last_error"] = message
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(state, separators=(",", ":")) + "\n", encoding="utf-8")


def ipc_path_from_url(url: str) -> str | None:
    prefix = "ipc://"
    if not url.startswith(prefix):
        return None
    return url[len(prefix) :]


def remove_stale_ipc_socket(url: str) -> None:
    path = ipc_path_from_url(url)
    if path and os.path.exists(path):
        os.remove(path)


def push_response(callback_url: str, payload: bytes, send_timeout_ms: int = 1500) -> None:
    if zmq is None:
        raise RuntimeError("python3-zmq is not installed")
    ctx = zmq.Context.instance()
    sock = ctx.socket(zmq.PUSH)
    try:
        sock.setsockopt(zmq.SNDTIMEO, send_timeout_ms)
        # Do not use LINGER=0 here. For ipc PUSH->PULL callbacks the connect
        # handshake can still be settling when send() returns; closing with
        # zero linger can silently drop the queued response. This mirrors the
        # upstream pzmq helper, which allows a short linger on close.
        sock.setsockopt(zmq.LINGER, send_timeout_ms)
        sock.connect(callback_url)
        sock.send(payload)
    finally:
        sock.close(send_timeout_ms)


def serve(unit: str, api_url: str, bind_dir: str, timeout: float, core_s3_poll_state_path: Path) -> int:
    if zmq is None:
        print("ERROR: python3-zmq is required. Install with: apt install python3-zmq", file=sys.stderr)
        return 2
    bind_url = f"ipc://{bind_dir.rstrip('/')}/rpc.{unit}"
    remove_stale_ipc_socket(bind_url)

    ctx = zmq.Context.instance()
    sock = ctx.socket(zmq.REP)
    sock.setsockopt(zmq.LINGER, 0)
    sock.bind(bind_url)
    print(f"power-sentinel StackFlow unit listening on {bind_url}; API={api_url}", flush=True)

    stopping = False

    def _stop(_signum: int, _frame: object) -> None:
        nonlocal stopping
        stopping = True

    signal.signal(signal.SIGTERM, _stop)
    signal.signal(signal.SIGINT, _stop)

    poller = zmq.Poller()
    poller.register(sock, zmq.POLLIN)
    while not stopping:
        events = dict(poller.poll(500))
        if sock not in events:
            continue
        request: StackFlowRequest | None = None
        try:
            parts = sock.recv_multipart()
            if len(parts) != 2:
                raise ValueError(f"expected 2 RPC frames, got {len(parts)}")
            request = parse_request(parts[0], parts[1])
            response = handle_request(request, api_url, timeout=timeout)
            push_response(request.callback_url, response)
            if is_core_s3_poll_request(request):
                record_core_s3_poll(core_s3_poll_state_path, request, ok=True, message="response pushed")
            sock.send(b"OK")
        except Exception as exc:
            if request and is_core_s3_poll_request(request):
                record_core_s3_poll(core_s3_poll_state_path, request, ok=False, message=f"{type(exc).__name__}: {exc}")
            # REP must always reply or llm_sys' REQ client can block until timeout.
            try:
                if request and request.callback_url:
                    payload = stackflow_response(
                        request,
                        object_name="power-sentinel.error.v1",
                        data={"ok": False, "error": type(exc).__name__, "message": str(exc)},
                        code=-9,
                        message="unit call failed",
                    )
                    push_response(request.callback_url, payload)
                sock.send(b"ERR")
            except Exception:
                pass
            print(f"request failed: {type(exc).__name__}: {exc}", file=sys.stderr, flush=True)

    sock.close(0)
    ctx.term()
    remove_stale_ipc_socket(bind_url)
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Power Sentinel custom StackFlow unit")
    parser.add_argument("--unit", default=os.environ.get("POWER_SENTINEL_STACKFLOW_UNIT", DEFAULT_UNIT))
    parser.add_argument("--api-url", default=os.environ.get("POWER_SENTINEL_API_URL", DEFAULT_API_URL))
    parser.add_argument("--bind-dir", default=os.environ.get("POWER_SENTINEL_STACKFLOW_BIND_DIR", DEFAULT_BIND_DIR))
    parser.add_argument("--timeout", type=float, default=float(os.environ.get("POWER_SENTINEL_API_TIMEOUT", DEFAULT_TIMEOUT)))
    parser.add_argument("--core-s3-poll-state-path", type=Path, default=DEFAULT_CORE_S3_POLL_STATE_PATH)
    args = parser.parse_args(argv)
    return serve(args.unit, args.api_url, args.bind_dir, args.timeout, args.core_s3_poll_state_path)


if __name__ == "__main__":
    raise SystemExit(main())

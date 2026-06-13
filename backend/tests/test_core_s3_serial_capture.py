import importlib.util
import io
import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[2]


def load_module():
    path = ROOT / "tools" / "core_s3_serial_capture.py"
    spec = importlib.util.spec_from_file_location("core_s3_serial_capture", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class FakeSerial:
    def __init__(self, chunks):
        self.chunks = list(chunks)

    def readline(self):
        if self.chunks:
            return self.chunks.pop(0)
        return b""


def test_parse_args_uses_cores3_live_capture_defaults():
    capture = load_module()

    args = capture.parse_args([])

    assert args.port == "/dev/ttyACM0"
    assert args.baud == 115200
    assert args.duration == 15.0
    assert args.idle_timeout == 5.0
    assert args.max_lines == 0
    assert args.output is None
    assert not args.quiet


def test_capture_serial_stream_decodes_lines_and_stops_at_max_lines():
    capture = load_module()
    output = io.StringIO()
    serial_obj = FakeSerial([
        b"Power Sentinel nut-monitor-clean-baseline-2026-06-08 clean NUT monitor baseline\r\n",
        b"Display policy: standby=300000 no_payload_off=900000 awake=160 dim=48\n",
        b"this third line should not be read\n",
    ])

    result = capture.capture_serial_stream(serial_obj, output, max_lines=2, duration=60, idle_timeout=60)

    assert result.lines == 2
    assert result.timed_out is False
    assert result.idle_timed_out is False
    assert "clean NUT monitor baseline" in output.getvalue()
    assert "Display policy: standby=300000" in output.getvalue()
    assert "third line" not in output.getvalue()


def test_capture_serial_stream_reports_idle_timeout_without_tty():
    capture = load_module()
    output = io.StringIO()
    serial_obj = FakeSerial([b""])
    times = iter([0.0, 0.0, 6.0])

    result = capture.capture_serial_stream(
        serial_obj,
        output,
        max_lines=0,
        duration=60,
        idle_timeout=5,
        monotonic=lambda: next(times),
        sleep=lambda _seconds: None,
    )

    assert result.lines == 0
    assert result.timed_out is False
    assert result.idle_timed_out is True
    assert output.getvalue() == ""


def test_decode_line_replaces_invalid_serial_bytes():
    capture = load_module()

    assert capture.decode_line(b"ALS sensor: ready adaptive=1\xff\n") == "ALS sensor: ready adaptive=1�"

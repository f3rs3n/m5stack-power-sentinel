import importlib.util
import pathlib
import sys
import tempfile
from contextlib import redirect_stdout
from io import StringIO


ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "bin" / "power-sentinel-stackflow-healthcheck.py"


def load_module():
    spec = importlib.util.spec_from_file_location("power_sentinel_stackflow_healthcheck", MODULE_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def test_core_s3_poll_status_reports_missing_state():
    healthcheck = load_module()
    with tempfile.TemporaryDirectory() as tmp:
        ok, msg = healthcheck.check_core_s3_poll(pathlib.Path(tmp) / "missing.json", max_age_seconds=120, now=1000.0)

        assert ok is False
        assert msg == "CoreS3 poll never seen"


def test_core_s3_poll_status_reports_fresh_and_stale_state():
    healthcheck = load_module()
    with tempfile.TemporaryDirectory() as tmp:
        path = pathlib.Path(tmp) / "core-s3-poll.json"
        path.write_text(
            '{"schema":"power-sentinel.core-s3-poll.v1","last_seen_epoch":900.0,"last_success_epoch":895.0,"ok_count":7,"fail_count":1}',
            encoding="utf-8",
        )

        fresh_ok, fresh_msg = healthcheck.check_core_s3_poll(path, max_age_seconds=120, now=1000.0)
        stale_ok, stale_msg = healthcheck.check_core_s3_poll(path, max_age_seconds=60, now=1000.0)

        assert fresh_ok is True
        assert fresh_msg == "CoreS3 poll fresh age=100s ok=7 fail=1"
        assert stale_ok is False
        assert stale_msg == "CoreS3 poll stale age=100s ok=7 fail=1"


def test_main_repairs_core_s3_stale_even_when_synthetic_stackflow_is_ok():
    healthcheck = load_module()
    calls = []
    original_check_http = healthcheck.check_http
    original_check_stackflow = healthcheck.check_stackflow
    original_check_core_s3_poll = healthcheck.check_core_s3_poll
    original_repair = healthcheck.repair

    def fake_repair(state_path, cooldown_seconds):
        calls.append((state_path, cooldown_seconds))
        return True, "restarted llm-sys.service and power-sentinel-stackflow-unit.service"

    try:
        healthcheck.check_http = lambda api_url, timeout: (True, "HTTP summary OK condition=healthy")
        healthcheck.check_stackflow = lambda host, port, timeout: (True, "StackFlow summary OK")
        healthcheck.check_core_s3_poll = lambda path, max_age_seconds: (False, "CoreS3 poll stale age=999s ok=12 fail=0")
        healthcheck.repair = fake_repair

        output = StringIO()
        with redirect_stdout(output):
            rc = healthcheck.main(["--repair", "--state-path", "/tmp/test-healthcheck.last", "--cooldown-seconds", "42"])

        assert rc == 1
        assert calls == [(pathlib.Path("/tmp/test-healthcheck.last"), 42)]
        assert "CoreS3 poll stale while synthetic StackFlow is healthy" in output.getvalue()
    finally:
        healthcheck.check_http = original_check_http
        healthcheck.check_stackflow = original_check_stackflow
        healthcheck.check_core_s3_poll = original_check_core_s3_poll
        healthcheck.repair = original_repair


def test_main_does_not_repair_core_s3_stale_without_repair_flag():
    healthcheck = load_module()
    original_check_http = healthcheck.check_http
    original_check_stackflow = healthcheck.check_stackflow
    original_check_core_s3_poll = healthcheck.check_core_s3_poll
    original_repair = healthcheck.repair

    def forbidden_repair(state_path, cooldown_seconds):
        raise AssertionError("repair should not run")

    try:
        healthcheck.check_http = lambda api_url, timeout: (True, "HTTP summary OK condition=healthy")
        healthcheck.check_stackflow = lambda host, port, timeout: (True, "StackFlow summary OK")
        healthcheck.check_core_s3_poll = lambda path, max_age_seconds: (False, "CoreS3 poll stale age=999s ok=12 fail=0")
        healthcheck.repair = forbidden_repair

        assert healthcheck.main([]) == 0
        assert healthcheck.main(["--require-core-s3-fresh"]) == 4
    finally:
        healthcheck.check_http = original_check_http
        healthcheck.check_stackflow = original_check_stackflow
        healthcheck.check_core_s3_poll = original_check_core_s3_poll
        healthcheck.repair = original_repair

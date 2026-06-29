import importlib.util
import pathlib
import sys
import tempfile


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

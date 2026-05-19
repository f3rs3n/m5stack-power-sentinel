import importlib.util
import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[2]


def load_module():
    path = ROOT / "backend" / "bin" / "m5stack-ups-detect.py"
    spec = importlib.util.spec_from_file_location("m5stack_ups_detect", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_redact_nut_config_hides_password_directives_and_monitor_secret():
    detect = load_module()
    raw = """[upsmon_primary]
    password = primary-secret
    upsmon master
MONITOR homelab_ups@localhost 1 upsmon_primary monitor-secret master
MONITOR homelab_ups@192.168.2.202 1 upsmon_secondary other-secret slave
"""

    redacted = detect.redact_nut_config(raw)

    assert "primary-secret" not in redacted
    assert "monitor-secret" not in redacted
    assert "other-secret" not in redacted
    assert "password = [REDACTED]" in redacted
    assert "MONITOR homelab_ups@localhost 1 upsmon_primary [REDACTED] master" in redacted
    assert "MONITOR homelab_ups@192.168.2.202 1 upsmon_secondary [REDACTED] slave" in redacted

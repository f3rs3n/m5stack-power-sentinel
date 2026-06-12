import importlib.util
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[2]
API_PATH = ROOT / "backend" / "bin" / "power-sentinel-api.py"
spec = importlib.util.spec_from_file_location("power_sentinel_api", API_PATH)
assert spec is not None and spec.loader is not None
api = importlib.util.module_from_spec(spec)
spec.loader.exec_module(api)


def with_fake_nut(func):
    old_run = api.run_text_command
    old_systemd = api.systemd_active
    try:
        api.run_text_command = lambda *args, **kwargs: (1, "", "no ups")
        api.systemd_active = lambda unit: None
        return func()
    finally:
        api.run_text_command = old_run
        api.systemd_active = old_systemd


def test_parse_upsc_derives_clean_nut_monitor_fields():
    parsed = api.parse_upsc_output(
        """
ups.mfr: APC
ups.model: Back-UPS ES 850G2
ups.status: OL CHRG
battery.charge: 97
battery.runtime: 2310
ups.load: 18
ups.realpower.nominal: 520
input.voltage: 232.1
battery.voltage: 13.5
ups.beeper.status: enabled
driver.name: usbhid-ups
""".strip()
    )
    assert parsed["available"] is True
    assert parsed["status_label"] == "Online"
    assert parsed["charging"] is True
    assert parsed["battery_percent"] == 97
    assert parsed["runtime_seconds"] == 2310
    assert parsed["load_w"] == 94
    assert parsed["would_shutdown"] is False
    assert parsed["model"] == "APC Back-UPS ES 850G2"


def test_shutdown_rule_requires_on_battery_low_battery_not_online_low_battery():
    online_low = api.parse_upsc_output("ups.status: OL CHRG LB\nbattery.charge: 8")
    battery_low = api.parse_upsc_output("ups.status: OB LB\nbattery.charge: 8")
    assert online_low["low_battery"] is True
    assert online_low["on_battery"] is False
    assert online_low["would_shutdown"] is False
    assert battery_low["would_shutdown"] is True


def test_summary_defaults_to_nut_only_and_declares_future_modules():
    def check():
        summary = api.build_summary({})
        assert summary["schema"] == api.SCHEMA_SUMMARY
        assert summary["profile"] == "nut-monitor-clean-baseline"
        assert summary["enabled_modules"] == ["nut"]
        assert summary["available_modules"] == ["nut", "proxmox", "ha"]
        assert summary["pages"] == ["NUT"]
        assert summary["modules"]["proxmox"]["implemented"] is False
        assert summary["ups"]["available"] is False
    with_fake_nut(check)


def test_summary_includes_module_lan_status_and_local_time():
    old_run = api.run_text_command
    old_systemd = api.systemd_active
    try:
        def fake_run(cmd, *args, **kwargs):
            if cmd[:5] == ["ip", "-4", "-o", "addr", "show"]:
                return (0, "2: eth0    inet 192.168.2.123/24 brd 192.168.2.255 scope global eth0\n", "")
            return (1, "", "no ups")

        setattr(api, "run_text_command", fake_run)
        setattr(api, "systemd_active", lambda unit: None)
        summary = api.build_summary({})
        assert summary["module"]["lan_connected"] is True
        assert summary["module"]["lan_ip"] == "192.168.2.123"
        assert re.match(r"^\d{2}:\d{2}$", summary["module"]["time_hhmm"])
    finally:
        setattr(api, "run_text_command", old_run)
        setattr(api, "systemd_active", old_systemd)


def test_summary_can_enable_placeholder_pages_without_telemetry():
    def check():
        summary = api.build_summary({"modules": {"nut": True, "proxmox": True, "ha": True}})
        assert summary["pages"] == ["NUT", "PROXMOX", "HA"]
        assert summary["modules"]["proxmox"]["status"] == "placeholder"
        assert summary["modules"]["ha"]["severity"] == "unknown"
        assert summary["severity"] == "warn"
    with_fake_nut(check)


def test_nut_summary_payload_counts_local_primary_and_configured_clients():
    old_load = api.load_nut_clients
    old_run = api.run_text_command
    old_systemd = api.systemd_active
    try:
        setattr(api, "load_nut_clients", lambda: [
            {"name": "pve", "host": "192.168.2.99", "role": "secondary", "enabled": True}
        ])
        setattr(api, "run_text_command", lambda *args, **kwargs: (0, "ups.status: OL\nbattery.charge: 70", ""))
        setattr(api, "systemd_active", lambda unit: True)
        summary = api.build_summary({"modules": {"nut": True}})
        assert summary["nut"]["client_count"] == 2
        assert [client["name"] for client in summary["nut"]["clients"]] == ["power-sentinel", "pve"]
    finally:
        setattr(api, "load_nut_clients", old_load)
        setattr(api, "run_text_command", old_run)
        setattr(api, "systemd_active", old_systemd)

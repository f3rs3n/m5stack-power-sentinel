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


def test_summary_contract_locks_stackflow_safe_aliases_and_top_row_fields():
    old_run = api.run_text_command
    old_systemd = api.systemd_active
    try:
        setattr(api, "run_text_command", lambda *args, **kwargs: (0, "ups.status: OL\nbattery.charge: 70\nbattery.runtime: 1200\nups.load: 20\ninput.voltage: 231", ""))
        setattr(api, "systemd_active", lambda unit: True)
        summary = api.build_summary({"modules": {"nut": True}}, stackflow_safe=True)

        assert summary["schema"] == api.SCHEMA_SUMMARY
        assert summary["profile"] == "nut-monitor-clean-baseline"
        assert summary["stackflow_safe"] is True
        assert summary["pages"] == ["NUT"]
        assert summary["modules"]["nut"]["condition"] == "healthy"
        assert summary["condition"] == "healthy"
        assert summary["severity"] == "ok"
        assert summary["ups"] == summary["modules"]["nut"]["ups"]
        assert summary["nut"] == summary["modules"]["nut"]["nut"]
        assert set(summary["module"]) >= {"lan_connected", "lan_ip", "time_hhmm"}
    finally:
        setattr(api, "run_text_command", old_run)
        setattr(api, "systemd_active", old_systemd)


def test_summary_can_enable_non_nut_pages_without_fake_telemetry():
    def check():
        summary = api.build_summary({"modules": {"nut": True, "proxmox": True, "ha": True}})
        assert summary["pages"] == ["NUT", "PROXMOX", "HA"]
        assert summary["modules"]["proxmox"]["status"] == "unconfigured"
        assert summary["modules"]["proxmox"]["condition"] == "unavailable"
        assert summary["modules"]["proxmox"]["signals"]
        assert summary["modules"]["ha"]["status"] == "placeholder"
        assert summary["modules"]["ha"]["severity"] == "unknown"
        assert summary["condition"] == "unavailable"
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


def test_nut_summary_exposes_condition_and_legacy_severity_alias():
    old_run = api.run_text_command
    old_systemd = api.systemd_active
    try:
        setattr(api, "run_text_command", lambda *args, **kwargs: (0, "ups.status: OL\nbattery.charge: 70", ""))
        setattr(api, "systemd_active", lambda unit: True)
        summary = api.build_summary({"modules": {"nut": True}})
        assert summary["condition"] == "healthy"
        assert summary["severity"] == "ok"
        assert summary["modules"]["nut"]["condition"] == "healthy"
        assert summary["modules"]["nut"]["severity"] == "ok"
    finally:
        setattr(api, "run_text_command", old_run)
        setattr(api, "systemd_active", old_systemd)


def test_nut_condition_maps_power_states_to_legacy_severity_aliases():
    old_run = api.run_text_command
    old_systemd = api.systemd_active
    try:
        setattr(api, "systemd_active", lambda unit: True)

        setattr(api, "run_text_command", lambda *args, **kwargs: (0, "ups.status: OB\nbattery.charge: 70", ""))
        on_battery = api.build_summary({"modules": {"nut": True}})
        assert on_battery["modules"]["nut"]["condition"] == "warning"
        assert on_battery["modules"]["nut"]["severity"] == "warn"

        setattr(api, "run_text_command", lambda *args, **kwargs: (0, "ups.status: OB LB\nbattery.charge: 8", ""))
        low_battery = api.build_summary({"modules": {"nut": True}})
        assert low_battery["modules"]["nut"]["condition"] == "critical"
        assert low_battery["modules"]["nut"]["severity"] == "critical"

        setattr(api, "run_text_command", lambda *args, **kwargs: (1, "", "no ups"))
        unavailable = api.build_summary({"modules": {"nut": True}})
        assert unavailable["modules"]["nut"]["condition"] == "unavailable"
        assert unavailable["modules"]["nut"]["severity"] == "warn"
    finally:
        setattr(api, "run_text_command", old_run)
        setattr(api, "systemd_active", old_systemd)


def test_condition_aggregation_ignores_disabled_modules_and_critical_wins():
    old_run = api.run_text_command
    old_systemd = api.systemd_active
    try:
        setattr(api, "systemd_active", lambda unit: True)

        setattr(api, "run_text_command", lambda *args, **kwargs: (0, "ups.status: OL\nbattery.charge: 70", ""))
        disabled_placeholders = api.build_summary({"modules": {"nut": True, "proxmox": False, "ha": False}})
        assert disabled_placeholders["condition"] == "healthy"
        assert disabled_placeholders["severity"] == "ok"
        assert "condition" not in disabled_placeholders["modules"]["proxmox"]

        setattr(api, "run_text_command", lambda *args, **kwargs: (0, "ups.status: OB LB\nbattery.charge: 8", ""))
        with_enabled_placeholder = api.build_summary({"modules": {"nut": True, "proxmox": True}})
        assert with_enabled_placeholder["condition"] == "critical"
        assert with_enabled_placeholder["severity"] == "critical"
    finally:
        setattr(api, "run_text_command", old_run)
        setattr(api, "systemd_active", old_systemd)


def test_module_runtime_concentrates_registry_aggregation_and_compatibility_aliases():
    runtime = api.ModuleRuntime({
        "nut": api.ModuleSpec(
            "nut",
            "NUT",
            "implemented",
            lambda config: {
                "enabled": True,
                "page": "NUT",
                "condition": "healthy",
                "severity": "ok",
                "ups": {"available": True},
                "nut": {"client_count": 1, "would_shutdown": False},
            },
        ),
        "proxmox": api.ModuleSpec(
            "proxmox",
            "PROXMOX",
            "implemented",
            lambda config: {
                "enabled": True,
                "page": "PROXMOX",
                "condition": "warning",
                "severity": "warn",
                "signals": [{"kind": "storage_warning", "condition": "warning", "summary": "local 86%"}],
            },
        ),
        "ha": api.ModuleSpec("ha", "HA", "placeholder"),
    })

    result = runtime.build({}, {"nut", "proxmox", "ha"})

    assert result.condition == "warning"
    assert result.severity == "warn"
    assert result.available_modules == ["nut", "proxmox", "ha"]
    assert result.pages == ["NUT", "PROXMOX", "HA"]
    assert result.modules["ha"]["condition"] == "unavailable"
    assert result.problems == ["module ha is enabled but not implemented in this clean baseline"]
    assert result.ups == {"available": True}
    assert result.nut == {"client_count": 1, "would_shutdown": False}


def test_health_payload_exposes_condition_and_derives_ok_from_condition():
    healthy = api.build_health_payload({"condition": "healthy", "severity": "ok"})
    critical = api.build_health_payload({"condition": "critical", "severity": "critical"})

    assert healthy["schema"] == api.SCHEMA_HEALTH
    assert healthy["condition"] == "healthy"
    assert healthy["severity"] == "ok"
    assert healthy["ok"] is True
    assert critical["condition"] == "critical"
    assert critical["ok"] is False


def test_enabled_proxmox_without_minimum_config_reports_unavailable_condition():
    old_run = api.run_text_command
    old_systemd = api.systemd_active
    try:
        setattr(api, "run_text_command", lambda *args, **kwargs: (0, "ups.status: OL\nbattery.charge: 70", ""))
        setattr(api, "systemd_active", lambda unit: True)
        summary = api.build_summary({"modules": {"nut": True, "proxmox": True}})
        proxmox = summary["modules"]["proxmox"]

        assert proxmox["enabled"] is True
        assert proxmox["implemented"] is True
        assert proxmox["page"] == "PROXMOX"
        assert proxmox["condition"] == "unavailable"
        assert proxmox["severity"] == "warn"
        assert proxmox["status"] == "unconfigured"
        assert proxmox["signals"][0]["kind"] == "unconfigured"
        assert summary["condition"] == "unavailable"
    finally:
        setattr(api, "run_text_command", old_run)
        setattr(api, "systemd_active", old_systemd)


def test_configured_proxmox_availability_slice_does_not_fake_telemetry():
    summary = api.build_summary({
        "modules": {"nut": False, "proxmox": True},
        "proxmox": {
            "api_url": "https://pve.example:8006",
            "token_id": "power-sentinel@pve!monitor",
            "token_secret": "CHANGE_ME",
            "watched_guests": [101, 120],
        },
    })
    proxmox = summary["modules"]["proxmox"]

    assert proxmox["implemented"] is True
    assert proxmox["condition"] == "unavailable"
    assert proxmox["status"] == "not_observed"
    assert proxmox["environment"]["api_url"] == "https://pve.example:8006"
    assert proxmox["environment"]["watched_guest_count"] == 2
    assert proxmox["signals"][0]["kind"] == "api_unavailable"
    assert proxmox["nodes"] == []
    assert proxmox["watched_guests"] == []


def test_proxmox_api_failure_reports_unavailable_without_exposing_token():
    old_get = api.proxmox_api_get
    try:
        def broken(*args, **kwargs):
            raise RuntimeError("401 Unauthorized: SECRET")

        api.proxmox_api_get = broken
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]
        payload_text = str(proxmox)

        assert proxmox["condition"] == "unavailable"
        assert proxmox["status"] == "api_unavailable"
        assert proxmox["signals"][0]["kind"] == "api_unavailable"
        assert "SECRET" not in payload_text
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_api_get_honors_verify_ssl_false_for_self_signed_pve():
    old_urlopen = api.urlopen
    captured = {}
    try:
        class FakeResponse:
            def __enter__(self):
                return self

            def __exit__(self, *args):
                return False

            def read(self, _size):
                return b'{"data": []}'

        def fake_urlopen(request, timeout, context=None):
            captured["url"] = request.full_url
            captured["auth"] = request.get_header("Authorization")
            captured["timeout"] = timeout
            captured["context"] = context
            return FakeResponse()

        api.urlopen = fake_urlopen
        payload = api.proxmox_api_get({
            "api_url": "https://pve.example:8006",
            "token_id": "power-sentinel@pve!monitor",
            "token_secret": "SECRET",
            "verify_ssl": False,
        }, "/nodes")

        assert payload == {"data": []}
        assert captured["url"] == "https://pve.example:8006/api2/json/nodes"
        assert captured["auth"].startswith("PVEAPIToken=power-sentinel@pve!monitor=")
        assert captured["timeout"] == 4.0
        assert captured["context"] is not None
        assert captured["context"].check_hostname is False
        assert captured["context"].verify_mode == api.ssl.CERT_NONE
    finally:
        api.urlopen = old_urlopen


def test_proxmox_first_healthy_observation_uses_read_only_api_adapter():
    old_get = api.proxmox_api_get
    calls = []
    try:
        def fake_get(config, path):
            calls.append(path)
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve", "status": "online", "cpu": 0.23, "mem": 61, "maxmem": 100}]}
            if path == "/nodes/pve/storage":
                return {"data": []}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert calls == ["/cluster/status", "/nodes", "/nodes/pve/storage"]
        assert proxmox["condition"] == "healthy"
        assert proxmox["severity"] == "ok"
        assert proxmox["status"] == "observed"
        assert proxmox["signals"] == []
        assert proxmox["environment"]["name"] == "pve"
        assert proxmox["environment"]["node_count"] == 1
        assert proxmox["cards"]["cpu"] == {"value_percent": 23, "condition": "healthy"}
        assert proxmox["cards"]["ram"] == {"value_percent": 61, "condition": "healthy"}
        assert proxmox["nodes"] == [{"name": "pve", "condition": "healthy", "status": "online", "cpu_percent": 23, "memory_percent": 61}]
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_cpu_pressure_requires_sustained_threshold_crossing():
    old_get = api.proxmox_api_get
    try:
        cpu_values = iter([0.90, 0.90, 0.99, 0.99])

        def fake_get(config, path):
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve", "status": "online", "cpu": next(cpu_values), "mem": 20, "maxmem": 100}]}
            if path == "/nodes/pve/storage":
                return {"data": []}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        config = {
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [],
            },
        }

        first_warning_sample = api.build_summary(config)["modules"]["proxmox"]
        sustained_warning = api.build_summary(config)["modules"]["proxmox"]
        first_critical_sample = api.build_summary(config)["modules"]["proxmox"]
        sustained_critical = api.build_summary(config)["modules"]["proxmox"]

        assert first_warning_sample["condition"] == "healthy"
        assert first_warning_sample["cards"]["cpu"] == {"value_percent": 90, "condition": "healthy"}
        assert first_warning_sample["signals"] == []

        assert sustained_warning["condition"] == "warning"
        assert sustained_warning["cards"]["cpu"] == {"value_percent": 90, "condition": "warning"}
        assert sustained_warning["signals"][0]["kind"] == "cpu_pressure"
        assert sustained_warning["signals"][0]["condition"] == "warning"
        assert sustained_warning["signals"][0]["context"] == {"node": "pve", "cpu_percent": 90, "threshold_percent": 85}

        assert first_critical_sample["condition"] == "warning"
        assert first_critical_sample["cards"]["cpu"] == {"value_percent": 99, "condition": "warning"}

        assert sustained_critical["condition"] == "critical"
        assert sustained_critical["cards"]["cpu"] == {"value_percent": 99, "condition": "critical"}
        assert sustained_critical["signals"][0]["kind"] == "cpu_pressure"
        assert sustained_critical["signals"][0]["condition"] == "critical"
        assert sustained_critical["signals"][0]["context"] == {"node": "pve", "cpu_percent": 99, "threshold_percent": 98}
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_memory_pressure_requires_sustained_threshold_crossing():
    old_get = api.proxmox_api_get
    try:
        memory_values = iter([91, 91, 99, 99])

        def fake_get(config, path):
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve", "status": "online", "cpu": 0.20, "mem": next(memory_values), "maxmem": 100}]}
            if path == "/nodes/pve/storage":
                return {"data": []}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        config = {
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [],
            },
        }

        first_warning_sample = api.build_summary(config)["modules"]["proxmox"]
        sustained_warning = api.build_summary(config)["modules"]["proxmox"]
        first_critical_sample = api.build_summary(config)["modules"]["proxmox"]
        sustained_critical = api.build_summary(config)["modules"]["proxmox"]

        assert first_warning_sample["condition"] == "healthy"
        assert first_warning_sample["cards"]["ram"] == {"value_percent": 91, "condition": "healthy"}
        assert first_warning_sample["signals"] == []

        assert sustained_warning["condition"] == "warning"
        assert sustained_warning["cards"]["ram"] == {"value_percent": 91, "condition": "warning"}
        assert sustained_warning["signals"][0]["kind"] == "memory_pressure"
        assert sustained_warning["signals"][0]["condition"] == "warning"
        assert sustained_warning["signals"][0]["context"] == {"node": "pve", "memory_percent": 91, "threshold_percent": 90}

        assert first_critical_sample["condition"] == "warning"
        assert first_critical_sample["cards"]["ram"] == {"value_percent": 99, "condition": "warning"}

        assert sustained_critical["condition"] == "critical"
        assert sustained_critical["cards"]["ram"] == {"value_percent": 99, "condition": "critical"}
        assert sustained_critical["signals"][0]["kind"] == "memory_pressure"
        assert sustained_critical["signals"][0]["condition"] == "critical"
        assert sustained_critical["signals"][0]["context"] == {"node": "pve", "memory_percent": 99, "threshold_percent": 98}
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_single_node_auto_selects_without_extra_config():
    old_get = api.proxmox_api_get
    calls = []
    try:
        def fake_get(config, path):
            calls.append(path)
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve-solo", "status": "online", "cpu": 0.12, "mem": 20, "maxmem": 100}]}
            if path == "/nodes/pve-solo/storage":
                return {"data": []}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert calls == ["/cluster/status", "/nodes", "/nodes/pve-solo/storage"]
        assert proxmox["condition"] == "healthy"
        assert proxmox["status"] == "observed"
        assert proxmox["environment"]["node_name"] == "pve-solo"
        assert proxmox["nodes"] == [{"name": "pve-solo", "condition": "healthy", "status": "online", "cpu_percent": 12, "memory_percent": 20}]
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_multi_node_without_node_name_is_unavailable_not_guessed():
    old_get = api.proxmox_api_get
    calls = []
    try:
        def fake_get(config, path):
            calls.append(path)
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [
                    {"node": "pve-a", "status": "online", "cpu": 0.12, "mem": 20, "maxmem": 100},
                    {"node": "pve-b", "status": "online", "cpu": 0.20, "mem": 30, "maxmem": 100},
                ]}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert calls == ["/cluster/status", "/nodes"]
        assert proxmox["condition"] == "unavailable"
        assert proxmox["severity"] == "warn"
        assert proxmox["status"] == "node_selection_required"
        assert proxmox["nodes"] == []
        assert [signal["kind"] for signal in proxmox["signals"]] == ["node_selection_required"]
        assert proxmox["signals"][0]["context"] == {"visible_nodes": ["pve-a", "pve-b"]}
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_explicit_node_name_observes_only_selected_node():
    old_get = api.proxmox_api_get
    calls = []
    try:
        def fake_get(config, path):
            calls.append(path)
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [
                    {"node": "pve-a", "status": "online", "cpu": 0.12, "mem": 20, "maxmem": 100},
                    {"node": "pve-b", "status": "online", "cpu": 0.88, "mem": 90, "maxmem": 100},
                ]}
            if path == "/nodes/pve-b/storage":
                return {"data": [{"storage": "local", "type": "dir", "enabled": 1, "active": 1, "used": 84, "total": 100}]}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "node_name": "pve-b",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert calls == ["/cluster/status", "/nodes", "/nodes/pve-b/storage"]
        assert proxmox["condition"] == "healthy"
        assert proxmox["environment"]["node_name"] == "pve-b"
        assert proxmox["nodes"] == [{"name": "pve-b", "condition": "healthy", "status": "online", "cpu_percent": 88, "memory_percent": 90}]
        assert proxmox["storage"] == [{"node": "pve-b", "name": "local", "condition": "healthy", "type": "dir", "used_percent": 84}]
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_selected_node_unavailable_is_module_unavailable_without_node_signal():
    old_get = api.proxmox_api_get
    try:
        def fake_get(config, path):
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [
                    {"node": "pve-a", "status": "online"},
                    {"node": "pve-b", "status": "offline"},
                ]}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "node_name": "pve-b",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert proxmox["condition"] == "unavailable"
        assert proxmox["status"] == "selected_node_unavailable"
        assert proxmox["nodes"] == []
        assert [signal["kind"] for signal in proxmox["signals"]] == ["selected_node_unavailable"]
        assert "node_down" not in str(proxmox)
        assert proxmox["signals"][0]["context"] == {"node": "pve-b", "status": "offline"}
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_watched_guests_running_are_healthy_inventory_only():
    old_get = api.proxmox_api_get
    calls = []
    try:
        def fake_get(config, path):
            calls.append(path)
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve-a", "status": "online"}]}
            if path == "/nodes/pve-a/storage":
                return {"data": []}
            if path == "/nodes/pve-a/qemu":
                return {"data": [
                    {"vmid": 101, "name": "ha", "status": "running"},
                    {"vmid": 120, "name": "db", "status": "running"},
                    {"vmid": 999, "name": "lab", "status": "stopped"},
                ]}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [101, 120],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert calls == ["/cluster/status", "/nodes", "/nodes/pve-a/storage", "/nodes/pve-a/qemu"]
        assert proxmox["condition"] == "healthy"
        assert proxmox["signals"] == []
        assert proxmox["environment"]["watched_guest_count"] == 2
        assert proxmox["environment"]["running_watched_guest_count"] == 2
        assert proxmox["watched_guests"] == [
            {"vmid": 101, "name": "ha", "node": "pve-a", "condition": "healthy", "status": "running"},
            {"vmid": 120, "name": "db", "node": "pve-a", "condition": "healthy", "status": "running"},
        ]
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_watched_guest_stopped_or_missing_is_critical_signal():
    old_get = api.proxmox_api_get
    try:
        def fake_get(config, path):
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve-a", "status": "online"}, {"node": "pve-b", "status": "offline"}]}
            if path == "/nodes/pve-a/storage":
                return {"data": []}
            if path == "/nodes/pve-a/qemu":
                return {"data": [{"vmid": 101, "name": "ha", "status": "stopped"}]}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "node_name": "pve-a",
                "watched_guests": [101, 120],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert proxmox["condition"] == "critical"
        assert proxmox["environment"]["running_watched_guest_count"] == 0
        assert proxmox["watched_guests"] == [
            {"vmid": 101, "name": "ha", "node": "pve-a", "condition": "critical", "status": "stopped"},
            {"vmid": 120, "name": None, "node": None, "condition": "critical", "status": "missing"},
        ]
        kinds = [signal["kind"] for signal in proxmox["signals"]]
        assert kinds == ["watched_guest_down", "watched_guest_down"]
        assert proxmox["signals"][0]["context"]["vmid"] == 101
        assert proxmox["signals"][1]["context"]["vmid"] == 120
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_storage_warning_and_critical_signals_from_online_nodes():
    old_get = api.proxmox_api_get
    calls = []
    try:
        def fake_get(config, path):
            calls.append(path)
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve-a", "status": "online"}, {"node": "pve-b", "status": "offline"}]}
            if path == "/nodes/pve-a/storage":
                return {"data": [
                    {"storage": "local", "type": "dir", "enabled": 1, "active": 1, "used": 86, "total": 100},
                    {"storage": "local-lvm", "type": "lvmthin", "enabled": 1, "active": 1, "used": 96, "total": 100},
                    {"storage": "backup", "type": "nfs", "enabled": 0, "active": 0, "used": 99, "total": 100},
                ]}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "node_name": "pve-a",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert calls == ["/cluster/status", "/nodes", "/nodes/pve-a/storage"]
        assert proxmox["condition"] == "critical"
        assert proxmox["environment"]["storage_count"] == 2
        assert proxmox["storage"] == [
            {"node": "pve-a", "name": "local", "condition": "warning", "type": "dir", "used_percent": 86},
            {"node": "pve-a", "name": "local-lvm", "condition": "critical", "type": "lvmthin", "used_percent": 96},
        ]
        assert proxmox["cards"]["storage"] == {"value_percent": 96, "condition": "critical"}
        assert [signal["kind"] for signal in proxmox["signals"]] == ["storage_warning", "storage_critical"]
        assert proxmox["signals"][0]["context"] == {"node": "pve-a", "storage": "local", "used_percent": 86}
        assert proxmox["signals"][1]["context"] == {"node": "pve-a", "storage": "local-lvm", "used_percent": 96}
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_storage_below_warning_threshold_is_healthy_inventory():
    old_get = api.proxmox_api_get
    try:
        def fake_get(config, path):
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve-a", "status": "online"}]}
            if path == "/nodes/pve-a/storage":
                return {"data": [{"storage": "local", "type": "dir", "enabled": 1, "active": 1, "used": 84, "total": 100}]}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert proxmox["condition"] == "healthy"
        assert proxmox["signals"] == []
        assert proxmox["cards"]["storage"] == {"value_percent": 84, "condition": "healthy"}
        assert proxmox["storage"] == [{"node": "pve-a", "name": "local", "condition": "healthy", "type": "dir", "used_percent": 84}]
    finally:
        api.proxmox_api_get = old_get


def test_proxmox_storage_warning_only_sets_module_warning_not_critical():
    old_get = api.proxmox_api_get
    try:
        def fake_get(config, path):
            if path == "/cluster/status":
                return {"data": [{"type": "cluster", "name": "pve"}]}
            if path == "/nodes":
                return {"data": [{"node": "pve-a", "status": "online"}]}
            if path == "/nodes/pve-a/storage":
                return {"data": [{"storage": "local", "type": "dir", "enabled": 1, "active": 1, "used": 86, "total": 100}]}
            raise AssertionError(path)

        api.proxmox_api_get = fake_get
        summary = api.build_summary({
            "modules": {"nut": False, "proxmox": True},
            "proxmox": {
                "api_url": "https://pve.example:8006",
                "token_id": "power-sentinel@pve!monitor",
                "token_secret": "SECRET",
                "watched_guests": [],
            },
        })
        proxmox = summary["modules"]["proxmox"]

        assert proxmox["condition"] == "warning"
        assert proxmox["severity"] == "warn"
        assert proxmox["cards"]["storage"] == {"value_percent": 86, "condition": "warning"}
        assert [signal["kind"] for signal in proxmox["signals"]] == ["storage_warning"]
    finally:
        api.proxmox_api_get = old_get

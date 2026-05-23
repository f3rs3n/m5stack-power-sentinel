import importlib.util
import json
import pathlib


ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "bin" / "power-sentinel-api.py"


def load_module():
    spec = importlib.util.spec_from_file_location("power_sentinel_api", MODULE_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_build_summary_has_stable_v1_contract():
    api = load_module()

    summary = api.build_summary(
        now=1_770_000_000,
        health={
            "overall_ok": True,
            "system": {
                "temperature_c": 45.4,
                "linux_mem": {"available_mb": 500},
                "root_disk": {"free_gb": 20.5},
            },
            "ports": {"stackflow_10001": {"ok": True}, "openai_8000": {"ok": True}},
            "openai": {"models": {"ok": True}},
            "stackflow": {"lsmode_ok": True},
            "chat_smoke": {"ok": True},
        },
        ups=None,
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
        network={"available": True, "severity": "ok", "default_route": True, "probe": "tcp", "target": "1.1.1.1:53"},
        pve=api.summarize_proxmox_data(
            node="pve-mini",
            latency_ms=10,
            node_status={"status": "online", "cpu": 0.1, "mem": 1, "maxmem": 2},
            qemu=[],
            lxc=[],
            zfs=[],
            disks=[],
        ),
    )

    assert summary["schema"] == "power-sentinel.summary.v1"
    assert summary["timestamp"] == "2026-02-02T02:40:00Z"
    assert summary["severity"] == "warn"
    assert summary["ups"]["available"] is False
    assert summary["ups"]["status"] == "UNAVAILABLE"
    assert summary["ups"]["stale"] is True
    assert summary["homeassistant"]["available"] is True
    assert summary["homeassistant"]["severity"] == "ok"
    assert summary["homeassistant"]["mqtt"] is True
    assert summary["proxmox"]["available"] is True
    assert summary["proxmox"]["severity"] == "ok"
    assert summary["proxmox"]["shutdown_state"] == "disarmed"
    assert summary["network"] == {"available": True, "severity": "ok", "default_route": True, "probe": "tcp", "target": "1.1.1.1:53"}
    assert summary["m5stack"]["temperature_c"] == 45.4
    assert summary["m5stack"]["stackflow_ok"] is True
    assert summary["m5stack"]["openai_ok"] is True
    assert summary["m5stack"]["chat_smoke_ok"] is True
    assert "UPS data unavailable" in summary["problems"]


def test_parse_upsc_derives_battery_status_and_numeric_values():
    api = load_module()

    ups = api.parse_upsc_output(
        """
ups.status: OB LB DISCHRG
battery.charge: 42
battery.runtime: 900
ups.load: 31
input.voltage: 228.5
output.voltage: 230.0
device.mfr: TestCorp
device.model: TestUPS 900
""".strip()
    )

    assert ups["available"] is True
    assert ups["status"] == "OB LB DISCHRG"
    assert ups["status_label"] == "Low battery"
    assert ups["on_battery"] is True
    assert ups["low_battery"] is True
    assert ups["battery_percent"] == 42
    assert ups["runtime_seconds"] == 900
    assert ups["load_percent"] == 31
    assert ups["input_voltage"] == 228.5
    assert ups["output_voltage"] == 230.0
    assert ups["device"]["manufacturer"] == "TestCorp"


def test_parse_upsc_exposes_extended_nut_fields_and_estimated_load_watts():
    api = load_module()

    ups = api.parse_upsc_output(
        """
ups.status: OL
battery.charge: 100
battery.runtime: 384
battery.voltage: 13.6
input.voltage: 224.0
input.transfer.reason: input voltage out of range
ups.load: 38
ups.realpower.nominal: 520
ups.beeper.status: disabled
ups.model: Back-UPS ES 850G2
ups.mfr: American Power Conversion
ups.serial: 5B2350TD6030
ups.firmware: 938.a2 .I
driver.name: usbhid-ups
driver.version: 2.7.4
""".strip()
    )

    assert ups["model"] == "Back-UPS ES 850G2"
    assert ups["manufacturer"] == "American Power Conversion"
    assert ups["serial"] == "5B2350TD6030"
    assert ups["battery_voltage"] == 13.6
    assert ups["realpower_nominal_w"] == 520
    assert ups["load_w"] == 198
    assert ups["beeper_status"] == "disabled"
    assert ups["transfer_reason"] == "input voltage out of range"
    assert ups["driver"] == "usbhid-ups"
    assert ups["firmware"] == "938.a2 .I"


def test_build_summary_includes_extended_ups_fields():
    api = load_module()

    ups = api.parse_upsc_output(
        """
ups.status: OL
battery.charge: 100
battery.runtime: 384
battery.voltage: 13.6
input.voltage: 224.0
input.transfer.reason: input voltage out of range
ups.load: 38
ups.realpower.nominal: 520
ups.beeper.status: disabled
ups.model: Back-UPS ES 850G2
driver.name: usbhid-ups
""".strip()
    )
    summary = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True},
        ups=ups,
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
    )

    assert summary["ups"]["model"] == "Back-UPS ES 850G2"
    assert summary["ups"]["battery_voltage"] == 13.6
    assert summary["ups"]["realpower_nominal_w"] == 520
    assert summary["ups"]["load_w"] == 198
    assert summary["ups"]["beeper_status"] == "disabled"
    assert summary["ups"]["transfer_reason"] == "input voltage out of range"
    assert summary["ups"]["driver"] == "usbhid-ups"


def test_m5stack_chat_smoke_missing_or_not_run_is_unknown_not_failure():
    api = load_module()

    missing = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True, "ports": {"stackflow_10001": {"ok": True}, "openai_8000": {"ok": True}}, "openai": {"models": {"ok": True}}},
        ups=api.parse_upsc_output("ups.status: OL"),
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
    )
    not_run = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True, "ports": {"stackflow_10001": {"ok": True}, "openai_8000": {"ok": True}}, "chat_smoke": {"ok": False, "not_run": True}},
        ups=api.parse_upsc_output("ups.status: OL"),
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
    )

    assert missing["m5stack"]["chat_smoke_ok"] is None
    assert not_run["m5stack"]["chat_smoke_ok"] is None


def test_build_summary_includes_nut_service_state_when_available():
    api = load_module()

    summary = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True},
        ups=api.unavailable_ups(),
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
        nut={
            "server_active": True,
            "driver_active": True,
            "monitor_active": False,
            "mode": "netserver",
            "client_count": None,
            "clients": [],
            "shutdown_state": "disarmed",
        },
    )

    assert summary["nut"] == {
        "server_active": True,
        "driver_active": True,
        "monitor_active": False,
        "mode": "netserver",
        "client_count": None,
        "clients": [],
        "shutdown_state": "disarmed",
    }


def test_nut_client_readiness_is_generic_and_distinguishes_discovery_states():
    api = load_module()

    base = {"name": "pve", "host": "192.168.2.99", "role": "secondary"}
    not_configured = api.summarize_nut_client(base, {"clients": []})
    reachable = api.summarize_nut_client(base | {"upsc_reachable": True, "package_installed": True, "config_present": False}, {"clients": []})
    connected = api.summarize_nut_client(base | {"upsc_reachable": True, "package_installed": True, "config_present": True, "monitor_active": False}, {"clients": ["192.168.2.99"]})
    armed = api.summarize_nut_client(base | {"upsc_reachable": True, "package_installed": True, "config_present": True, "monitor_active": True}, {"clients": ["192.168.2.99"]})

    assert not_configured["state"] == "not_configured"
    assert not_configured["reachable_via_upsc"] is None
    assert not_configured["connected_as_upsmon"] is False
    assert not_configured["armed"] is False
    assert reachable["state"] == "reachable_via_upsc"
    assert connected["state"] == "connected_as_upsmon"
    assert armed["state"] == "armed"
    assert armed["configured"] is True
    assert armed["host"] == "192.168.2.99"
    assert armed["name"] == "pve"


def test_standard_nut_shutdown_reports_generic_client_readiness_without_ambiguous_mode_fields():
    api = load_module()
    ups = api.parse_upsc_output(
        """
ups.status: OL
battery.charge: 100
battery.charge.low: 10
battery.runtime: 384
battery.runtime.low: 120
""".strip()
    )
    nut = {
        "server_active": True,
        "driver_active": True,
        "monitor_active": False,
        "mode": "netserver",
        "client_count": 0,
        "clients": [],
        "shutdown_state": "disarmed",
    }

    shutdown = api.summarize_standard_nut_shutdown(ups, nut, clients=[{"name": "pve", "host": "192.168.2.99", "role": "secondary", "package_installed": True, "upsc_reachable": True}])

    assert "strategy" not in shutdown
    assert "mode" not in shutdown
    assert shutdown["armed"] is False
    assert shutdown["real_shutdown_owner"] == "upsmon"
    assert shutdown["primary_ready"] is True
    assert shutdown["primary_monitor_active"] is False
    assert shutdown["secondary_ready"] is False
    assert shutdown["would_shutdown"] is False
    assert shutdown["reason"] == "UPS online"
    assert shutdown["thresholds"] == {"battery_charge_low_percent": 10, "battery_runtime_low_seconds": 120}
    assert shutdown["nut_clients"][0]["name"] == "pve"
    assert shutdown["nut_clients"][0]["state"] == "reachable_via_upsc"
    assert shutdown["nut_client_summary"] == {"total": 1, "secondary_total": 1, "connected": 0, "armed": 0}


def test_standard_nut_shutdown_would_shutdown_on_low_battery_only():
    api = load_module()
    nut = {"server_active": True, "driver_active": True, "monitor_active": False, "mode": "netserver", "client_count": 1, "clients": ["pve"], "shutdown_state": "disarmed"}

    on_battery = api.summarize_standard_nut_shutdown(api.parse_upsc_output("ups.status: OB\nbattery.runtime: 600"), nut)
    low_battery = api.summarize_standard_nut_shutdown(api.parse_upsc_output("ups.status: OB LB\nbattery.runtime: 90"), nut)

    assert on_battery["would_shutdown"] is False
    assert on_battery["reason"] == "UPS on battery, waiting for NUT LOWBATT/FSD"
    assert low_battery["would_shutdown"] is True
    assert low_battery["reason"] == "UPS low battery; standard NUT upsmon would initiate shutdown"


def test_build_summary_exposes_standard_nut_shutdown_contract():
    api = load_module()
    nut = {"server_active": True, "driver_active": True, "monitor_active": False, "mode": "netserver", "client_count": 0, "clients": [], "shutdown_state": "disarmed"}

    summary = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True},
        ups=api.parse_upsc_output("ups.status: OL\nbattery.charge.low: 10\nbattery.runtime.low: 120"),
        nut=nut,
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
        pve=api.summarize_proxmox_data("pve", 1, {"status": "online"}, [], [], [], []),
        network={"available": True, "severity": "ok", "default_route": True, "probe": "tcp", "target": "1.1.1.1:53"},
        mqtt={"available": True, "severity": "ok", "broker": "192.168.2.200:1883"},
        homeassistant_mqtt={"status": "unknown", "source": "mqtt", "status_topic": "homeassistant/status"},
        zigbee2mqtt=api.summarize_zigbee2mqtt_payloads("zigbee2mqtt", '{"state":"online"}', '{"coordinator": {"type": "ember"}}', "[]"),
    )

    assert "strategy" not in summary["shutdown"]
    assert "mode" not in summary["shutdown"]
    assert "nut_clients" in summary["shutdown"]
    assert "shutdown" not in summary["problems"]


def test_summarize_proxmox_data_reports_node_metrics_workloads_zfs_and_smart():
    api = load_module()

    pve = api.summarize_proxmox_data(
        node="pve-mini",
        latency_ms=42,
        node_status={"status": "online", "cpu": 0.18, "memory": {"used": 8 * 1024**3, "total": 16 * 1024**3}, "rootfs": {"used": 62, "total": 100}},
        qemu=[
            {"name": "ha", "status": "running", "cpu": 0.12, "mem": 4 * 1024**3, "maxmem": 8 * 1024**3, "disk": 40 * 1024**3, "maxdisk": 100 * 1024**3},
            {"name": "test", "status": "stopped", "cpu": 0.99, "mem": 1, "maxmem": 2, "disk": 1, "maxdisk": 2},
            {"vmid": 101, "status": "running"},
        ],
        lxc=[{"name": "hermes", "status": "running", "cpu": 0.03, "mem": 1024**3, "maxmem": 2 * 1024**3, "disk": 8 * 1024**3, "maxdisk": 16 * 1024**3}, {"name": "old", "status": "stopped"}],
        zfs=[{"name": "rpool", "health": "ONLINE", "alloc": 55, "size": 100}],
        disks=[{"devpath": "/dev/sda", "health": "PASSED"}, {"devpath": "/dev/sdb", "health": "OK"}],
    )

    assert pve["available"] is True
    assert pve["severity"] == "ok"
    assert pve["node"] == "pve-mini"
    assert pve["api_latency_ms"] == 42
    assert pve["cpu_percent"] == 18
    assert pve["ram_percent"] == 50
    assert pve["storage_percent"] == 62
    assert pve["zfs"]["status"] == "ONLINE"
    assert pve["zfs"]["pools"][0]["capacity_percent"] == 55
    assert pve["smart"]["status"] == "OK"
    assert pve["vm"]["running_count"] == 2
    assert pve["vm"]["running_names"] == ["ha", "101"]
    assert pve["vm"]["running_items"][0] == {
        "name": "ha",
        "cpu_percent": 12,
        "ram_percent": 50,
        "ram_total_bytes": 8 * 1024**3,
        "disk_percent": 40,
        "disk_total_bytes": 100 * 1024**3,
    }
    assert pve["lxc"]["running_count"] == 1
    assert pve["lxc"]["running_names"] == ["hermes"]
    assert pve["lxc"]["running_items"][0]["cpu_percent"] == 3
    assert pve["lxc"]["running_items"][0]["ram_percent"] == 50
    assert pve["lxc"]["running_items"][0]["disk_percent"] == 50
    assert pve["shutdown_state"] == "disarmed"


def test_summarize_proxmox_data_marks_zfs_and_smart_failures_critical():
    api = load_module()

    pve = api.summarize_proxmox_data(
        node="pve-mini",
        latency_ms=11,
        node_status={"status": "online", "cpu": 0.10, "mem": 1, "maxmem": 2},
        qemu=[],
        lxc=[],
        zfs=[{"name": "rpool", "health": "DEGRADED", "alloc": 50, "size": 100}],
        disks=[{"devpath": "/dev/sda", "health": "FAILED"}],
    )

    assert pve["available"] is True
    assert pve["severity"] == "critical"
    assert pve["zfs"]["status"] == "DEGRADED"
    assert pve["smart"]["status"] == "FAIL"
    assert "ZFS DEGRADED" in pve["problems"]
    assert "SMART disk health failure" in pve["problems"]


def test_build_summary_uses_pve_read_only_payload_and_global_criticality():
    api = load_module()
    pve = api.summarize_proxmox_data(
        node="pve-mini",
        latency_ms=11,
        node_status={"status": "online", "cpu": 0.10, "mem": 1, "maxmem": 2},
        qemu=[],
        lxc=[],
        zfs=[{"name": "rpool", "health": "DEGRADED"}],
        disks=[],
    )

    summary = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True},
        ups=api.parse_upsc_output("ups.status: OL"),
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
        pve=pve,
    )

    assert summary["severity"] == "critical"
    assert summary["proxmox"]["node"] == "pve-mini"
    assert summary["proxmox"]["zfs"]["status"] == "DEGRADED"
    assert "ZFS DEGRADED" in summary["problems"]


def test_build_summary_reports_network_probe_and_warns_when_internet_unavailable():
    api = load_module()

    summary = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True},
        ups=api.parse_upsc_output("ups.status: OL"),
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
        network={"available": False, "severity": "warn", "default_route": True, "probe": "tcp", "target": "1.1.1.1:53"},
        pve=api.summarize_proxmox_data(
            node="pve-mini",
            latency_ms=10,
            node_status={"status": "online", "cpu": 0.1, "mem": 1, "maxmem": 2},
            qemu=[],
            lxc=[],
            zfs=[],
            disks=[],
        ),
    )

    assert summary["network"]["available"] is False
    assert summary["network"]["target"] == "1.1.1.1:53"
    assert summary["severity"] == "warn"
    assert "Internet/network probe failed" in summary["problems"]


def test_mosquitto_config_uses_option_file_style_and_keeps_password_out_of_argv():
    api = load_module()
    cfg = {"host": "192.168.2.200", "port": 1883, "username": "user", "password": "secret"}

    lines = api.mosquitto_config_lines(cfg)
    argv = api.mosquitto_sub_command("zigbee2mqtt/bridge/state", timeout=5)

    assert "-P secret" in lines
    assert "secret" not in " ".join(argv)
    assert "-t" in argv
    assert "zigbee2mqtt/bridge/state" in argv


def test_summarize_zigbee2mqtt_payloads_reports_coordinator_and_device_counts():
    api = load_module()

    z2m = api.summarize_zigbee2mqtt_payloads(
        base_topic="zigbee2mqtt",
        state_payload='{"state":"online"}',
        info_payload=json.dumps({
            "version": "1.40.0",
            "coordinator": {"type": "zStack3x0", "ieee_address": "0x00124b", "meta": {"revision": "20240710"}},
            "config": {"advanced": {"channel": 11, "pan_id": 6754}},
        }),
        devices_payload=json.dumps([
            {"friendly_name": "Coordinator", "type": "Coordinator", "disabled": False, "interview_completed": True},
            {"friendly_name": "lamp", "disabled": False, "interview_completed": True},
            {"friendly_name": "old_sensor", "disabled": True, "interview_completed": False},
        ]),
    )

    assert z2m["available"] is True
    assert z2m["state"] == "online"
    assert z2m["severity"] == "ok"
    assert z2m["version"] == "1.40.0"
    assert z2m["coordinator"]["available"] is True
    assert z2m["coordinator"]["type"] == "zStack3x0"
    assert z2m["coordinator"]["firmware"] == "20240710"
    assert z2m["devices"] == {"total": 3, "interviewed": 2, "disabled": 1}




def test_homeassistant_updates_count_update_entities_and_default_zero_without_token():
    api = load_module()

    unavailable = api.summarize_homeassistant_updates(None, "HA token not configured")
    assert unavailable["available"] is False
    assert unavailable["available_count"] == 0
    assert unavailable["items"] == []
    assert unavailable["problems"] == []

    updates = api.summarize_homeassistant_updates([
        {"entity_id": "update.home_assistant_core_update", "state": "on", "attributes": {"friendly_name": "Home Assistant Core", "installed_version": "2026.5.1", "latest_version": "2026.5.2"}},
        {"entity_id": "update.zigbee2mqtt_update", "state": "off", "attributes": {"friendly_name": "Zigbee2MQTT"}},
        {"entity_id": "sensor.some_sensor", "state": "on", "attributes": {}},
    ])
    assert updates["available"] is True
    assert updates["available_count"] == 1
    assert updates["items"][0]["name"] == "Home Assistant Core"

    summary = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True},
        ups=api.parse_upsc_output("ups.status: OL"),
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
        homeassistant_updates=updates,
    )
    assert summary["homeassistant"]["updates"]["available"] is True
    assert summary["homeassistant"]["updates"]["available_count"] == 1

def test_build_summary_includes_mqtt_first_ha_and_zigbee2mqtt_status():
    api = load_module()
    mqtt = {"available": True, "severity": "ok", "broker": "192.168.2.200:1883"}
    z2m = api.summarize_zigbee2mqtt_payloads(
        base_topic="zigbee2mqtt",
        state_payload='{"state":"online"}',
        info_payload=json.dumps({"coordinator": {"type": "ember", "ieee_address": "0xabc"}}),
        devices_payload="[]",
    )

    summary = api.build_summary(
        now=1_770_000_000,
        health={"overall_ok": True},
        ups=api.parse_upsc_output("ups.status: OL"),
        checks={"homeassistant": True, "mqtt": True, "proxmox": True},
        mqtt=mqtt,
        homeassistant_mqtt={"status": "unknown", "source": "mqtt", "status_topic": "homeassistant/status"},
        zigbee2mqtt=z2m,
        pve=api.summarize_proxmox_data("pve", 1, {"status": "online"}, [], [], [], []),
        network={"available": True, "severity": "ok", "default_route": True, "probe": "tcp", "target": "1.1.1.1:53"},
    )

    assert summary["mqtt"]["available"] is True
    assert summary["homeassistant"]["source"] == "mqtt"
    assert summary["homeassistant"]["status"] == "unknown"
    assert summary["zigbee2mqtt"]["available"] is True
    assert summary["zigbee2mqtt"]["coordinator"]["available"] is True


def test_http_json_response_is_valid_for_summary_endpoint():
    api = load_module()
    body, status, content_type = api.route_request("/api/v1/summary")

    assert status == 200
    assert content_type == "application/json"
    payload = json.loads(body.decode("utf-8"))
    assert payload["schema"] == "power-sentinel.summary.v1"
    assert "ups" in payload
    assert "m5stack" in payload


def test_health_endpoint_reports_service_name_and_ok_boolean():
    api = load_module()
    body, status, _ = api.route_request("/api/v1/health")

    assert status == 200
    payload = json.loads(body.decode("utf-8"))
    assert payload["schema"] == "power-sentinel.health.v1"
    assert payload["service"] == "power-sentinel-api"
    assert isinstance(payload["ok"], bool)

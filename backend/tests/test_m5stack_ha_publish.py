import importlib.util
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "bin" / "m5stack-ha-publish.py"


def load_module():
    spec = importlib.util.spec_from_file_location("m5stack_ha_publish", MODULE_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_mosquitto_pub_command_keeps_password_out_of_argv_and_uses_stdin_payload():
    pub = load_module()

    argv = pub.mosquitto_pub_command("m5stack/llm/state", retain=True)
    lines = pub.mosquitto_config_lines("192.168.2.200", 1883, "user", "secret")

    assert argv == ["mosquitto_pub", "-t", "m5stack/llm/state", "-r", "-s"]
    assert "secret" not in " ".join(argv)
    assert "-P secret" in lines


def test_mosquitto_pub_non_retained_command_omits_retain_flag():
    pub = load_module()

    argv = pub.mosquitto_pub_command("m5stack/llm/state", retain=False)

    assert "-r" not in argv
    assert argv[-1] == "-s"

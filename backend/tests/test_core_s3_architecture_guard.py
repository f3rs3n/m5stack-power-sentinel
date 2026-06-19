import importlib.util
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[2]
GUARD_PATH = ROOT / "tools" / "check_core_s3_architecture.py"


def load_guard_module():
    spec = importlib.util.spec_from_file_location("check_core_s3_architecture", GUARD_PATH)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_core_s3_architecture_guard_flags_main_page_policy_re_coupling():
    guard = load_guard_module()
    findings = guard.analyze_sources({
        "main.cpp": """
            LedcardsInterfaceNutView makeLedcardsInterfaceNutView() { return {}; }
            ProxmoxAmbientView makeProxmoxAmbientView() { return {}; }
            uint8_t ambientPageCount() { return 2; }
        """,
        "ambient-console-shell.h": "struct AmbientConsoleShell {};",
        "ambient-console-nut-page.h": "struct AmbientConsoleNutPage {};",
        "ambient-console-proxmox-page.h": "struct AmbientConsoleProxmoxPage {};",
    })

    assert any("main.cpp" in finding and "makeLedcardsInterfaceNutView" in finding for finding in findings)
    assert any("main.cpp" in finding and "makeProxmoxAmbientView" in finding for finding in findings)
    assert any("main.cpp" in finding and "ambientPageCount" in finding for finding in findings)


def test_core_s3_architecture_guard_flags_shell_module_mapping_policy_re_coupling():
    guard = load_guard_module()
    findings = guard.analyze_sources({
        "main.cpp": "AmbientConsoleShell ambientShell; ambientShell.refresh(state);",
        "ambient-console-shell.h": """
            struct AmbientConsoleShell {
              LedcardsInterfaceNutView makeNutPageView(const SummaryState &state) {
                LedcardsInterfaceNutView view{};
                view.batteryPercent = state.ups.batteryPercent;
                return view;
              }
              ProxmoxAmbientView makeProxmoxAmbientView(const SummaryState &state) {
                ProxmoxAmbientView view{};
                view.cpuPercent = state.proxmox.cpuPercent;
                return view;
              }
            };
        """,
        "ambient-console-nut-page.h": "struct AmbientConsoleNutPage {};",
        "ambient-console-proxmox-page.h": "struct AmbientConsoleProxmoxPage {};",
    })

    assert any("ambient-console-shell.h" in finding and "state.ups.batteryPercent" in finding for finding in findings)
    assert any("ambient-console-shell.h" in finding and "state.proxmox.cpuPercent" in finding for finding in findings)


def test_core_s3_architecture_guard_accepts_current_shell_page_split():
    guard = load_guard_module()
    findings = guard.analyze_repo(ROOT)

    assert findings == []

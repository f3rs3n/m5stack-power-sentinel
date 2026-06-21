import pathlib
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"


def compile_and_run(source: str) -> str:
    build_dir = ROOT / ".tmp-tests"
    build_dir.mkdir(exist_ok=True)
    source_path = build_dir / "ambient_console_shell_test.cpp"
    binary_path = build_dir / "ambient_console_shell_test"
    source_path.write_text(source, encoding="utf-8")
    cp = subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(SRC_DIR), str(source_path), "-o", str(binary_path)],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    if cp.returncode != 0:
        raise AssertionError(cp.stdout)
    run = subprocess.run([str(binary_path)], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
    if run.returncode != 0:
        raise AssertionError(run.stdout)
    return run.stdout


def test_ambient_console_shell_page_registry_keeps_enabled_pages_in_order_and_hides_disabled_modules():
    output = compile_and_run(textwrap.dedent(r'''
        #include <iostream>
        #include "ambient-console-page-registry.h"

        static int assert_page(const AmbientConsolePageRegistry &registry, uint8_t index, AmbientConsolePageId expected) {
          const AmbientConsolePage *page = ambientConsolePageAt(registry, index);
          if (!page) return 1;
          if (page->id != expected) return 2;
          if (page->index != index) return 3;
          return 0;
        }

        int main() {
          AmbientConsolePageAvailability availability{};
          availability.nutEnabled = true;
          availability.proxmoxEnabled = true;
          availability.proxmoxImplemented = true;

          AmbientConsolePageRegistry registry = ambientConsoleBuildPageRegistry(availability);
          if (registry.pageCount != 2) return 10;
          if (assert_page(registry, 0, AMBIENT_CONSOLE_PAGE_NUT) != 0) return 11;
          if (assert_page(registry, 1, AMBIENT_CONSOLE_PAGE_PROXMOX) != 0) return 12;
          if (ambientConsolePageIndex(registry, AMBIENT_CONSOLE_PAGE_NUT) != 0) return 13;
          if (ambientConsolePageIndex(registry, AMBIENT_CONSOLE_PAGE_PROXMOX) != 1) return 14;
          if (ambientConsoleClampPageIndex(registry, 7) != 0) return 15;
          if (ambientConsoleClampPageIndex(registry, 1) != 1) return 16;

          availability.proxmoxEnabled = false;
          registry = ambientConsoleBuildPageRegistry(availability);
          if (registry.pageCount != 1) return 20;
          if (assert_page(registry, 0, AMBIENT_CONSOLE_PAGE_NUT) != 0) return 21;
          if (ambientConsolePageIndex(registry, AMBIENT_CONSOLE_PAGE_PROXMOX) != kAmbientConsolePageMissing) return 22;
          if (ambientConsolePageAt(registry, 1) != nullptr) return 23;

          availability.proxmoxEnabled = true;
          availability.proxmoxImplemented = false;
          registry = ambientConsoleBuildPageRegistry(availability);
          if (registry.pageCount != 1) return 30;
          if (assert_page(registry, 0, AMBIENT_CONSOLE_PAGE_NUT) != 0) return 31;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_ambient_console_shell_auto_page_focus_follows_condition_changes_and_manual_override():
    output = compile_and_run(textwrap.dedent(r'''
        #include <iostream>
        #include "ambient-console-shell.h"

        static SummaryState state_with_conditions(const char *nutCondition, const char *proxmoxCondition) {
          SummaryState state{};
          ambientConsoleSafeCopy(state.nut.condition, sizeof(state.nut.condition), nutCondition);
          state.proxmox.enabled = true;
          state.proxmox.implemented = true;
          ambientConsoleSafeCopy(state.proxmox.condition, sizeof(state.proxmox.condition), proxmoxCondition);
          return state;
        }

        int main() {
          AmbientConsoleShell shell{};
          shell.autoPageFocusCooldownMs = 60000UL;

          SummaryState state = state_with_conditions("healthy", "healthy");
          if (shell.applyAutoPageFocus(state, 1000UL)) return 1;
          if (shell.currentPageIndex != 0) return 2;

          state = state_with_conditions("healthy", "warning");
          if (!shell.applyAutoPageFocus(state, 2000UL)) return 3;
          if (shell.currentPageIndex != 1) return 4;
          if (!shell.autoPageFocusActive) return 5;
          if (shell.autoPageFocusReturnPageIndex != 0) return 6;

          if (shell.applyAutoPageFocus(state, 70000UL)) return 7;
          if (shell.currentPageIndex != 1) return 8;

          shell.currentPageIndex = 0;
          shell.cancelAutoPageFocus(71000UL);
          if (shell.applyAutoPageFocus(state, 132000UL)) return 9;
          if (shell.currentPageIndex != 0) return 10;

          state = state_with_conditions("healthy", "critical");
          if (!shell.applyAutoPageFocus(state, 132001UL)) return 11;
          if (shell.currentPageIndex != 1) return 12;

          state = state_with_conditions("healthy", "healthy");
          if (!shell.applyAutoPageFocus(state, 133000UL)) return 13;
          if (shell.currentPageIndex != 0) return 14;
          if (shell.autoPageFocusActive) return 15;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_ambient_console_shell_auto_page_focus_uses_worst_condition_and_delays_during_cooldown():
    output = compile_and_run(textwrap.dedent(r'''
        #include <iostream>
        #include "ambient-console-shell.h"

        static SummaryState state_with_conditions(const char *nutCondition, const char *proxmoxCondition) {
          SummaryState state{};
          ambientConsoleSafeCopy(state.nut.condition, sizeof(state.nut.condition), nutCondition);
          state.proxmox.enabled = true;
          state.proxmox.implemented = true;
          ambientConsoleSafeCopy(state.proxmox.condition, sizeof(state.proxmox.condition), proxmoxCondition);
          return state;
        }

        int main() {
          AmbientConsoleShell shell{};
          shell.autoPageFocusCooldownMs = 60000UL;

          SummaryState state = state_with_conditions("healthy", "warning");
          if (!shell.applyAutoPageFocus(state, 1000UL)) return 1;
          if (shell.currentPageIndex != 1) return 2;

          state = state_with_conditions("warning", "warning");
          if (shell.applyAutoPageFocus(state, 2000UL)) return 12;
          if (shell.currentPageIndex != 1) return 13;

          if (!shell.applyAutoPageFocus(state, 61000UL)) return 14;
          if (shell.currentPageIndex != 0) return 15;

          state = state_with_conditions("healthy", "warning");
          if (shell.applyAutoPageFocus(state, 62000UL)) return 16;
          if (shell.currentPageIndex != 0) return 17;

          if (!shell.applyAutoPageFocus(state, 121000UL)) return 18;
          if (shell.currentPageIndex != 1) return 19;

          state = state_with_conditions("critical", "warning");
          if (shell.applyAutoPageFocus(state, 122000UL)) return 3;
          if (shell.currentPageIndex != 1) return 4;

          if (!shell.applyAutoPageFocus(state, 181000UL)) return 5;
          if (shell.currentPageIndex != 0) return 6;
          if (shell.autoPageFocusReturnPageIndex != 0) return 7;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

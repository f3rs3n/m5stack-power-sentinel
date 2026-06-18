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

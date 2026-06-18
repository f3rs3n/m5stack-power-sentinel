import pathlib
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"


def compile_and_run(source: str) -> str:
    build_dir = ROOT / ".tmp-tests"
    build_dir.mkdir(exist_ok=True)
    (build_dir / "Arduino.h").write_text("#pragma once\n#include <stdint.h>\n", encoding="utf-8")
    (build_dir / "ArduinoJson.h").write_text(textwrap.dedent('''
        #pragma once
        struct JsonVariantConst {
          bool isNull() const { return true; }
          template <typename T> bool is() const { return false; }
          template <typename T> T as() const { return T(); }
        };
    '''), encoding="utf-8")
    source_path = build_dir / "ambient_console_module_pages_test.cpp"
    binary_path = build_dir / "ambient_console_module_pages_test"
    source_path.write_text(source, encoding="utf-8")
    cp = subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(build_dir), "-I", str(SRC_DIR), str(source_path), "-o", str(binary_path)],
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


def test_module_pages_own_summary_mapping_and_render_policy_contracts():
    output = compile_and_run(textwrap.dedent(r'''
        #include <iostream>
        #include <string.h>
        #include "ambient-console-nut-page.h"
        #include "ambient-console-proxmox-page.h"

        void createLedcardsInterfaceUi(const LedcardsInterfaceNutView &) {}
        void updateLedcardsInterfaceUi(const LedcardsInterfaceNutView &) {}
        bool ledcardsInterfacePageTransitionActive() { return false; }
        bool transitionLedcardsInterfacePageUi(uint8_t, uint8_t, const LedcardsInterfaceNutView &, const ProxmoxAmbientView &) { return false; }
        void renderProxmoxAmbientUnavailableUi(const ProxmoxAmbientView &, const LedcardsInterfaceNutView &) {}
        void renderProxmoxAmbientUi(const ProxmoxAmbientView &, const LedcardsInterfaceNutView &) {}

        int main() {
          SummaryState state{};
          state.ups.available = true;
          state.ups.batteryPercent = 97;
          state.ups.runtimeSeconds = 3600;
          state.ups.loadPercent = 18;
          state.ups.inputVoltage = 232.0f;
          state.nut.clientCount = 2;
          state.moduleLanConnected = true;
          state.lastGoodMillis = 1000;
          ambientConsoleSafeCopy(state.moduleTimeHhmm, sizeof(state.moduleTimeHhmm), "09:41");
          ambientConsoleSafeCopy(state.transportStatus, sizeof(state.transportStatus), "live");
          state.proxmox.enabled = true;
          state.proxmox.implemented = true;
          state.proxmox.hasLiveData = true;
          ambientConsoleSafeCopy(state.proxmox.condition, sizeof(state.proxmox.condition), "healthy");
          ambientConsoleSafeCopy(state.proxmox.status, sizeof(state.proxmox.status), "observed");
          state.proxmox.cpuPercent = 11;
          state.proxmox.ramPercent = 42;
          state.proxmox.guestRunning = 0;
          state.proxmox.guestTotal = 0;
          state.proxmox.storagePercent = 55;
          state.proxmox.networkPercent = 7;

          AmbientConsoleNutPage nutPage{};
          AmbientConsoleProxmoxPage proxmoxPage{};
          LedcardsInterfaceNutView nutView = nutPage.makeView(state, true, 88, true, 1200, 2, 1, 10000UL);
          if (nutView.pageCount != 2 || nutView.pageIndex != 1) return 10;
          if (!nutView.linkOk || strcmp(nutView.moduleTimeHhmm, "09:41") != 0) return 11;
          if (nutView.batteryPercent != 97 || nutView.nutClientCount != 2) return 12;

          ProxmoxAmbientView proxmoxView = proxmoxPage.makeView(state);
          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(proxmoxView);
          if (!proxmoxView.enabled || !proxmoxView.implemented || !proxmoxView.hasLiveData) return 20;
          if (model.cardCount != 5) return 21;
          if (strcmp(model.cards[0].label, "CPU") != 0) return 22;
          if (strcmp(model.cards[1].label, "RAM") != 0) return 23;
          if (strcmp(model.cards[2].label, "Guests") != 0) return 24;
          if (strcmp(model.cards[3].label, "Storage") != 0) return 25;
          if (strcmp(model.cards[4].label, "Network") != 0) return 26;

          nutPage.render(nutView);
          proxmoxPage.render(proxmoxView, nutView);
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

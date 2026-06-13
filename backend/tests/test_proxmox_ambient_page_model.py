import pathlib
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"


def compile_and_run(source: str) -> str:
    build_dir = ROOT / ".tmp-tests"
    build_dir.mkdir(exist_ok=True)
    source_path = build_dir / "proxmox_ambient_page_model_test.cpp"
    binary_path = build_dir / "proxmox_ambient_page_model_test"
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


def test_proxmox_ambient_page_model_unconfigured_and_api_unavailable_states():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        static int assert_unconfigured() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "unavailable");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "unconfigured");
          proxmoxAmbientCopy(view.signalKind, sizeof(view.signalKind), "unconfigured");
          proxmoxAmbientCopy(view.signalSummary, sizeof(view.signalSummary), "Proxmox config missing");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.condition, "unavailable") != 0) return 101;
          if (std::strcmp(model.telemetryState, "unconfigured") != 0) return 102;
          if (std::strcmp(model.heroTitle, "PROXMOX") != 0) return 103;
          if (std::strcmp(model.heroValue, "SETUP") != 0) return 104;
          if (std::strcmp(model.heroDetail, "Config missing") != 0) return 105;
          if (std::strcmp(model.visualClass, "purple") != 0) return 106;
          if (model.cardCount != 3) return 107;
          if (std::strcmp(model.cards[0].label, "API") != 0) return 108;
          if (std::strcmp(model.cards[0].value, "--") != 0) return 109;
          if (std::strcmp(model.cards[0].stateText, "UNCONFIGURED") != 0) return 110;
          if (std::strcmp(model.cards[1].label, "Nodes") != 0) return 111;
          if (std::strcmp(model.cards[1].value, "--") != 0) return 112;
          if (std::strcmp(model.cards[2].label, "Guests") != 0) return 113;
          if (std::strcmp(model.cards[2].value, "--") != 0) return 114;
          return 0;
        }

        static int assert_api_unavailable() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "unavailable");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "api_unavailable");
          proxmoxAmbientCopy(view.signalKind, sizeof(view.signalKind), "api_unavailable");
          proxmoxAmbientCopy(view.signalSummary, sizeof(view.signalSummary), "API auth failed");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.condition, "unavailable") != 0) return 201;
          if (std::strcmp(model.telemetryState, "api_unavailable") != 0) return 202;
          if (std::strcmp(model.heroValue, "OFFLINE") != 0) return 203;
          if (std::strcmp(model.heroDetail, "API unavailable") != 0) return 204;
          if (std::strcmp(model.visualClass, "purple") != 0) return 205;
          if (std::strcmp(model.cards[0].stateText, "API UNAVAILABLE") != 0) return 206;
          return 0;
        }

        int main() {
          int result = assert_unconfigured();
          if (result != 0) return result;
          result = assert_api_unavailable();
          if (result != 0) return result;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_missing_live_data_never_shows_fake_telemetry():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "unavailable");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "not_observed");
          proxmoxAmbientCopy(view.signalKind, sizeof(view.signalKind), "api_unavailable");
          view.nodeCount = 99;
          view.onlineNodeCount = 88;
          view.watchedGuestCount = 77;
          view.runningWatchedGuestCount = 66;
          view.hasLiveData = false;

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.telemetryState, "not_observed") != 0) return 1;
          if (std::strcmp(model.heroValue, "WAIT") != 0) return 2;
          if (std::strcmp(model.cards[1].value, "--") != 0) return 3;
          if (std::strcmp(model.cards[2].value, "--") != 0) return 4;
          if (std::strstr(model.cards[1].value, "88") != nullptr) return 5;
          if (std::strstr(model.cards[2].value, "66") != nullptr) return 6;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_healthy_observed_summary_cards():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "healthy");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          view.nodeCount = 2;
          view.onlineNodeCount = 2;
          view.watchedGuestCount = 2;
          view.runningWatchedGuestCount = 2;
          view.storageCount = 3;
          view.maxStorageUsedPercent = 64;

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.condition, "healthy") != 0) return 1;
          if (std::strcmp(model.telemetryState, "observed") != 0) return 2;
          if (std::strcmp(model.heroTitle, "PROXMOX") != 0) return 3;
          if (std::strcmp(model.heroValue, "OK") != 0) return 4;
          if (std::strcmp(model.heroDetail, "Read-only API") != 0) return 5;
          if (std::strcmp(model.visualClass, "green") != 0) return 6;
          if (model.cardCount != 4) return 7;
          if (std::strcmp(model.cards[0].label, "API") != 0) return 8;
          if (std::strcmp(model.cards[0].value, "OK") != 0) return 9;
          if (std::strcmp(model.cards[0].stateText, "OBSERVED") != 0) return 10;
          if (std::strcmp(model.cards[1].label, "Nodes") != 0) return 11;
          if (std::strcmp(model.cards[1].value, "2/2") != 0) return 12;
          if (std::strcmp(model.cards[1].stateText, "ONLINE") != 0) return 13;
          if (std::strcmp(model.cards[2].label, "Guests") != 0) return 14;
          if (std::strcmp(model.cards[2].value, "2/2") != 0) return 15;
          if (std::strcmp(model.cards[2].stateText, "RUNNING") != 0) return 16;
          if (std::strcmp(model.cards[3].label, "Storage") != 0) return 17;
          if (std::strcmp(model.cards[3].value, "64") != 0) return 18;
          if (std::strcmp(model.cards[3].unit, "% max") != 0) return 19;
          if (std::strcmp(model.cards[3].stateText, "OK") != 0) return 20;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_privileges_critical_node_signals():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        static ProxmoxAmbientView base_view(const char *kind) {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "critical");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.signalKind, sizeof(view.signalKind), kind);
          proxmoxAmbientCopy(view.signalSummary, sizeof(view.signalSummary), "Proxmox node pve-b is offline");
          proxmoxAmbientCopy(view.signalContext, sizeof(view.signalContext), "pve-b");
          view.nodeCount = 3;
          view.onlineNodeCount = 2;
          view.watchedGuestCount = 2;
          view.runningWatchedGuestCount = 2;
          view.storageCount = 2;
          view.maxStorageUsedPercent = 64;
          return view;
        }

        int main() {
          ProxmoxAmbientPageModel down = makeProxmoxAmbientPageModel(base_view("node_down"));
          if (std::strcmp(down.heroValue, "NODE DOWN") != 0) return 1;
          if (std::strcmp(down.heroDetail, "pve-b") != 0) return 2;
          if (std::strcmp(down.visualClass, "red") != 0) return 3;
          if (std::strcmp(down.cards[1].stateText, "NODE DOWN") != 0) return 4;
          if (std::strcmp(down.cards[1].visualClass, "red") != 0) return 5;

          ProxmoxAmbientView degradedView = base_view("node_degraded");
          proxmoxAmbientCopy(degradedView.signalSummary, sizeof(degradedView.signalSummary), "Proxmox node pve-c is unknown");
          proxmoxAmbientCopy(degradedView.signalContext, sizeof(degradedView.signalContext), "pve-c");
          ProxmoxAmbientPageModel degraded = makeProxmoxAmbientPageModel(degradedView);
          if (std::strcmp(degraded.heroValue, "NODE DEG") != 0) return 6;
          if (std::strcmp(degraded.heroDetail, "pve-c") != 0) return 7;
          if (std::strcmp(degraded.cards[1].stateText, "DEGRADED") != 0) return 8;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_privileges_watched_guest_down_context():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "critical");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.signalKind, sizeof(view.signalKind), "watched_guest_down");
          proxmoxAmbientCopy(view.signalSummary, sizeof(view.signalSummary), "Watched guest 101 is stopped");
          proxmoxAmbientCopy(view.signalContext, sizeof(view.signalContext), "haos 101");
          view.nodeCount = 1;
          view.onlineNodeCount = 1;
          view.watchedGuestCount = 2;
          view.runningWatchedGuestCount = 1;
          view.storageCount = 2;
          view.maxStorageUsedPercent = 64;

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.heroValue, "GUEST DOWN") != 0) return 1;
          if (std::strcmp(model.heroDetail, "haos 101") != 0) return 2;
          if (std::strcmp(model.visualClass, "red") != 0) return 3;
          if (std::strcmp(model.cards[2].stateText, "GUEST DOWN") != 0) return 4;
          if (std::strcmp(model.cards[2].visualClass, "red") != 0) return 5;
          if (std::strcmp(model.cards[1].stateText, "ONLINE") != 0) return 6;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_surfaces_storage_warning_context():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "warning");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.signalKind, sizeof(view.signalKind), "storage_warning");
          proxmoxAmbientCopy(view.signalSummary, sizeof(view.signalSummary), "Proxmox storage pve-a/local is 86% used");
          proxmoxAmbientCopy(view.signalContext, sizeof(view.signalContext), "pve-a/local 86%");
          view.nodeCount = 1;
          view.onlineNodeCount = 1;
          view.watchedGuestCount = 0;
          view.runningWatchedGuestCount = 0;
          view.storageCount = 2;
          view.maxStorageUsedPercent = 86;

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.heroValue, "STOR WARN") != 0) return 1;
          if (std::strcmp(model.heroDetail, "pve-a/local 86%") != 0) return 2;
          if (std::strcmp(model.visualClass, "orange") != 0) return 3;
          if (std::strcmp(model.cards[3].value, "86") != 0) return 4;
          if (std::strcmp(model.cards[3].stateText, "WARNING") != 0) return 5;
          if (std::strcmp(model.cards[3].visualClass, "orange") != 0) return 6;
          if (std::strcmp(model.cards[1].stateText, "ONLINE") != 0) return 7;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_storage_critical_outranks_warning():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "critical");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.signalKind, sizeof(view.signalKind), "storage_critical");
          proxmoxAmbientCopy(view.signalSummary, sizeof(view.signalSummary), "Proxmox storage pve-a/local-lvm is 96% used");
          proxmoxAmbientCopy(view.signalContext, sizeof(view.signalContext), "pve-a/local-lvm 96%");
          view.nodeCount = 1;
          view.onlineNodeCount = 1;
          view.watchedGuestCount = 0;
          view.runningWatchedGuestCount = 0;
          view.storageCount = 2;
          view.maxStorageUsedPercent = 96;

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.heroValue, "STOR CRIT") != 0) return 1;
          if (std::strcmp(model.heroDetail, "pve-a/local-lvm 96%") != 0) return 2;
          if (std::strcmp(model.visualClass, "red") != 0) return 3;
          if (std::strcmp(model.cards[3].stateText, "CRITICAL") != 0) return 4;
          if (std::strcmp(model.cards[3].visualClass, "red") != 0) return 5;
          if (std::strcmp(model.cards[1].visualClass, "green") != 0) return 6;
          if (std::strcmp(model.cards[2].visualClass, "green") != 0) return 7;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

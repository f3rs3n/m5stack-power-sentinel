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


def test_proxmox_ambient_page_model_exposes_five_card_vocabulary_and_cpu_default_hero():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        static int assert_card(const ProxmoxAmbientCard &card, const char *label) {
          if (std::strcmp(card.label, label) != 0) return 1;
          if (std::strcmp(card.value, "--") != 0) return 2;
          if (std::strcmp(card.stateText, "PLACEHOLDER") != 0) return 3;
          if (std::strcmp(card.visualClass, "blue") != 0) return 4;
          return 0;
        }

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "healthy");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (model.cardCount != 5) return 10;
          if (model.heroCardIndex != 0) return 11;
          if (std::strcmp(model.heroTitle, "CPU") != 0) return 12;
          if (std::strcmp(model.heroDisplayValue, "--") != 0) return 13;
          if (std::strcmp(model.heroDetail, "PLACEHOLDER") != 0) return 14;
          if (std::strcmp(model.visualClass, "blue") != 0) return 15;

          const char *labels[5] = {"CPU", "RAM", "Guests", "Storage", "Network"};
          for (int i = 0; i < 5; ++i) {
            int result = assert_card(model.cards[i], labels[i]);
            if (result != 0) return 100 + i * 10 + result;
          }
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_healthy_observed_cards_use_health_positive_green_except_empty_guests():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.cpuPercent = 12;
          view.ramPercent = 34;
          view.guestRunning = 2;
          view.guestTotal = 2;
          view.storagePercent = 40;
          view.networkPercent = 5;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "healthy");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.cpuCondition, sizeof(view.cpuCondition), "healthy");
          proxmoxAmbientCopy(view.ramCondition, sizeof(view.ramCondition), "healthy");
          proxmoxAmbientCopy(view.guestCondition, sizeof(view.guestCondition), "healthy");
          proxmoxAmbientCopy(view.storageCondition, sizeof(view.storageCondition), "healthy");
          proxmoxAmbientCopy(view.networkCondition, sizeof(view.networkCondition), "healthy");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);
          for (int i = 0; i < 5; ++i) {
            if (std::strcmp(model.cards[i].stateClass, "healthy") != 0) return 1 + i;
            if (std::strcmp(model.cards[i].visualClass, "green") != 0) return 10 + i;
          }

          view.guestRunning = 0;
          view.guestTotal = 0;
          model = makeProxmoxAmbientPageModel(view);
          if (std::strcmp(model.cards[2].stateClass, "healthy") != 0) return 20;
          if (std::strcmp(model.cards[2].visualClass, "blue") != 0) return 21;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_legacy_api_nodes_signal_vocabulary_is_absent():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "unavailable");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "api_unavailable");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);
          for (int i = 0; i < model.cardCount; ++i) {
            if (std::strcmp(model.cards[i].label, "API") == 0) return 1;
            if (std::strcmp(model.cards[i].label, "Nodes") == 0) return 2;
            if (std::strcmp(model.cards[i].label, "Signal") == 0) return 3;
            if (std::strstr(model.cards[i].stateText, "WATCHED") != nullptr) return 4;
          }
          if (std::strcmp(model.heroTitle, "PROXMOX") == 0) return 5;
          if (std::strcmp(model.heroValue, "OFFLINE") == 0) return 6;
          if (std::strcmp(model.heroDetail, "API unavailable") == 0) return 7;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_enabled_unconfigured_stays_visible_without_fake_telemetry():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = false;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "unavailable");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "unconfigured");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);
          if (std::strcmp(model.condition, "unavailable") != 0) return 1;
          if (std::strcmp(model.telemetryState, "unconfigured") != 0) return 2;
          if (model.cardCount != 5) return 3;
          if (std::strcmp(model.heroDisplayValue, "--") != 0) return 4;
          if (std::strcmp(model.heroDetail, "UNCONFIGURED") != 0) return 5;
          if (std::strcmp(model.visualClass, "purple") != 0) return 6;
          for (int i = 0; i < model.cardCount; ++i) {
            if (std::strcmp(model.cards[i].value, "--") != 0) return 10 + i;
            if (std::strcmp(model.cards[i].stateText, "UNCONFIGURED") != 0) return 20 + i;
            if (std::strcmp(model.cards[i].stateClass, "unavailable") != 0) return 30 + i;
            if (std::strcmp(model.cards[i].visualClass, "purple") != 0) return 40 + i;
          }
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_enabled_api_unavailable_stays_visible_without_fake_telemetry():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = false;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "unavailable");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "api_unavailable");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);
          if (std::strcmp(model.telemetryState, "api_unavailable") != 0) return 1;
          if (std::strcmp(model.heroDisplayValue, "--") != 0) return 2;
          if (std::strcmp(model.heroDetail, "UNAVAILABLE") != 0) return 3;
          if (std::strcmp(model.visualClass, "purple") != 0) return 4;
          for (int i = 0; i < model.cardCount; ++i) {
            if (std::strcmp(model.cards[i].value, "--") != 0) return 10 + i;
            if (std::strcmp(model.cards[i].stateText, "UNAVAILABLE") != 0) return 20 + i;
            if (std::strcmp(model.cards[i].stateClass, "unavailable") != 0) return 30 + i;
            if (std::strcmp(model.cards[i].visualClass, "purple") != 0) return 40 + i;
          }
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_renders_reduced_cards_without_duplicate_hero():
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

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (proxmoxReducedCardCount(model) != 4) return 1;
          const char *labels[4] = {"RAM", "Guests", "Storage", "Network"};
          for (uint8_t i = 0; i < 4; ++i) {
            const ProxmoxAmbientCard *card = proxmoxReducedCardAt(model, i);
            if (!card) return 10 + i;
            if (std::strcmp(card->label, labels[i]) != 0) return 20 + i;
          }
          if (proxmoxReducedCardAt(model, 4) != nullptr) return 30;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_cpu_card_uses_latest_percent_and_condition():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.cpuPercent = 90;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "warning");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.cpuCondition, sizeof(view.cpuCondition), "warning");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (model.heroCardIndex != 0) return 1;
          if (std::strcmp(model.heroTitle, "CPU") != 0) return 2;
          if (std::strcmp(model.heroDisplayValue, "90") != 0) return 3;
          if (std::strcmp(model.heroDetail, "CPU WARN") != 0) return 4;
          if (std::strcmp(model.visualClass, "orange") != 0) return 5;
          if (std::strcmp(model.cards[0].label, "CPU") != 0) return 6;
          if (std::strcmp(model.cards[0].value, "90") != 0) return 7;
          if (std::strcmp(model.cards[0].unit, "%") != 0) return 8;
          if (std::strcmp(model.cards[0].stateText, "CPU WARN") != 0) return 9;
          if (std::strcmp(model.cards[0].stateClass, "warning") != 0) return 10;
          if (std::strcmp(model.cards[0].visualClass, "orange") != 0) return 11;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_ram_card_uses_latest_percent_and_condition():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.ramPercent = 91;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "warning");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.ramCondition, sizeof(view.ramCondition), "warning");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (model.heroCardIndex != 1) return 1;
          if (std::strcmp(model.heroTitle, "RAM") != 0) return 2;
          if (std::strcmp(model.heroDisplayValue, "91") != 0) return 3;
          if (std::strcmp(model.heroDetail, "RAM WARN") != 0) return 4;
          if (std::strcmp(model.visualClass, "orange") != 0) return 5;
          if (std::strcmp(model.cards[1].label, "RAM") != 0) return 6;
          if (std::strcmp(model.cards[1].value, "91") != 0) return 7;
          if (std::strcmp(model.cards[1].unit, "%") != 0) return 8;
          if (std::strcmp(model.cards[1].stateText, "RAM WARN") != 0) return 9;
          if (std::strcmp(model.cards[1].stateClass, "warning") != 0) return 10;
          if (std::strcmp(model.cards[1].visualClass, "orange") != 0) return 11;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_cpu_has_priority_over_ram_within_same_tier():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.cpuPercent = 86;
          view.ramPercent = 92;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "warning");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.cpuCondition, sizeof(view.cpuCondition), "warning");
          proxmoxAmbientCopy(view.ramCondition, sizeof(view.ramCondition), "warning");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (model.heroCardIndex != 0) return 1;
          if (std::strcmp(model.heroTitle, "CPU") != 0) return 2;
          if (std::strcmp(model.heroDetail, "CPU WARN") != 0) return 3;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_storage_card_uses_worst_percent_and_condition():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.storagePercent = 96;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "critical");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.storageCondition, sizeof(view.storageCondition), "critical");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (model.heroCardIndex != 3) return 1;
          if (std::strcmp(model.heroTitle, "Storage") != 0) return 2;
          if (std::strcmp(model.heroDisplayValue, "96") != 0) return 3;
          if (std::strcmp(model.heroDetail, "STOR CRIT") != 0) return 4;
          if (std::strcmp(model.visualClass, "red") != 0) return 5;
          if (std::strcmp(model.cards[3].label, "Storage") != 0) return 6;
          if (std::strcmp(model.cards[3].value, "96") != 0) return 7;
          if (std::strcmp(model.cards[3].unit, "%") != 0) return 8;
          if (std::strcmp(model.cards[3].stateText, "STOR CRIT") != 0) return 9;
          if (std::strcmp(model.cards[3].stateClass, "critical") != 0) return 10;
          if (std::strcmp(model.cards[3].visualClass, "red") != 0) return 11;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_storage_outranks_cpu_and_ram_within_same_tier():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.cpuPercent = 90;
          view.ramPercent = 92;
          view.storagePercent = 86;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "warning");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.cpuCondition, sizeof(view.cpuCondition), "warning");
          proxmoxAmbientCopy(view.ramCondition, sizeof(view.ramCondition), "warning");
          proxmoxAmbientCopy(view.storageCondition, sizeof(view.storageCondition), "warning");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (model.heroCardIndex != 3) return 1;
          if (std::strcmp(model.heroTitle, "Storage") != 0) return 2;
          if (std::strcmp(model.heroDetail, "STOR WARN") != 0) return 3;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_guests_card_renders_running_over_total():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.guestRunning = 2;
          view.guestTotal = 3;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "warning");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.guestCondition, sizeof(view.guestCondition), "warning");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.cards[2].label, "Guests") != 0) return 1;
          if (std::strcmp(model.cards[2].value, "2/3") != 0) return 2;
          if (std::strcmp(model.cards[2].unit, "") != 0) return 3;
          if (std::strcmp(model.cards[2].stateText, "GUEST WARN") != 0) return 4;
          if (std::strcmp(model.cards[2].stateClass, "warning") != 0) return 5;
          if (std::strcmp(model.cards[2].visualClass, "orange") != 0) return 6;
          if (model.heroCardIndex != 2) return 7;
          if (std::strcmp(model.heroTitle, "Guests") != 0) return 8;
          if (std::strcmp(model.heroDisplayValue, "2/3") != 0) return 9;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_guests_zero_zero_is_healthy_blue():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.guestRunning = 0;
          view.guestTotal = 0;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "healthy");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.guestCondition, sizeof(view.guestCondition), "healthy");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (std::strcmp(model.cards[2].value, "0/0") != 0) return 1;
          if (std::strcmp(model.cards[2].stateText, "GUEST OK") != 0) return 2;
          if (std::strcmp(model.cards[2].stateClass, "healthy") != 0) return 3;
          if (std::strcmp(model.cards[2].visualClass, "blue") != 0) return 4;
          if (model.heroCardIndex != 0) return 5;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_network_card_uses_saturation_and_condition():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.networkPercent = 85;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "warning");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.networkCondition, sizeof(view.networkCondition), "warning");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (model.heroCardIndex != 4) return 1;
          if (std::strcmp(model.heroTitle, "Network") != 0) return 2;
          if (std::strcmp(model.heroDisplayValue, "85") != 0) return 3;
          if (std::strcmp(model.heroDetail, "NET WARN") != 0) return 4;
          if (std::strcmp(model.cards[4].label, "Network") != 0) return 5;
          if (std::strcmp(model.cards[4].value, "85") != 0) return 6;
          if (std::strcmp(model.cards[4].unit, "%") != 0) return 7;
          if (std::strcmp(model.cards[4].stateText, "NET WARN") != 0) return 8;
          if (std::strcmp(model.cards[4].stateClass, "warning") != 0) return 9;
          if (std::strcmp(model.cards[4].visualClass, "orange") != 0) return 10;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_network_unavailable_promotes_when_no_critical_card_exists():
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
          proxmoxAmbientCopy(view.networkCondition, sizeof(view.networkCondition), "unavailable");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);

          if (model.heroCardIndex != 4) return 1;
          if (std::strcmp(model.heroTitle, "Network") != 0) return 2;
          if (std::strcmp(model.heroDisplayValue, "--") != 0) return 3;
          if (std::strcmp(model.heroDetail, "NET UNAVAIL") != 0) return 4;
          if (std::strcmp(model.cards[4].stateClass, "unavailable") != 0) return 5;
          if (std::strcmp(model.cards[4].visualClass, "purple") != 0) return 6;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_detects_hero_transition_for_card_animation():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        static ProxmoxAmbientView observed_healthy() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.cpuPercent = 12;
          view.ramPercent = 34;
          view.guestRunning = 2;
          view.guestTotal = 2;
          view.storagePercent = 40;
          view.networkPercent = 5;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "healthy");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.cpuCondition, sizeof(view.cpuCondition), "healthy");
          proxmoxAmbientCopy(view.ramCondition, sizeof(view.ramCondition), "healthy");
          proxmoxAmbientCopy(view.guestCondition, sizeof(view.guestCondition), "healthy");
          proxmoxAmbientCopy(view.storageCondition, sizeof(view.storageCondition), "healthy");
          proxmoxAmbientCopy(view.networkCondition, sizeof(view.networkCondition), "healthy");
          return view;
        }

        int main() {
          ProxmoxAmbientPageModel previous = makeProxmoxAmbientPageModel(observed_healthy());
          if (previous.heroCardIndex != 0) return 1;

          ProxmoxAmbientView storageWarning = observed_healthy();
          proxmoxAmbientCopy(storageWarning.condition, sizeof(storageWarning.condition), "warning");
          storageWarning.storagePercent = 88;
          proxmoxAmbientCopy(storageWarning.storageCondition, sizeof(storageWarning.storageCondition), "warning");
          ProxmoxAmbientPageModel next = makeProxmoxAmbientPageModel(storageWarning);

          if (next.heroCardIndex != 3) return 2;
          if (!proxmoxAmbientShouldAnimateHeroTransition(previous, next)) return 3;

          ProxmoxAmbientPageModel sameHero = makeProxmoxAmbientPageModel(storageWarning);
          sameHero.cards[3].value[0] = '8';
          if (proxmoxAmbientShouldAnimateHeroTransition(next, sameHero)) return 4;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_page_model_does_not_animate_unavailable_or_stale_telemetry():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView observed{};
          observed.enabled = true;
          observed.implemented = true;
          observed.hasLiveData = true;
          observed.cpuPercent = 20;
          proxmoxAmbientCopy(observed.condition, sizeof(observed.condition), "healthy");
          proxmoxAmbientCopy(observed.status, sizeof(observed.status), "observed");

          ProxmoxAmbientView unavailable = observed;
          unavailable.hasLiveData = false;
          proxmoxAmbientCopy(unavailable.condition, sizeof(unavailable.condition), "unavailable");
          proxmoxAmbientCopy(unavailable.status, sizeof(unavailable.status), "api_unavailable");
          proxmoxAmbientCopy(unavailable.networkCondition, sizeof(unavailable.networkCondition), "unavailable");

          ProxmoxAmbientView stale = observed;
          stale.hasLiveData = false;
          proxmoxAmbientCopy(stale.condition, sizeof(stale.condition), "stale");
          proxmoxAmbientCopy(stale.status, sizeof(stale.status), "stale");
          proxmoxAmbientCopy(stale.storageCondition, sizeof(stale.storageCondition), "warning");

          ProxmoxAmbientPageModel observedModel = makeProxmoxAmbientPageModel(observed);
          ProxmoxAmbientPageModel unavailableModel = makeProxmoxAmbientPageModel(unavailable);
          ProxmoxAmbientPageModel staleModel = makeProxmoxAmbientPageModel(stale);

          if (proxmoxAmbientShouldAnimateHeroTransition(observedModel, unavailableModel)) return 1;
          if (proxmoxAmbientShouldAnimateHeroTransition(observedModel, staleModel)) return 2;
          if (proxmoxAmbientShouldAnimateHeroTransition(unavailableModel, observedModel)) return 3;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_slot_order_rotates_as_one_ring_when_promoting_cards():
    output = compile_and_run(textwrap.dedent(r'''
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        static int assert_order(const uint8_t actual[5], const uint8_t expected[5]) {
          for (uint8_t i = 0; i < 5; ++i) {
            if (actual[i] != expected[i]) return 1 + i;
          }
          return 0;
        }

        int main() {
          uint8_t order[5] = {0, 1, 2, 3, 4};
          if (!proxmoxAmbientRotateSlotOrderToHero(order, 5, 1)) return 1;
          uint8_t ramHero[5] = {1, 2, 3, 4, 0};
          if (assert_order(order, ramHero) != 0) return 10 + assert_order(order, ramHero);

          if (!proxmoxAmbientRotateSlotOrderToHero(order, 5, 3)) return 2;
          uint8_t storageHero[5] = {3, 4, 0, 1, 2};
          if (assert_order(order, storageHero) != 0) return 20 + assert_order(order, storageHero);

          if (!proxmoxAmbientRotateSlotOrderToHero(order, 5, 4)) return 3;
          uint8_t networkHero[5] = {4, 0, 1, 2, 3};
          if (assert_order(order, networkHero) != 0) return 30 + assert_order(order, networkHero);

          uint8_t beforeNoop[5] = {4, 0, 1, 2, 3};
          if (proxmoxAmbientRotateSlotOrderToHero(order, 5, 4)) return 4;
          if (assert_order(order, beforeNoop) != 0) return 40 + assert_order(order, beforeNoop);

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_proxmox_ambient_touch_override_promotes_selected_card_then_expires_to_canonical_hero():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "proxmox-ambient-page-model.h"

        int main() {
          ProxmoxAmbientView view{};
          view.enabled = true;
          view.implemented = true;
          view.hasLiveData = true;
          view.cpuPercent = 10;
          view.ramPercent = 20;
          view.guestRunning = 0;
          view.guestTotal = 0;
          view.storagePercent = 55;
          view.networkPercent = 5;
          proxmoxAmbientCopy(view.condition, sizeof(view.condition), "warning");
          proxmoxAmbientCopy(view.status, sizeof(view.status), "observed");
          proxmoxAmbientCopy(view.cpuCondition, sizeof(view.cpuCondition), "healthy");
          proxmoxAmbientCopy(view.ramCondition, sizeof(view.ramCondition), "healthy");
          proxmoxAmbientCopy(view.guestCondition, sizeof(view.guestCondition), "healthy");
          proxmoxAmbientCopy(view.storageCondition, sizeof(view.storageCondition), "warning");
          proxmoxAmbientCopy(view.networkCondition, sizeof(view.networkCondition), "healthy");

          ProxmoxAmbientPageModel model = makeProxmoxAmbientPageModel(view);
          if (model.heroCardIndex != 3) return 1;

          ProxmoxAmbientTouchHeroOverride touch = makeProxmoxAmbientTouchHeroOverride(2, 1000, 60000);
          ProxmoxAmbientHeroPolicyInput input{};
          input.currentHeroCardIndex = model.heroCardIndex;
          input.touchOverrideActive = touch.active;
          input.touchOverrideCardIndex = touch.cardIndex;
          input.touchOverrideUntilMs = touch.untilMs;
          input.nowMillis = 2000;

          ProxmoxAmbientHeroPolicyDecision decision = acceptProxmoxAmbientHeroCard(model, input);
          if (!decision.touchOverrideActive) return 2;
          if (decision.acceptedHeroCardIndex != 2) return 3;
          applyProxmoxAmbientHeroCard(model, decision.acceptedHeroCardIndex);
          if (model.heroCardIndex != 2) return 4;
          if (std::strcmp(model.heroTitle, "Guests") != 0) return 5;
          if (std::strcmp(model.heroDisplayValue, "0/0") != 0) return 6;
          if (std::strcmp(model.visualClass, "blue") != 0) return 7;

          input.currentHeroCardIndex = model.heroCardIndex;
          input.nowMillis = 70001;
          decision = acceptProxmoxAmbientHeroCard(makeProxmoxAmbientPageModel(view), input);
          if (decision.touchOverrideActive) return 8;
          if (decision.acceptedHeroCardIndex != 3) return 9;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

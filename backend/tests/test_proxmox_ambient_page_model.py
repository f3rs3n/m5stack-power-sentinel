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

import pathlib
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"


def compile_and_run(source: str) -> str:
    build_dir = ROOT / ".tmp-tests"
    build_dir.mkdir(exist_ok=True)
    source_path = build_dir / "nut_ambient_page_model_test.cpp"
    binary_path = build_dir / "nut_ambient_page_model_test"
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


def test_nut_ambient_page_model_interprets_healthy_line_power_baseline():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "nut-ambient-page-model.h"

        int main() {
          LedcardsInterfaceNutView view{};
          view.offline = false;
          view.upsAvailable = true;
          view.upsStale = false;
          view.onBattery = false;
          view.lowBattery = false;
          view.charging = true;
          view.batteryPercent = 97;
          view.runtimeSeconds = 2310;
          view.loadPercent = 18;
          view.inputVoltage = 232.0f;
          view.nutClientCount = 2;

          NutAmbientPageModel model = makeNutAmbientPageModel(view);

          if (std::strcmp(model.condition, "healthy") != 0) return 1;
          if (model.cardCount != 5) return 2;
          if (model.heroMetric != NUT_AMBIENT_METRIC_BATTERY) return 3;
          if (std::strcmp(model.cards[0].metricId, "battery") != 0) return 4;
          if (std::strcmp(model.cards[0].label, "Battery") != 0) return 5;
          if (std::strcmp(model.cards[0].value, "97") != 0) return 6;
          if (std::strcmp(model.cards[0].unit, "%") != 0) return 7;
          if (std::strcmp(model.cards[0].stateText, "FULL") != 0) return 8;
          if (std::strcmp(model.cards[0].stateClass, "healthy") != 0) return 9;
          if (std::strcmp(model.cards[1].metricId, "runtime") != 0) return 10;
          if (std::strcmp(model.cards[2].metricId, "load") != 0) return 11;
          if (std::strcmp(model.cards[3].metricId, "input") != 0) return 12;
          if (std::strcmp(model.cards[4].metricId, "nut") != 0) return 13;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_nut_ambient_page_model_maps_power_warning_and_shutdown_states():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "nut-ambient-page-model.h"

        static LedcardsInterfaceNutView baseline() {
          LedcardsInterfaceNutView view{};
          view.offline = false;
          view.upsAvailable = true;
          view.upsStale = false;
          view.onBattery = false;
          view.lowBattery = false;
          view.charging = false;
          view.batteryPercent = 84;
          view.runtimeSeconds = 880;
          view.loadPercent = 22;
          view.inputVoltage = 231.0f;
          view.nutClientCount = 2;
          return view;
        }

        static int assert_on_battery_warning() {
          LedcardsInterfaceNutView view = baseline();
          view.onBattery = true;
          view.inputVoltage = 0.0f;

          NutAmbientPageModel model = makeNutAmbientPageModel(view);

          if (std::strcmp(model.condition, "warning") != 0) return 101;
          if (model.shutdownRelevant) return 102;
          if (model.heroMetric != NUT_AMBIENT_METRIC_RUNTIME) return 103;
          if (std::strcmp(model.cards[0].stateText, "ON BATTERY") != 0) return 104;
          if (std::strcmp(model.cards[0].stateClass, "warning") != 0) return 105;
          return 0;
        }

        static int assert_ob_lb_critical_shutdown_relevant() {
          LedcardsInterfaceNutView view = baseline();
          view.onBattery = true;
          view.lowBattery = true;
          view.batteryPercent = 8;
          view.runtimeSeconds = 90;
          view.inputVoltage = 0.0f;

          NutAmbientPageModel model = makeNutAmbientPageModel(view);

          if (std::strcmp(model.condition, "critical") != 0) return 201;
          if (!model.shutdownRelevant) return 202;
          if (model.heroMetric != NUT_AMBIENT_METRIC_RUNTIME) return 203;
          if (std::strcmp(model.cards[0].stateText, "CRITICAL BATTERY") != 0) return 204;
          if (std::strcmp(model.cards[0].stateClass, "critical") != 0) return 205;
          return 0;
        }

        static int assert_online_low_battery_warning_no_shutdown() {
          LedcardsInterfaceNutView view = baseline();
          view.onBattery = false;
          view.lowBattery = true;
          view.charging = true;
          view.batteryPercent = 14;
          view.runtimeSeconds = 1200;
          view.inputVoltage = 231.0f;

          NutAmbientPageModel model = makeNutAmbientPageModel(view);

          if (std::strcmp(model.condition, "warning") != 0) return 301;
          if (model.shutdownRelevant) return 302;
          if (model.heroMetric != NUT_AMBIENT_METRIC_BATTERY) return 303;
          if (std::strcmp(model.cards[0].stateText, "CHARGING") != 0) return 304;
          if (std::strcmp(model.cards[0].stateClass, "warning") != 0) return 305;
          return 0;
        }

        int main() {
          int result = assert_on_battery_warning();
          if (result != 0) return result;
          result = assert_ob_lb_critical_shutdown_relevant();
          if (result != 0) return result;
          result = assert_online_low_battery_warning_no_shutdown();
          if (result != 0) return result;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

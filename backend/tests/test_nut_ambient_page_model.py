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

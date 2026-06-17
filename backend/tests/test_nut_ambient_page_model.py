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
          if (std::strcmp(model.cards[0].visualClass, "green") != 0) return 10;
          if (std::strcmp(model.cards[1].metricId, "runtime") != 0) return 11;
          if (std::strcmp(model.cards[1].value, "38:30") != 0) return 12;
          if (std::strcmp(model.cards[1].compactValue, "39") != 0) return 13;
          if (std::strcmp(model.cards[1].compactUnit, "m") != 0) return 14;
          if (std::strcmp(model.cards[1].stateText, "RESERVE") != 0) return 15;
          if (std::strcmp(model.cards[1].visualClass, "blue") != 0) return 16;
          if (std::strcmp(model.cards[2].metricId, "load") != 0) return 17;
          if (std::strcmp(model.cards[2].visualClass, "green") != 0) return 18;
          if (std::strcmp(model.cards[3].metricId, "input") != 0) return 19;
          if (std::strcmp(model.cards[3].visualClass, "green") != 0) return 20;
          if (std::strcmp(model.cards[4].metricId, "nut") != 0) return 21;
          if (std::strcmp(model.cards[4].visualClass, "blue") != 0) return 22;
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
          view.batteryPercent = 97;
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
          if (std::strcmp(model.cards[0].stateText, "LOW BATTERY") != 0) return 304;
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


def test_nut_ambient_page_model_owns_hero_priority_and_touch_override_policy():
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
          view.charging = true;
          view.batteryPercent = 97;
          view.runtimeSeconds = 1440;
          view.loadPercent = 24;
          view.inputVoltage = 230.0f;
          view.nutClientCount = 2;
          view.nowMillis = 1000;
          return view;
        }

        static int assert_hero_priority_states() {
          LedcardsInterfaceNutView view = baseline();
          if (makeNutAmbientPageModel(view).heroMetric != NUT_AMBIENT_METRIC_BATTERY) return 101;

          view = baseline();
          view.upsAvailable = false;
          if (makeNutAmbientPageModel(view).heroMetric != NUT_AMBIENT_METRIC_NUT) return 102;

          view = baseline();
          view.upsStale = true;
          if (makeNutAmbientPageModel(view).heroMetric != NUT_AMBIENT_METRIC_NUT) return 103;

          view = baseline();
          view.onBattery = true;
          view.lowBattery = true;
          view.batteryPercent = 8;
          view.runtimeSeconds = 90;
          view.inputVoltage = 0.0f;
          if (makeNutAmbientPageModel(view).heroMetric != NUT_AMBIENT_METRIC_RUNTIME) return 104;

          view = baseline();
          view.batteryPercent = 14;
          if (makeNutAmbientPageModel(view).heroMetric != NUT_AMBIENT_METRIC_BATTERY) return 105;

          view = baseline();
          view.loadPercent = 82;
          if (makeNutAmbientPageModel(view).heroMetric != NUT_AMBIENT_METRIC_LOAD) return 106;

          view = baseline();
          view.inputVoltage = 205.0f;
          if (makeNutAmbientPageModel(view).heroMetric != NUT_AMBIENT_METRIC_INPUT) return 107;

          return 0;
        }

        static int assert_touch_override_policy() {
          LedcardsInterfaceNutView view = baseline();
          NutAmbientPageModel model = makeNutAmbientPageModel(view);

          NutAmbientTouchHeroOverride touch = makeNutAmbientTouchHeroOverride(NUT_AMBIENT_METRIC_LOAD, 1000, 60000);
          if (!touch.active) return 201;
          if (touch.metric != NUT_AMBIENT_METRIC_LOAD) return 202;
          if (touch.untilMs != 61000) return 203;

          NutAmbientHeroPolicyInput input{};
          input.currentMetric = NUT_AMBIENT_METRIC_BATTERY;
          input.touchOverrideActive = touch.active;
          input.touchOverrideMetric = touch.metric;
          input.touchOverrideUntilMs = touch.untilMs;
          input.lastHeroSwapMs = 0;
          input.nowMillis = 2000;
          input.cooldownMs = 5000;

          NutAmbientHeroPolicyDecision decision = acceptNutAmbientHeroMetric(model, input);
          if (decision.acceptedMetric != NUT_AMBIENT_METRIC_LOAD) return 204;
          if (!decision.touchOverrideActive) return 205;
          if (decision.lastHeroSwapMs != 0) return 206;

          input.currentMetric = NUT_AMBIENT_METRIC_NUT;
          input.touchOverrideActive = true;
          input.touchOverrideMetric = NUT_AMBIENT_METRIC_LOAD;
          input.touchOverrideUntilMs = 1000;
          input.lastHeroSwapMs = 500;
          input.nowMillis = 1000;
          input.cooldownMs = 5000;
          decision = acceptNutAmbientHeroMetric(model, input);
          if (decision.touchOverrideActive) return 207;
          if (decision.acceptedMetric != NUT_AMBIENT_METRIC_NUT) return 208;
          if (decision.lastHeroSwapMs != 500) return 209;

          input.touchOverrideActive = false;
          input.lastHeroSwapMs = 500;
          input.nowMillis = 6000;
          decision = acceptNutAmbientHeroMetric(model, input);
          if (decision.acceptedMetric != NUT_AMBIENT_METRIC_BATTERY) return 210;
          if (decision.lastHeroSwapMs != 6000) return 211;

          return 0;
        }

        int main() {
          int result = assert_hero_priority_states();
          if (result != 0) return result;
          result = assert_touch_override_policy();
          if (result != 0) return result;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_nut_ambient_page_model_maps_line_power_battery_readiness_warnings():
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
          view.runtimeSeconds = 1200;
          view.loadPercent = 20;
          view.inputVoltage = 231.0f;
          view.nutClientCount = 2;
          return view;
        }

        int main() {
          LedcardsInterfaceNutView view = baseline();
          view.charging = true;
          view.batteryPercent = 89;
          NutAmbientPageModel model = makeNutAmbientPageModel(view);
          if (std::strcmp(model.condition, "warning") != 0) return 1;
          if (std::strcmp(model.cards[0].stateText, "CHARGING") != 0) return 2;
          if (std::strcmp(model.cards[0].stateClass, "warning") != 0) return 3;
          if (std::strcmp(model.cards[0].visualClass, "yellow") != 0) return 4;

          view = baseline();
          view.charging = false;
          view.batteryPercent = 89;
          model = makeNutAmbientPageModel(view);
          if (std::strcmp(model.condition, "warning") != 0) return 5;
          if (std::strcmp(model.cards[0].stateText, "NOT READY") != 0) return 6;
          if (std::strcmp(model.cards[0].visualClass, "yellow") != 0) return 7;

          view = baseline();
          view.charging = true;
          view.batteryPercent = 42;
          model = makeNutAmbientPageModel(view);
          if (std::strcmp(model.cards[0].stateText, "CHARGING") != 0) return 8;
          if (std::strcmp(model.cards[0].visualClass, "orange") != 0) return 9;

          view = baseline();
          view.charging = false;
          view.batteryPercent = 14;
          model = makeNutAmbientPageModel(view);
          if (std::strcmp(model.cards[0].stateText, "LOW BATTERY") != 0) return 10;
          if (std::strcmp(model.cards[0].visualClass, "orange") != 0) return 11;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_nut_ambient_page_model_distinguishes_stale_unavailable_and_missing_metrics():
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
          view.charging = true;
          view.batteryPercent = 76;
          view.runtimeSeconds = 1440;
          view.loadPercent = 24;
          view.inputVoltage = 230.0f;
          view.nutClientCount = 2;
          return view;
        }

        static int assert_stale_telemetry_is_not_unavailable() {
          LedcardsInterfaceNutView view = baseline();
          view.upsStale = true;

          NutAmbientPageModel model = makeNutAmbientPageModel(view);

          if (std::strcmp(model.condition, "stale") != 0) return 101;
          if (std::strcmp(model.telemetryState, "stale") != 0) return 102;
          if (model.hasMissingMetrics) return 103;
          if (model.heroMetric != NUT_AMBIENT_METRIC_NUT) return 104;
          if (std::strcmp(model.cards[0].value, "--") != 0) return 105;
          if (std::strcmp(model.cards[0].stateText, "STALE") != 0) return 106;
          if (std::strcmp(model.cards[0].stateClass, "stale") != 0) return 107;
          if (std::strcmp(model.cards[0].visualClass, "gray") != 0) return 108;
          if (std::strcmp(model.cards[4].stateText, "STALE") != 0) return 109;
          if (std::strcmp(model.cards[4].visualClass, "gray") != 0) return 110;
          return 0;
        }

        static int assert_unavailable_telemetry_is_honest() {
          LedcardsInterfaceNutView view = baseline();
          view.upsAvailable = false;
          view.batteryPercent = 99;
          view.runtimeSeconds = 999;
          view.loadPercent = 1;
          view.inputVoltage = 240.0f;
          view.nutClientCount = 9;

          NutAmbientPageModel model = makeNutAmbientPageModel(view);

          if (std::strcmp(model.condition, "unavailable") != 0) return 201;
          if (std::strcmp(model.telemetryState, "unavailable") != 0) return 202;
          if (model.hasMissingMetrics) return 203;
          if (std::strcmp(model.cards[0].value, "--") != 0) return 204;
          if (std::strcmp(model.cards[0].stateText, "UNAVAILABLE") != 0) return 205;
          if (std::strcmp(model.cards[0].stateClass, "unavailable") != 0) return 206;
          if (std::strcmp(model.cards[0].visualClass, "purple") != 0) return 207;
          if (std::strcmp(model.cards[4].value, "--") != 0) return 208;
          if (std::strcmp(model.cards[4].stateText, "UNAVAILABLE") != 0) return 209;
          if (std::strcmp(model.cards[4].visualClass, "purple") != 0) return 210;
          return 0;
        }

        static int assert_missing_metric_values_are_not_healthy() {
          LedcardsInterfaceNutView view = baseline();
          view.batteryPercent = -1;
          view.runtimeSeconds = -1;
          view.loadPercent = -1;
          view.inputVoltage = 0.0f;
          view.nutClientCount = -1;

          NutAmbientPageModel model = makeNutAmbientPageModel(view);

          if (std::strcmp(model.telemetryState, "partial") != 0) return 301;
          if (!model.hasMissingMetrics) return 302;
          if (std::strcmp(model.cards[0].value, "--") != 0) return 303;
          if (std::strcmp(model.cards[0].stateClass, "unavailable") != 0) return 304;
          if (std::strcmp(model.cards[1].value, "--") != 0) return 305;
          if (std::strcmp(model.cards[1].stateClass, "unavailable") != 0) return 306;
          if (std::strcmp(model.cards[2].value, "--") != 0) return 307;
          if (std::strcmp(model.cards[2].stateClass, "unavailable") != 0) return 308;
          if (std::strcmp(model.cards[3].value, "--") != 0) return 309;
          if (std::strcmp(model.cards[3].stateClass, "unavailable") != 0) return 310;
          if (std::strcmp(model.cards[3].visualClass, "gray") != 0) return 313;
          if (std::strcmp(model.cards[4].value, "--") != 0) return 311;
          if (std::strcmp(model.cards[4].stateClass, "unavailable") != 0) return 312;
          if (std::strcmp(model.cards[4].visualClass, "purple") != 0) return 314;
          return 0;
        }

        int main() {
          int result = assert_stale_telemetry_is_not_unavailable();
          if (result != 0) return result;
          result = assert_unavailable_telemetry_is_honest();
          if (result != 0) return result;
          result = assert_missing_metric_values_are_not_healthy();
          if (result != 0) return result;
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

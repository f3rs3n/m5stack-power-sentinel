import pathlib
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"


def compile_and_run(source: str) -> str:
    build_dir = ROOT / ".tmp-tests"
    build_dir.mkdir(exist_ok=True)
    source_path = build_dir / "display_brightness_policy_test.cpp"
    binary_path = build_dir / "display_brightness_policy_test"
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


def test_hardware_aware_curves_expose_physical_backlight_levels_not_nominal_steps():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstdint>
        #include <iostream>
        #include "display-brightness-policy.h"

        static int assert_eq(uint8_t actual, uint8_t expected, int code) {
          if (actual != expected) {
            std::cerr << "expected " << static_cast<int>(expected)
                      << " got " << static_cast<int>(actual)
                      << " code " << code << "\n";
            return code;
          }
          return 0;
        }

        int main() {
          const uint16_t raw[6] = {0, 12, 45, 70, 95, 135};
          const uint8_t dimLevels[6] = {20, 20, 21, 21, 22, 22};
          const uint8_t awakeLevels[6] = {21, 22, 23, 24, 25, 25};
          const uint8_t dimNominal[6] = {15, 15, 46, 46, 78, 78};
          const uint8_t awakeNominal[6] = {46, 78, 110, 142, 174, 174};

          for (uint8_t i = 0; i < 6; ++i) {
            int result = assert_eq(psBacklightLevelForRaw(raw[i], PsBrightnessMode::Dim), dimLevels[i], 10 + i);
            if (result != 0) return result;
            result = assert_eq(psBacklightLevelForRaw(raw[i], PsBrightnessMode::Awake), awakeLevels[i], 20 + i);
            if (result != 0) return result;
            result = assert_eq(psBrightnessForRaw(raw[i], PsBrightnessMode::Dim), dimNominal[i], 30 + i);
            if (result != 0) return result;
            result = assert_eq(psBrightnessForRaw(raw[i], PsBrightnessMode::Awake), awakeNominal[i], 40 + i);
            if (result != 0) return result;
          }

          if (psCoreS3BacklightLevelForBrightness(15) != 20) return 50;
          if (psCoreS3BacklightLevelForBrightness(46) != 21) return 51;
          if (psCoreS3BacklightLevelForBrightness(78) != 22) return 52;
          if (psCoreS3BacklightLevelForBrightness(110) != 23) return 53;
          if (psCoreS3BacklightLevelForBrightness(142) != 24) return 54;
          if (psCoreS3BacklightLevelForBrightness(174) != 25) return 55;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

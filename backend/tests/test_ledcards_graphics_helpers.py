import pathlib
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"


def compile_and_run(source: str) -> str:
    build_dir = ROOT / ".tmp-tests"
    build_dir.mkdir(exist_ok=True)
    source_path = build_dir / "ledcards_graphics_helpers_test.cpp"
    binary_path = build_dir / "ledcards_graphics_helpers_test"
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


def test_ledcards_graphics_helpers_map_visual_classes_and_own_card_text():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "ledcards-graphics.h"

        int main() {
          if (ledcardsVisualClassColor("red") != kLedcardsRed) return 1;
          if (ledcardsVisualClassColor("orange") != kLedcardsOrange) return 2;
          if (ledcardsVisualClassColor("yellow") != kLedcardsYellow) return 3;
          if (ledcardsVisualClassColor("blue") != kLedcardsBlue) return 4;
          if (ledcardsVisualClassColor("purple") != kLedcardsPurple) return 5;
          if (ledcardsVisualClassColor("gray") != kLedcardsGray) return 6;
          if (ledcardsVisualClassColor("green") != kLedcardsGreen) return 7;
          if (ledcardsVisualClassColor("") != kLedcardsGreen) return 8;
          if (ledcardsVisualClassColor(nullptr) != kLedcardsGreen) return 9;

          if (ledcardsStateFill(kLedcardsRed) != 0x200d0c) return 10;
          if (ledcardsStateFill(kLedcardsBlue) != 0x07161d) return 11;
          if (ledcardsStateFill(kLedcardsGreen) != 0x071c12) return 12;
          if (ledcardsStateTextColor(kLedcardsRed) != kLedcardsRedText) return 13;
          if (ledcardsStateTextColor(kLedcardsPurple) != kLedcardsPurpleText) return 14;
          if (ledcardsStateTextColor(kLedcardsGreen) != 0x9bb2a0) return 15;

          LedcardsAmbientCardRender card{};
          ledcardsAmbientCopy(card.value, sizeof(card.value), "12345678901234567890");
          ledcardsAmbientCopy(card.label, sizeof(card.label), "Runtime");
          ledcardsAmbientCopy(card.unit, sizeof(card.unit), "minutes");
          ledcardsAmbientCopy(card.stateText, sizeof(card.stateText), "CRITICAL RUNTIME");

          if (std::strcmp(card.value, "123456789012345") != 0) return 20;
          if (std::strcmp(card.label, "Runtime") != 0) return 21;
          if (std::strcmp(card.unit, "minutes") != 0) return 22;
          if (std::strcmp(card.stateText, "CRITICAL RUNTIME") != 0) return 23;

          const char *source = "Battery";
          ledcardsAmbientCopy(card.label, sizeof(card.label), source);
          source = "Mutated";
          if (std::strcmp(card.label, "Battery") != 0) return 24;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_ledcards_graphics_helpers_expose_stable_ring_slot_geometry():
    output = compile_and_run(textwrap.dedent(r'''
        #include <iostream>
        #include "ledcards-graphics.h"

        static int assert_slot(uint8_t slot, int x, int y) {
          LedcardsRingSlotPosition pos = ledcardsRingSlotPosition(slot);
          if (pos.x != x) return 1;
          if (pos.y != y) return 2;
          return 0;
        }

        int main() {
          if (kLedcardsRingSlotCount != 5) return 1;
          if (assert_slot(0, 43, 57) != 0) return 10 + assert_slot(0, 43, 57);
          if (assert_slot(1, 166, 124) != 0) return 20 + assert_slot(1, 166, 124);
          if (assert_slot(2, 166, 182) != 0) return 30 + assert_slot(2, 166, 182);
          if (assert_slot(3, 12, 182) != 0) return 40 + assert_slot(3, 12, 182);
          if (assert_slot(4, 12, 124) != 0) return 50 + assert_slot(4, 12, 124);
          if (assert_slot(99, 12, 124) != 0) return 60 + assert_slot(99, 12, 124);
          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

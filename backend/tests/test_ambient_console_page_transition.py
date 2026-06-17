import pathlib
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"


def compile_and_run(source: str) -> str:
    build_dir = ROOT / ".tmp-tests"
    build_dir.mkdir(exist_ok=True)
    source_path = build_dir / "ambient_console_page_transition_test.cpp"
    binary_path = build_dir / "ambient_console_page_transition_test"
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


def test_ambient_console_page_transition_tracks_direction_and_coalesces_refreshes():
    output = compile_and_run(textwrap.dedent(r'''
        #include <iostream>
        #include "ambient-console-page-transition.h"

        int main() {
          AmbientConsolePageTransitionState state{};
          state.selectedPage = 0;

          if (ambientConsoleClampSelectedPage(state, 1) != 0) return 1;
          if (state.selectedPage != 0) return 2;

          bool started = ambientConsoleStartPageTransition(state, 1, 2);
          if (!started) return 3;
          if (!state.active) return 4;
          if (state.previousPage != 0) return 5;
          if (state.selectedPage != 1) return 6;
          if (state.direction != AMBIENT_PAGE_TRANSITION_FORWARD) return 7;

          if (ambientConsoleStartPageTransition(state, 1, 2)) return 8;
          if (!ambientConsoleNotePageRefresh(state)) return 9;
          if (!state.pendingRefresh) return 10;
          ambientConsoleFinishPageTransition(state);
          if (state.active) return 11;
          if (state.pendingRefresh) return 12;

          started = ambientConsoleStartPageTransition(state, 0, 2);
          if (!started) return 13;
          if (state.previousPage != 1) return 14;
          if (state.selectedPage != 0) return 15;
          if (state.direction != AMBIENT_PAGE_TRANSITION_REVERSE) return 16;

          state.selectedPage = 4;
          if (ambientConsoleClampSelectedPage(state, 2) != 0) return 17;
          if (state.selectedPage != 0) return 18;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

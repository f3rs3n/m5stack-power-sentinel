#!/usr/bin/env python3
import importlib.util
import pathlib
import subprocess
import sys
import traceback

ROOT = pathlib.Path(__file__).resolve().parents[1]
TEST_DIR = ROOT / "backend" / "tests"


def load_module(path: pathlib.Path):
    spec = importlib.util.spec_from_file_location(path.stem, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    failures = 0
    test_files = sorted(TEST_DIR.glob("test_*.py"))
    if not test_files:
        print(f"No test files found in {TEST_DIR}")
        return 1
    for path in test_files:
        module = load_module(path)
        for name in sorted(n for n in dir(module) if n.startswith("test_")):
            try:
                getattr(module, name)()
                print(f"PASS {path.name}::{name}")
            except Exception as exc:
                failures += 1
                print(f"FAIL {path.name}::{name}: {type(exc).__name__}: {exc}")
                traceback.print_exc(limit=4)
    ui_check = ROOT / "tools" / "check_core_s3_ui.py"
    if ui_check.exists():
        cp = subprocess.run([sys.executable, str(ui_check)], cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, check=False)
        print(cp.stdout.strip())
        if cp.returncode != 0:
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())

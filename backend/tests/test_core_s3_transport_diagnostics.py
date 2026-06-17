import pathlib
import subprocess
import textwrap

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"


def compile_and_run(source: str) -> str:
    build_dir = ROOT / ".tmp-tests"
    build_dir.mkdir(exist_ok=True)
    source_path = build_dir / "core_s3_transport_diagnostics_test.cpp"
    binary_path = build_dir / "core_s3_transport_diagnostics_test"
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


def test_core_s3_transport_diagnostics_preserve_last_good_and_classify_failures_until_recovery():
    output = compile_and_run(textwrap.dedent(r'''
        #include <cstring>
        #include <iostream>
        #include "core-s3-transport-diagnostics.h"

        int main() {
          CoreS3TransportDiagnostics diag{};
          char line[96]{};

          coreS3TransportRecordSuccess(diag, CORE_S3_TRANSPORT_STACKFLOW_SERIAL, 1000);
          if (diag.attemptCount != 1) return 1;
          if (diag.okCount != 1) return 2;
          if (diag.failCount != 0) return 3;
          if (diag.consecutiveFailCount != 0) return 4;
          if (diag.lastGoodMillis != 1000) return 5;
          if (diag.lastGoodSource != CORE_S3_TRANSPORT_STACKFLOW_SERIAL) return 6;
          coreS3TransportFormatStatus(diag, line, sizeof(line));
          if (std::strcmp(line, "StackFlow serial OK") != 0) return 7;

          coreS3TransportRecordFailure(diag, CORE_S3_TRANSPORT_FAILURE_TIMEOUT, 1600, 2);
          coreS3TransportRecordFailure(diag, CORE_S3_TRANSPORT_FAILURE_JSON_PARSE, 2100, 2);
          if (diag.attemptCount != 3) return 8;
          if (diag.okCount != 1) return 9;
          if (diag.failCount != 2) return 10;
          if (diag.consecutiveFailCount != 2) return 11;
          if (diag.lastGoodMillis != 1000) return 12;
          if (diag.lastFailureKind != CORE_S3_TRANSPORT_FAILURE_JSON_PARSE) return 13;
          if (diag.lastFailureMillis != 2100) return 14;
          coreS3TransportFormatStatus(diag, line, sizeof(line));
          if (std::strcmp(line, "StackFlow JSON parse fail x2 last_ok=1100ms") != 0) return 15;

          if (!coreS3TransportShouldLogFailure(diag, 2)) return 16;
          if (coreS3TransportShouldLogFailure(diag, 3)) return 17;

          coreS3TransportRecordSuccess(diag, CORE_S3_TRANSPORT_STACKFLOW_SERIAL, 2400);
          if (diag.recoveredFailureCount != 2) return 18;
          if (diag.consecutiveFailCount != 0) return 19;
          if (!coreS3TransportRecovered(diag)) return 20;
          coreS3TransportFormatStatus(diag, line, sizeof(line));
          if (std::strcmp(line, "StackFlow serial OK recovered x2") != 0) return 21;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"


def test_core_s3_transport_diagnostics_logs_first_success_once_without_spamming_steady_state():
    output = compile_and_run(textwrap.dedent(r'''
        #include <iostream>
        #include "core-s3-transport-diagnostics.h"

        int main() {
          CoreS3TransportDiagnostics diag{};

          coreS3TransportRecordSuccess(diag, CORE_S3_TRANSPORT_STACKFLOW_SERIAL, 1000);
          if (!coreS3TransportShouldLogSuccess(diag)) return 1;

          coreS3TransportRecordSuccess(diag, CORE_S3_TRANSPORT_STACKFLOW_SERIAL, 6000);
          if (coreS3TransportShouldLogSuccess(diag)) return 2;

          coreS3TransportRecordFailure(diag, CORE_S3_TRANSPORT_FAILURE_TIMEOUT, 11000, 3);
          coreS3TransportRecordSuccess(diag, CORE_S3_TRANSPORT_STACKFLOW_SERIAL, 16000);
          if (!coreS3TransportShouldLogSuccess(diag)) return 3;

          std::cout << "ok\n";
          return 0;
        }
    '''))
    assert output == "ok\n"

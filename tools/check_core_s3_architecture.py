#!/usr/bin/env python3
"""Static guard for CoreS3 Ambient Console Shell/Page ownership."""

from __future__ import annotations

import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC_DIR = ROOT / "firmware" / "core-s3-display" / "src"

REQUIRED_SOURCES = {
    "main.cpp": SRC_DIR / "main.cpp",
    "ambient-console-shell.h": SRC_DIR / "ambient-console-shell.h",
    "ambient-console-nut-page.h": SRC_DIR / "ambient-console-nut-page.h",
    "ambient-console-proxmox-page.h": SRC_DIR / "ambient-console-proxmox-page.h",
}

MAIN_FORBIDDEN_POLICY = [
    "bool proxmoxPageAvailable(",
    "uint8_t ambientPageCount(",
    "void clampCurrentPageIndex(",
    "LedcardsInterfaceNutView makeLedcardsInterfaceNutView(",
    "LedcardsInterfaceNutView makeNutPageView(",
    "ProxmoxAmbientView makeProxmoxAmbientView(",
    "ProxmoxAmbientView makeProxmoxPageView(",
    "void refreshLedcardsUi(",
    "bool handleTopBarPageTap(",
    "renderProxmoxAmbientUi(",
    "updateLedcardsInterfaceUi(",
]

SHELL_FORBIDDEN_MODULE_POLICY = [
    "state.ups.",
    "state.ups.batteryPercent",
    "state.nut.",
    "state.moduleLanConnected",
    "state.transportStatus",
    "state.moduleTimeHhmm",
    "state.proxmox.hasLiveData",
    "state.proxmox.condition",
    "state.proxmox.status",
    "state.proxmox.cpuPercent",
    "state.proxmox.cpuCondition",
    "state.proxmox.ramPercent",
    "state.proxmox.ramCondition",
    "state.proxmox.guestRunning",
    "state.proxmox.guestTotal",
    "state.proxmox.guestCondition",
    "state.proxmox.storagePercent",
    "state.proxmox.storageCondition",
    "state.proxmox.networkPercent",
    "state.proxmox.networkCondition",
]

REQUIRED_MAIN_DELEGATION = [
    "AmbientConsoleShell ambientShell",
    "ambientConsoleParseSummary(state",
    "ambientShell.create(state",
    "ambientShell.refresh(state",
    "ambientShell.handleTopBarPageTap(state",
]

REQUIRED_SHELL_DELEGATION = [
    "AmbientConsoleNutPage nutPage",
    "AmbientConsoleProxmoxPage proxmoxPage",
    "nutPage.makeView(state",
    "nutPage.render(view)",
    "proxmoxPage.makeView(state)",
    "proxmoxPage.render(",
]

REQUIRED_PAGE_MAPPING = {
    "ambient-console-nut-page.h": [
        "struct AmbientConsoleNutPage",
        "LedcardsInterfaceNutView makeView(const SummaryState &state",
        "view.batteryPercent = state.ups.batteryPercent",
        "view.nutClientCount = state.nut.clientCount",
        "view.moduleLanConnected = state.moduleLanConnected",
        "updateLedcardsInterfaceUi(view)",
    ],
    "ambient-console-proxmox-page.h": [
        "struct AmbientConsoleProxmoxPage",
        "ProxmoxAmbientView makeView(const SummaryState &state)",
        "view.cpuPercent = state.proxmox.cpuPercent",
        "view.ramPercent = state.proxmox.ramPercent",
        "view.guestRunning = state.proxmox.guestRunning",
        "view.storagePercent = state.proxmox.storagePercent",
        "view.networkPercent = state.proxmox.networkPercent",
        "renderProxmoxAmbientUi(view, statusView)",
    ],
}


def _missing(source_name: str, text: str, needles: list[str]) -> list[str]:
    return [f"{source_name} missing required ownership marker {needle!r}" for needle in needles if needle not in text]


def _contains_forbidden(source_name: str, text: str, needles: list[str]) -> list[str]:
    return [f"{source_name} contains forbidden ownership marker {needle!r}" for needle in needles if needle in text]


def analyze_sources(sources: dict[str, str]) -> list[str]:
    findings: list[str] = []
    for name in REQUIRED_SOURCES:
        if name not in sources:
            findings.append(f"missing source {name}")

    main = sources.get("main.cpp", "")
    shell = sources.get("ambient-console-shell.h", "")
    findings.extend(_missing("main.cpp", main, REQUIRED_MAIN_DELEGATION))
    findings.extend(_contains_forbidden("main.cpp", main, MAIN_FORBIDDEN_POLICY))
    findings.extend(_missing("ambient-console-shell.h", shell, REQUIRED_SHELL_DELEGATION))
    findings.extend(_contains_forbidden("ambient-console-shell.h", shell, SHELL_FORBIDDEN_MODULE_POLICY))

    for source_name, needles in REQUIRED_PAGE_MAPPING.items():
        findings.extend(_missing(source_name, sources.get(source_name, ""), needles))

    return findings


def analyze_repo(root: pathlib.Path = ROOT) -> list[str]:
    src_dir = root / "firmware" / "core-s3-display" / "src"
    paths = {
        "main.cpp": src_dir / "main.cpp",
        "ambient-console-shell.h": src_dir / "ambient-console-shell.h",
        "ambient-console-nut-page.h": src_dir / "ambient-console-nut-page.h",
        "ambient-console-proxmox-page.h": src_dir / "ambient-console-proxmox-page.h",
    }
    sources: dict[str, str] = {}
    findings: list[str] = []
    for name, path in paths.items():
        if not path.exists():
            findings.append(f"missing source {path}")
            sources[name] = ""
            continue
        try:
            sources[name] = path.read_text(encoding="utf-8")
        except OSError as exc:
            findings.append(f"unable to read source {path}: {exc}")
            sources[name] = ""
        except UnicodeDecodeError as exc:
            findings.append(f"unable to decode source {path}: {exc}")
            sources[name] = ""
    return findings + analyze_sources(sources)


def main() -> int:
    findings = analyze_repo(ROOT)
    if findings:
        for finding in findings:
            print(f"FAIL core-s3-architecture: {finding}")
        return 1
    print("PASS core-s3-architecture Shell/Page ownership guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

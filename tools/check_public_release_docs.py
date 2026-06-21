#!/usr/bin/env python3
"""Static guard for public release readiness documentation."""

from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

PUBLIC_DOCS = [
    ROOT / "README.md",
    ROOT / "docs" / "quickstart.md",
    ROOT / "docs" / "hardware.md",
    ROOT / "docs" / "backend-installer.md",
    ROOT / "docs" / "configuration.md",
    ROOT / "docs" / "firmware-flashing.md",
    ROOT / "docs" / "modules" / "nut.md",
    ROOT / "docs" / "modules" / "proxmox.md",
    ROOT / "docs" / "troubleshooting.md",
    ROOT / "docs" / "public-repo-audit.md",
    ROOT / "SECURITY.md",
    ROOT / "LICENSE",
    ROOT / "NOTICE",
]

PUBLIC_PLACEHOLDER_NEEDLES = [
    "First Public Release User",
    "Backend Installer",
    "Firmware Release Path",
    "NUT Monitor",
    "Proxmox Module",
    "Configuration Surface",
    "Public Repo Audit",
]

README_LINKS = [
    "docs/quickstart.md",
    "docs/hardware.md",
    "docs/backend-installer.md",
    "docs/configuration.md",
    "docs/firmware-flashing.md",
    "docs/modules/nut.md",
    "docs/modules/proxmox.md",
    "docs/troubleshooting.md",
    "docs/public-repo-audit.md",
    "SECURITY.md",
    "LICENSE",
    "NOTICE",
]

PRIVATE_PUBLIC_PATTERNS = [
    re.compile(r"192\.168\.2\."),
    re.compile(r"\broot@192\.168\."),
    re.compile(r"/home/martino"),
    re.compile(r"doomtrain", re.IGNORECASE),
    re.compile(r"Le-Case", re.IGNORECASE),
]


def fail(message: str) -> int:
    print(f"FAIL public-release-docs: {message}")
    return 1


def read(path: pathlib.Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except FileNotFoundError:
        raise AssertionError(f"missing {path.relative_to(ROOT)}")


def assert_contains(text: str, needle: str, context: str) -> None:
    if needle not in text:
        raise AssertionError(f"{context} missing {needle!r}")


def assert_public_safe(path: pathlib.Path, text: str) -> None:
    for pattern in PRIVATE_PUBLIC_PATTERNS:
        if pattern.search(text):
            raise AssertionError(f"{path.relative_to(ROOT)} contains private/local marker {pattern.pattern!r}")


def main() -> int:
    try:
        docs = {path: read(path) for path in PUBLIC_DOCS}
        readme = docs[ROOT / "README.md"]
        for needle in README_LINKS:
            assert_contains(readme, needle, "README public navigation")
        for needle in PUBLIC_PLACEHOLDER_NEEDLES:
            assert_contains(readme, needle, "README public vocabulary")
        for phrase in [
            "English-only public documentation",
            "no OTA firmware updates in v1",
            "no Module LLM-mediated flashing in v1",
            "no CoreS3 setup/options page in v1",
            "NUT and upsmon own real shutdown behavior",
        ]:
            assert_contains(readme, phrase, "README limitations")

        quickstart = docs[ROOT / "docs" / "quickstart.md"]
        for phrase in [
            "Backend Installer",
            "Firmware Release Path",
            "scripts/install-power-sentinel.sh --modules nut --dry-run",
            "pio run -e m5stack-cores3",
            "pio device list",
            "api/v1/summary?stackflow_safe=1",
        ]:
            assert_contains(quickstart, phrase, "quickstart")

        firmware = docs[ROOT / "docs" / "firmware-flashing.md"]
        for phrase in [
            "CoreS3 RX=G18",
            "CoreS3 TX=G17",
            "Serial2",
            "115200",
            "pio device list",
            "--upload-port",
            "No OTA",
            "No Module LLM-mediated flashing",
            "tools/core_s3_serial_capture.py",
        ]:
            assert_contains(firmware, phrase, "firmware docs")

        configuration = docs[ROOT / "docs" / "configuration.md"]
        for phrase in [
            "/etc/power-sentinel.json",
            "Configuration Surface",
            "disabled",
            "unconfigured",
            "unavailable",
            "stale",
            "warning",
            "critical",
        ]:
            assert_contains(configuration, phrase, "configuration docs")

        nut = docs[ROOT / "docs" / "modules" / "nut.md"]
        for phrase in [
            "supported default module",
            "OB LB",
            "OL ... LB",
            "upsmon",
            "shutdown remains owned by NUT",
        ]:
            assert_contains(nut, phrase, "NUT docs")

        proxmox = docs[ROOT / "docs" / "modules" / "proxmox.md"]
        for phrase in [
            "supported optional module",
            "API-only",
            "read-only",
            "single-node",
            "token_secret",
            "unconfigured",
            "not_observed",
            "api_unavailable",
            "no SSH",
        ]:
            assert_contains(proxmox, phrase, "Proxmox docs")

        security = docs[ROOT / "SECURITY.md"]
        for phrase in ["Supported scope", "Reporting a vulnerability", "Do not publish secrets"]:
            assert_contains(security, phrase, "SECURITY")

        license_text = docs[ROOT / "LICENSE"]
        for phrase in ["Apache License", "Version 2.0"]:
            assert_contains(license_text, phrase, "LICENSE")
        for forbidden in ["All rights reserved", "No license is currently granted"]:
            if forbidden in license_text:
                raise AssertionError(f"LICENSE still contains placeholder language {forbidden!r}")

        notices = docs[ROOT / "NOTICE"]
        for phrase in ["D-DIN", "Montserrat", "Nerd Fonts", "SIL Open Font License"]:
            assert_contains(notices, phrase, "NOTICE")

        troubleshooting = docs[ROOT / "docs" / "troubleshooting.md"]
        assert_contains(troubleshooting, "Public Repo Audit", "troubleshooting/audit docs")

        backend_installer = docs[ROOT / "docs" / "backend-installer.md"]
        for phrase in ["Backend Installer", "not a CoreS3 firmware flasher", "port `8088`"]:
            assert_contains(backend_installer, phrase, "backend installer docs")

        public_repo_audit = docs[ROOT / "docs" / "public-repo-audit.md"]
        for phrase in ["Public Repo Audit", "Maintainer Operations Notes", "secrets"]:
            assert_contains(public_repo_audit, phrase, "public repo audit docs")

        for path, text in docs.items():
            assert_public_safe(path, text)

        for operations_doc in sorted((ROOT / "docs" / "operations").glob("*.md")):
            ops = operations_doc.read_text(encoding="utf-8")
            context = f"operations {operations_doc.relative_to(ROOT)}"
            assert_contains(ops, "Maintainer Operations Notes", context)
            assert_contains(ops, "not the public install path", context)
    except AssertionError as exc:
        return fail(str(exc))
    print("PASS public-release-docs guard")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

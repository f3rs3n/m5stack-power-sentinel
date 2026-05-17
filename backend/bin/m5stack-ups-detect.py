#!/usr/bin/env python3
import os
import shutil
import subprocess
from datetime import datetime, timezone


def run(title, cmd, timeout=12):
    print(f"\n### {title}")
    print("$ " + " ".join(cmd))
    try:
        cp = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=timeout)
        out = cp.stdout.strip()
        err = cp.stderr.strip()
        if out:
            print(out)
        if err:
            print(err)
        print(f"[exit {cp.returncode}]")
    except FileNotFoundError:
        print("command not found")
    except subprocess.TimeoutExpired:
        print(f"timeout after {timeout}s")


def main():
    print("M5Stack UPS/NUT detect")
    print("timestamp=" + datetime.now(timezone.utc).isoformat())
    print("This script is read-only: it does not start drivers or modify NUT config.")

    run("kernel", ["uname", "-a"])
    run("USB devices", ["lsusb"] if shutil.which("lsusb") else ["true"])
    run("USB bus tree", ["sh", "-c", "find /dev/bus/usb -maxdepth 3 -type c -printf '%p %m %u:%g\\n' 2>/dev/null | sort | sed -n '1,120p'"])
    run("HID/USB kernel log tail", ["sh", "-c", "dmesg | grep -Ei 'usb|hid|ups|battery|power' | tail -n 120"], timeout=8)
    run("NUT scanner USB", ["nut-scanner", "-U"], timeout=30)
    run("NUT installed commands", ["sh", "-c", "for c in upsc upsdrvctl upsd upsmon nut-scanner; do printf '%s=' \"$c\"; command -v \"$c\" || true; done"])
    run("NUT service state", ["sh", "-c", "for s in nut-server nut-monitor nut-driver; do echo -- $s; systemctl is-enabled $s 2>/dev/null || true; systemctl is-active $s 2>/dev/null || true; done"])
    run("NUT config files", ["sh", "-c", "for f in /etc/nut/nut.conf /etc/nut/ups.conf /etc/nut/upsd.conf /etc/nut/upsd.users /etc/nut/upsmon.conf; do echo -- $f; test -f $f && sed -n '1,80p' $f | sed -E 's/(password[[:space:]]*=).*/\\1 [REDACTED]/I' || true; done"])


if __name__ == "__main__":
    main()

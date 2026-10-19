"""Dismiss the "Update Available" dialog MuMu's BitLife shows on launch.

The mod build asks a server whether a newer version exists and blocks the screen with
an Update/Cancel AlertDialog. There is no Play Store in this emulator, so the update can
never be applied -- the dialog is pure obstruction. This helper finds the Cancel button
by its bounds and taps it, and does nothing when no dialog is up.

Usage:  python3 tools/mumu_gate.py [--device 127.0.0.1:16384]

Exit code 0 always: a missing dialog is the normal case.
"""
import argparse
import re
import subprocess
import sys
import xml.etree.ElementTree as ET


def adb(device, *args):
    return subprocess.run(["adb", "-s", device, *args], capture_output=True, text=True)


def find_cancel(xml):
    try:
        root = ET.fromstring(xml)
    except ET.ParseError:
        return None
    for node in root.iter("node"):
        if node.get("text") != "Cancel":
            continue
        bounds = node.get("bounds") or ""
        m = re.match(r"\[(\d+),(\d+)\]\[(\d+),(\d+)\]", bounds)
        if not m:
            continue
        x1, y1, x2, y2 = (int(v) for v in m.groups())
        return ((x1 + x2) // 2, (y1 + y2) // 2)
    return None


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", default="127.0.0.1:16384")
    args = ap.parse_args(argv)

    dump = adb(args.device, "shell", "uiautomator", "dump", "/sdcard/ui_gate.xml")
    if dump.returncode != 0:
        print("uiautomator unavailable:", dump.stderr.strip()[:120])
        return 0
    xml = adb(args.device, "shell", "cat", "/sdcard/ui_gate.xml").stdout
    spot = find_cancel(xml)
    if spot is None:
        print("no update dialog")
        return 0
    adb(args.device, "shell", "input", "tap", str(spot[0]), str(spot[1]))
    print("dismissed the update dialog at", spot)
    return 0


if __name__ == "__main__":
    sys.exit(main())
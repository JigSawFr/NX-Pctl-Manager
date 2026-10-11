#!/usr/bin/env python3
"""Writes compat.json, published with every release next to playguard.nro.

The app reads the one of the latest release (Tools > Check for updates, and
the "firmware not supported yet" screen) to know whether updating would make
the console's firmware supported. Values come from the sources, so they can
never disagree with the build:
  version            CMakeLists.txt  VERSION_MAJOR / MINOR / ALTER
  fw_tested_max      source/core/sysinfo.h  PCTL_FW_TESTED_MAX (verified on a console:
                     above it the app asks first, so a newer release "supports"
                     a firmware only once it was verified there)
  fw_min_play_timer  source/core/sysinfo.h  PCTL_FW_MIN_PLAYTIMER

Usage: tools/gen_compat.py [repo-root] [output]   (default: . and ./compat.json)
"""
import json
import re
import sys

ROOT = sys.argv[1] if len(sys.argv) > 1 else "."
OUT = sys.argv[2] if len(sys.argv) > 2 else "compat.json"


def fail(msg):
    print("gen_compat:", msg)
    sys.exit(1)


cmake = open(f"{ROOT}/CMakeLists.txt", encoding="utf-8").read()
parts = []
for name in ("MAJOR", "MINOR", "ALTER"):
    m = re.search(rf'set\(VERSION_{name} "(\d+)"\)', cmake)
    if not m:
        fail(f"VERSION_{name} not found in CMakeLists.txt")
    parts.append(m.group(1))

sysinfo = open(f"{ROOT}/source/core/sysinfo.h", encoding="utf-8").read()


def firmware(macro):
    m = re.search(rf"#define {macro}\s+MAKEHOSVERSION\((\d+),\s*(\d+),\s*(\d+)\)", sysinfo)
    if not m:
        fail(f"{macro} not found in source/core/sysinfo.h")
    return ".".join(m.groups())


compat = {
    "schema": 1,
    "version": ".".join(parts),
    "fw_tested_max": firmware("PCTL_FW_TESTED_MAX"),
    "fw_min_play_timer": firmware("PCTL_FW_MIN_PLAYTIMER"),
}
with open(OUT, "w", encoding="utf-8") as f:
    json.dump(compat, f, indent=2)
    f.write("\n")
print(f"{OUT}: {json.dumps(compat)}")

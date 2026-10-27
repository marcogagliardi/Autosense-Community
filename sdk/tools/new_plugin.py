#!/usr/bin/env python3
"""
new_plugin.py  -  create a new plugin from the template.

    python sdk/tools/new_plugin.py P012 "Soil moisture" --name SOIL --bus analog

Writes src/Plugins/P012.cpp in the AutoSense firmware, src/P012.cpp in the
Community test harness (picks the next free code if you pass "auto").
Then:  python sdk/tools/check_plugin.py src/Plugins/P012.cpp
"""
import argparse
import glob
import os
import re
import sys

SDK = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT = os.path.dirname(SDK)
BUSES = {"analog": "Analog", "digital": "Digital", "i2c": "I2C", "onewire": "OneWire", "none": "None"}


def used_codes():
    codes = set()
    for f in glob.glob(os.path.join(PROJECT, "src", "**", "*.cpp"), recursive=True):
        with open(f, encoding="utf-8", errors="ignore") as fh:
            codes.update(re.findall(r'AUTOSENSE_REGISTER\s*\(\s*"(P\d{3})"', fh.read()))
    return codes


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("code", help='plugin code like P012, or "auto"')
    ap.add_argument("title", help='title shown in the portal, e.g. "Soil moisture"')
    ap.add_argument("--name", help="short wire name (MQTT sensorType), default from the title")
    ap.add_argument("--bus", choices=sorted(BUSES), default="analog")
    default_out = os.path.join(PROJECT, "src", "Plugins")
    if not os.path.isdir(default_out):  # Community harness: plugins live directly in src/
        default_out = os.path.join(PROJECT, "src")
    ap.add_argument("--out", default=default_out)
    a = ap.parse_args()

    used = used_codes()
    code = a.code.upper()
    community = not os.path.isdir(os.path.join(PROJECT, "src", "Plugins"))
    if code == "AUTO":
        # Community harness: P900-P999 are for plugins in development; the
        # maintainers assign the final code when the plugin is merged.
        n = 900 if community else 12
        while "P%03d" % n in used:
            n += 1
        code = "P%03d" % n
    if not re.fullmatch(r"P\d{3}", code):
        sys.exit("code must look like P012")
    if code in used:
        sys.exit("%s is already used" % code)
    name = (a.name or re.sub(r"[^A-Za-z0-9]", "", a.title)).upper()[:15]

    with open(os.path.join(SDK, "templates", "PXXX.cpp"), encoding="utf-8") as f:
        t = f.read()
    t = t.replace("PXXX", code).replace("<Sensor name>", a.title)
    t = t.replace('d.info("MYSENSOR", F("My sensor")', 'd.info("%s", F("%s")' % (name, a.title))
    t = t.replace("MYSENSOR", name).replace("Bus::Analog", "Bus::" + BUSES[a.bus])
    os.makedirs(a.out, exist_ok=True)
    path = os.path.join(a.out, code + ".cpp")
    if os.path.exists(path):
        sys.exit("%s exists" % path)
    with open(path, "w", encoding="utf-8") as f:
        f.write(t)
    print("created %s\nnext:  python sdk/tools/check_plugin.py %s" % (os.path.relpath(path, PROJECT), os.path.relpath(path, PROJECT)))


if __name__ == "__main__":
    main()

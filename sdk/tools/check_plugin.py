#!/usr/bin/env python3
"""
check_plugin.py  -  check an AutoSense plugin WITHOUT the AutoSense core.

    python sdk/tools/check_plugin.py src/Plugins/P012.cpp
    python sdk/tools/check_plugin.py P012.cpp --adc 3000 -I path/to/SensorLib/src

1. Static rules (always): only the SDK header from AutoSense, one registration,
   no blocking loops, no direct EEPROM / WiFi / web server / MQTT / OLED access.
2. Simulation (needs g++ on PATH: Linux, macOS, WSL or MinGW): compiles the
   plugin against sdk/include + Arduino stand-ins, prints the schema the portal
   will render, validates it and runs begin()/read()/calibration with a
   simulated analog input.

Sensor libraries: headers are found in .pio/libdeps/<env>/ automatically (run
`pio pkg install` once) or pass -I.  Library CODE is not executed - if read()
calls into the library the simulation stops there; the schema checks still count.
Exit code 0 = no errors.
"""
import argparse
import glob
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SDK = os.path.dirname(HERE)
PROJECT = os.path.dirname(SDK)

FORBIDDEN_INCLUDES = ["Global.h", "handlers.h", "mqtt_core.h", "storage_service.h", "display_helpers.h",
                      "PluginHost.h", "SettingsStore.h", "Outputs.h", "HubBridge.h", "ApiRoutes.h", "Display.h",
                      "EEPROM.h", "ESPAsyncWebServer.h", "PubSubClient.h", "Adafruit_SSD1306.h", "WiFi.h", "ESP8266WiFi.h"]
FORBIDDEN_CALLS = [
    (r"\bEEPROM\s*\.", "do not touch EEPROM: use settings (d.number/d.text...) and ctx.setNum()+ctx.save()"),
    (r"\bevents\s*\.send|\bserver\s*\.on", "do not talk to the web server: describe channels, the core streams them"),
    (r"\bclient\s*\.publish", "do not publish MQTT yourself: use d.mqtt(...) and Channel::mqtt(slot)"),
    (r"\bDisplay(Text|Clear|ClearLine)\s*\(", "do not draw on the OLED: use Channel::oled(priority, tag)"),
    (r"while\s*\(\s*(1|true)\s*\)\s*;?", "endless loop: return or call out.fail() instead"),
    (r"\buser_settings\b|\bpluginSettings\b", "core globals are private: use ctx.num()/ctx.text()"),
    (r"\bESP\s*\.\s*restart", "plugins must not reboot the device"),
]


def static_checks(src, text):
    errors, warnings = [], []
    for inc in re.findall(r'#\s*include\s*[<"]([^>"]+)[>"]', text):
        if os.path.basename(inc) in FORBIDDEN_INCLUDES:
            errors.append('includes "%s" - plugins may only include <AutoSensePlugin.h> from AutoSense' % inc)
    if "AutoSensePlugin.h" not in text:
        errors.append("missing #include <AutoSensePlugin.h>")
    regs = re.findall(r'AUTOSENSE_REGISTER\s*\(\s*"([^"]+)"\s*,\s*(\w+)\s*\)', text)
    if len(regs) != 1:
        errors.append("exactly one AUTOSENSE_REGISTER(\"Pnnn\", Class) is required (found %d)" % len(regs))
    else:
        code = regs[0][0]
        if not re.fullmatch(r"P\d{3}", code):
            warnings.append('plugin code "%s" should look like P012' % code)
        name = os.path.splitext(os.path.basename(src))[0]
        if name != code:
            warnings.append('file name "%s" should match the plugin code "%s"' % (name, code))
        others = glob.glob(os.path.join(PROJECT, "src", "**", "*.cpp"), recursive=True)
        for o in others:
            if os.path.abspath(o) == os.path.abspath(src):
                continue
            with open(o, encoding="utf-8", errors="ignore") as f:
                if re.search(r'AUTOSENSE_REGISTER\s*\(\s*"%s"' % re.escape(code), f.read()):
                    errors.append('code "%s" is already used by %s' % (code, os.path.relpath(o, PROJECT)))
    for pat, msg in FORBIDDEN_CALLS:
        for m in re.finditer(pat, text):
            line = text.count("\n", 0, m.start()) + 1
            errors.append("line %d: %s" % (line, msg))
    for m in re.finditer(r"\bdelay\s*\(\s*(\d+)\s*\)", text):
        if int(m.group(1)) > 50:
            line = text.count("\n", 0, m.start()) + 1
            warnings.append("line %d: delay(%s) blocks the whole device; keep calls short (< 50 ms)" % (line, m.group(1)))
    for m in re.finditer(r'd\.(?:number|pin|address|text|choice|toggle)\s*\(\s*"([^"]+)"', text):
        if len(m.group(1)) > 15:
            errors.append('setting key "%s" is longer than 15 characters' % m.group(1))
    for m in re.finditer(r'(?:info|number|pin|address|text|choice|toggle|channel|hint|help)\s*\([^;]*?,\s*"[^"]*"\s*[,)]', text):
        pass  # labels as plain strings compile fine; F() only saves RAM
    return errors, warnings


def lib_includes(extra):
    dirs = list(extra or [])
    for env_dir in glob.glob(os.path.join(PROJECT, ".pio", "libdeps", "*")):
        for lib in glob.glob(os.path.join(env_dir, "*")):
            dirs.append(lib)
            if os.path.isdir(os.path.join(lib, "src")):
                dirs.append(os.path.join(lib, "src"))
        break  # one environment is enough for headers
    return dirs


def simulate(src, args):
    gxx = shutil.which("g++") or shutil.which("clang++")
    if not gxx:
        print("\n(simulation skipped: no g++ on PATH - install build tools, WSL or MinGW)")
        return 0
    out = os.path.join(tempfile.gettempdir(), "autosense_sim" + (".exe" if os.name == "nt" else ""))
    cmd = [gxx, "-std=gnu++17", "-w", "-g", "-D" + args.chip, "-DARDUINO=10800", "-DAUTOSENSE_SIM=1",
           "-I" + os.path.join(HERE, "sim", "stubs"), "-I" + os.path.join(SDK, "include")]
    cmd += ["-I" + d for d in lib_includes(args.include)]
    cmd += [os.path.abspath(src), os.path.join(HERE, "sim", "sim_main.cpp"), "-o", out]
    if sys.platform.startswith("linux"):
        cmd.append("-Wl,--unresolved-symbols=ignore-all")
    elif sys.platform == "darwin":
        cmd += ["-Wl,-undefined,dynamic_lookup"]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("\nCompile errors (against the SDK only):")
        print("\n".join(l for l in r.stderr.splitlines() if "error" in l or "note" not in l)[:4000])
        return 1
    env = dict(os.environ, AUTOSENSE_SIM_ADC=str(args.adc))
    r = subprocess.run([out], capture_output=True, text=True, env=env)
    for line in r.stdout.splitlines():
        if line.startswith("SCHEMA "):
            print("\nSchema (what the portal, OLED and MQTT will use)")
            import json
            s = json.loads(line[7:])
            print("  %s  %s - %s  (wire name %s)" % (s["code"], s["title"], s["summary"], s["name"]))
            for x in s["settings"]:
                print("  setting  %-14s %-32s default %s" % (x["key"], x["label"], x["default"]))
            for c in s["channels"]:
                extra = []
                if c["kind"]: extra.append("kind " + c["kind"])
                if c["oled"]: extra.append("OLED prio %d" % c["oled"])
                if c["mqttSlot"]: extra.append("MQTT slot %d" % c["mqttSlot"])
                print("  channel  %-14s %-24s %-6s %s" % (c["key"], c["label"], c["unit"], ", ".join(extra)))
            if s["calibration"]:
                print("  calibration: %s" % s["calibration"])
        else:
            print(line)
    if r.returncode not in (0, 1):
        if "Simulation" in r.stdout:
            print("  (stopped inside sensor-library code, which cannot run here; the schema checks above still count)")
            return 0
        print("(the simulator crashed with code %d)" % r.returncode)
        return 1
    return r.returncode


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("plugin", help="the plugin .cpp file")
    ap.add_argument("-I", "--include", action="append", help="extra include dir for the sensor library")
    ap.add_argument("--adc", type=int, default=2048, help="simulated analogRead() value (default 2048)")
    ap.add_argument("--chip", choices=["ESP32", "ESP8266"], default="ESP32")
    ap.add_argument("--static-only", action="store_true", help="skip the compile/simulation step")
    args = ap.parse_args()

    with open(args.plugin, encoding="utf-8") as f:
        text = f.read()
    print("Checking %s\n" % args.plugin)
    errors, warnings = static_checks(args.plugin, text)
    print("Rules")
    for e in errors:
        print("  ERROR   " + e)
    for w in warnings:
        print("  WARNING " + w)
    if not errors and not warnings:
        print("  all rules OK")
    rc = 1 if errors else 0
    if not args.static_only:
        rc = max(rc, simulate(args.plugin, args))
    print("\nRESULT: %s" % ("OK" if rc == 0 else "FAILED"))
    sys.exit(rc)


if __name__ == "__main__":
    main()

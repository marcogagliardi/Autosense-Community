# Contributing a plugin

Thank you for extending AutoSense. Submissions are reviewed and built into the
official firmware, so please go through this list first.

## Before you submit

- [ ] One file, `src/P9xx.cpp`, that includes only `<AutoSensePlugin.h>` (plus your sensor library).
- [ ] `python sdk/tools/check_plugin.py src/P9xx.cpp` prints `RESULT: OK`.
- [ ] Tested with the harness on real hardware: `begin()` finds the sensor and readings look right.
- [ ] If you can, tested on both ESP32 (`esp32dev`) and ESP8266 (`nodemcuv2`). Say which one you used.
- [ ] Unplugging the sensor gives `out.fail(...)` (or `begin()` -> false), not a crash or a hang.
- [ ] `read()` takes well under 200 ms (the harness prints the time).
- [ ] Labels and help texts are clear to a user who has never seen the code (they appear in the portal).
- [ ] If there is a calibration: `cal`, `capture`, `save` and `cancel` all behave in the harness.

## How to submit

**Pull request (preferred).** Fork this repository, add your plugin as
`src/P9xx.cpp` and `examples/P9xx_Name/README.md` (wiring, `lib_deps`, what it
measures), and open a pull request. The GitHub workflow (when enabled) checks the rules and
builds the harness for ESP32 and ESP8266 with your plugin.

**Discussion.** If you prefer, post in the
[community discussion](https://github.com/marcogagliardi/Autosense-Community/discussions/2) with:

1. Sensor model and a link to its datasheet.
2. Wiring (pins, I²C address, pull-ups, supply voltage).
3. The `lib_deps` line(s) you added.
4. Your `.cpp` file.
5. The output of `check_plugin.py`.
6. A Serial Monitor log from the harness showing a few readings (and a calibration, if any).

## What happens next

The maintainers build your plugin into the private firmware, test it on the
AutoSense hardware and give it its final code (P012, P013, ...). You'll be credited in the
release notes. If something needs changing, we'll reply on your pull request or
discussion.

## Code style

- Keep it readable: short functions, comments for anything sensor-specific
  (formulas, datasheet constants, timing).
- Wrap labels in `F("...")` (saves RAM on ESP8266); keys, units and tags are plain literals.
- No `String` concatenation in `read()` (heap fragmentation on ESP8266).

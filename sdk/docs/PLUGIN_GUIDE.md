# Writing an AutoSense plugin

A plugin is **one `.cpp` file** that includes **one AutoSense header**,
`<AutoSensePlugin.h>`. It describes the sensor (settings, readings, optional
calibration) and reads it. AutoSense builds everything else from that
description:

| You write | AutoSense does |
|---|---|
| `d.number(...)`, `d.pin(...)`, `d.choice(...)` | Settings form in the portal, validation, EEPROM storage, migration |
| `d.channel(...)` | Dashboard card, live updates, sparkline, `/api/readings` |
| `.oled(priority, tag)` | Line on the OLED (shared with other modules by priority) |
| `d.mqtt(slot, ...)`, `.mqtt(slot)` | MQTT topic/metric settings and the publish envelope |
| `d.calibration(...)` | Portal wizard, `/api/cal/*`, MQTT hub capability |

You never touch HTML, EEPROM, the web server, MQTT or the display. The core
code is not needed to write, check or test a plugin: external contributors get
the `sdk/` folder and the on-device test harness in the
[AutoSense Community](https://github.com/marcogagliardi/Autosense-Community) repository.

Paths below are for the Community repository (`src/P9xx.cpp`). In the AutoSense
firmware repository plugins live in `src/Plugins/`.

## 1. Create it

```bash
python sdk/tools/new_plugin.py auto "Soil moisture" --name SOIL --bus analog
# Community repository -> src/P900.cpp   (P900-P999: in development; the final code is given on merge)
# AutoSense firmware   -> src/Plugins/P012.cpp   (next free code)
```

## 2. Fill in the four parts

```cpp
#include <AutoSensePlugin.h>

class P900 : public autosense::Plugin {
public:
  void describe(autosense::Descriptor &d) override {
    d.info("SOIL", F("Soil moisture"), F("Capacitive soil probe"), autosense::Bus::Analog);
    d.pin("adc_pin", F("Analog pin (GPIO)"), A0).adc1();
    d.number("dry", F("Reading in dry air"), 3000).range(0, 4095);
    d.number("wet", F("Reading in water"), 1300).range(0, 4095);
    d.mqtt(1, "SOIL/Value", "SOIL");
    d.channel("moisture", F("Moisture"), "%", 0).kind("soil_moisture").oled(3, "SOIL").mqtt(1);
  }
  bool begin(autosense::Context &ctx) override { return true; }
  void read(autosense::Context &ctx, autosense::Readings &out) override {
    float raw = autosense::analogAverage((int)ctx.num("adc_pin"));
    float pct = (ctx.num("dry") - raw) / (ctx.num("dry") - ctx.num("wet")) * 100;
    out.set("moisture", constrain(pct, 0, 100));
  }
};
AUTOSENSE_REGISTER("P900", P900)
```

| Hook | When | Rules |
|---|---|---|
| `describe(d)` | whenever the core needs the schema | no side effects, no hardware |
| `begin(ctx)` | boot, when switched on, after a setting changed | must be callable again; free what the last call created; return `false` if the sensor does not answer |
| `read(ctx, out)` | every reading interval | short (< ~100 ms), never `while(1)`; `out.fail("why")` on errors; `NAN` for a single bad value |

### Settings

| Call | Portal control | Notes |
|---|---|---|
| `d.number(key, label, def)` | decimal input | `.range(lo,hi)`, `.step()`, `.unit("V")` |
| `d.pin(key, label, def)` | GPIO input | `.adc1()` = must be GPIO 32–39 on ESP32 (analog with Wi-Fi on) |
| `d.address(key, label, 0x76)` | hex input | I²C address, validated 0x01–0x7F |
| `d.choice(key, label, def).option(v, F("..."))` | segmented buttons | up to 4 options |
| `d.toggle(key, label, def)` | switch | stored as 0/1 |
| `d.text(key, label, "def")` | text | `.maxLength(n)` |
| `d.mqtt(slot, "TAIL", "METRIC", F("what"))` | MQTT topic + metric | default topic `<device>/<TAIL>` |

Common modifiers: `.help(F("..."))`, `.group("Calibration")` (heading in the
form), `.live()` (no module restart needed), `.legacy("old_key", convert)`
(import a value stored by firmware 1.x). Keys: max 15 characters, stored as
`<code>_<key>`; **never rename a key after release** (users would lose the value).

Read them with `ctx.num("key")` / `ctx.text("key")`. Only values different
from the default are written to EEPROM.

### Channels (readings)

`d.channel(key, label, unit, decimals)` then:

- `.kind("temperature")` — semantic id. Another plugin can use the latest
  value of any module with `ctx.latest("temperature")` (TDS and pH use
  `"liquid_temperature"` for compensation, the MQ-9 uses `"temperature"`).
- `.oled(priority, "T", "TEMP")` — the OLED shows the 3 (64 px) or 2 (32 px)
  values with the lowest priority number across all modules.
- `.mqtt(slot)` — publish alone on topic slot n; or `d.mqttJson(slot)` to publish
  all channels as one JSON object.
- `.hidden()` — MQTT only, no dashboard card. `.localOnly()` — never sent to MQTT.
- `.mqttLocation("...")`, `.mqttUnit("...")` — override envelope fields.

### Calibration (optional)

```cpp
d.calibration(F("Calibrate pH"))
  .intro(F("You need two buffers..."))
  .live("voltage").stable(5, 0.005f)          // value watched + "stable" rule
  .mqtt("calibrate_ph", F("Calibrate pH"), "calibration/ph/reading")  // hub capability (optional)
  .step("min", F("Low buffer"),  F("Put the probe in..."), "min", F("Capture Min (pH 4)"))
  .step("max", F("High buffer"), F("Rinse, then..."),      "max", F("Capture Max (pH 7)"))
  .field("voltage", F("Live voltage"), "number", "V", 3, true)
  .field("minCaptured", F("Min captured"), "boolean", "", 0, true);
```

| Hook | Called | Do |
|---|---|---|
| `calibrationStart(ctx, st)` | session starts | reset accumulators, `st.set(...)` initial fields; `false` to refuse |
| `calibrationTick(ctx, st)` | about every second | sample, update the live field |
| `calibrationCapture(ctx, stepId, st)` | user/hub captures a step | store the point; `st.message("why")` + `false` on error |
| `calibrationFinish(ctx, save, st)` | session ends | if `save`: validate, `ctx.setNum()` + `ctx.save()`; `false` + message rejects |

One session runs at a time; it is cancelled after 15 minutes without
activity. The wizard, `/api/cal/*` and the MQTT hub all drive these same hooks.
Examples: `P006.cpp` (two points, applied at the end), `P011.cpp` (one
capture, applied immediately, reverted on discard).

## 3. Check it on the PC

```bash
python sdk/tools/check_plugin.py src/P900.cpp            # rules + schema + simulated reads
python sdk/tools/check_plugin.py src/P900.cpp --adc 3500 # other analog value
```

It checks the contract (only the SDK header, one registration, no EEPROM /
Wi-Fi / web / MQTT / OLED access, no endless loops or long delays, key lengths,
declared channels, MQTT slots, calibration fields), prints the schema the
portal will show and runs `begin()`, `read()` and the calibration hooks with
simulated inputs. Library headers are taken from `.pio/libdeps`; add more with
`-I`. Library code itself doesn't run in the simulator, so the simulation stops at the
first call into the sensor library. The checks before that still count. Needs Python 3
and g++ (Linux, macOS, WSL or MinGW); without g++ use `--static-only`.

## 4. Test it on hardware

**Community repository:** add the sensor library to `lib_deps` in `platformio.ini`,
then PlatformIO **Upload and Monitor**. The harness prints the schema, calls
`begin()` and `read()`, and shows the values, the MQTT message and the OLED lines.
Type `help` in the Serial Monitor to change settings (`set P900 dry 2900`), run the
calibration (`cal P900`, `capture`, `save`) or scan the I²C bus (`i2c`).

**AutoSense firmware repository:**

```bash
pio run -e esp32dev           # or nodemcuv2
cd web && npm run dev         # portal preview without hardware (http://localhost:5173)
```

To see your plugin in the mock portal, regenerate the fixtures with
`tools/hosttest/dump.sh` (Linux/WSL).

## Rules of thumb

- Only `<AutoSensePlugin.h>` from AutoSense; your sensor library is fine.
- Strings passed as keys/units/tags must be literals; wrap labels in `F("...")`.
- Allocate hardware objects in `begin()` with `new`, delete the previous ones first.
- No `delay()` above a few ms; average quickly (`analogAverage(pin, 10, 2)`).
- A plugin that is switched off is never called (no `begin`, no `read`).
- Need a custom dashboard look? Plugins can't ship portal code, but the maintainers can add a card for it (`web/src/cards/custom.tsx` in the firmware repository).

# AutoSense Community: plugin development kit

Write a sensor plugin for [AutoSense](https://github.com/marcogagliardi/Autosense),
test it on your own ESP32 or ESP8266, and submit it for the next firmware release.

AutoSense 2.x plugins are written against a small public SDK, a single header,
`sdk/include/AutoSensePlugin.h`. A plugin is **one `.cpp` file**. It *describes*
its settings, readings and (optionally) a calibration, and AutoSense builds the
rest from that description: the settings form in the web portal, the dashboard
card, the OLED line, the MQTT topics, and the calibration wizard. Plugins never
contain HTML or touch EEPROM, Wi‑Fi, MQTT or the display.

This repository contains that SDK and a **test harness**: a PlatformIO project
that runs your plugin on real hardware the same way the firmware does, without
the private AutoSense core.

```
Autosense-Community/
├── platformio.ini            harness for esp32dev and nodemcuv2; add your sensor library here
├── src/
│   ├── main.cpp              the test harness (you never edit it)
│   └── P9xx.cpp              <- your plugin goes here
├── sdk/
│   ├── include/AutoSensePlugin.h   the plugin SDK (the only AutoSense header a plugin includes)
│   ├── templates/PXXX.cpp          starting point
│   ├── docs/PLUGIN_GUIDE.md        full reference
│   └── tools/
│       ├── new_plugin.py           create a plugin from the template
│       └── check_plugin.py         rule check + schema preview + simulation on the PC
├── examples/
│   ├── P002_DHT/             digital sensor, two MQTT topics
│   ├── P010_BME688/          I2C sensor, one JSON MQTT message
│   └── P006_pH/              analog sensor with a two-point calibration
├── CONTRIBUTING.md           how to submit
└── tools/github/check.yml    automatic checks on push / pull request (move to .github/workflows/ to enable)
```

## Quick start

You need VS Code with the PlatformIO extension, and Python 3 for the tools.

```bash
git clone https://github.com/marcogagliardi/Autosense-Community.git
cd Autosense-Community
python sdk/tools/new_plugin.py auto "Soil moisture" --name SOIL --bus analog
```

This creates `src/P900.cpp`. Codes P900-P999 are for plugins in development;
the maintainers give it its final code when it's merged.

1. Open `src/P900.cpp` and fill in `describe()`, `begin()` and `read()`. The
   template explains each part; [the plugin guide](sdk/docs/PLUGIN_GUIDE.md)
   has the full reference.
2. Add your sensor library to `lib_deps` in `platformio.ini`.
3. Check it on the PC (needs g++, i.e. Linux, macOS, WSL or MinGW, for the simulation part):
   ```bash
   python sdk/tools/check_plugin.py src/P900.cpp
   ```
4. Wire the sensor, connect the board, pick `esp32dev` or `nodemcuv2` in PlatformIO
   and click **Upload and Monitor**.

Prefer to start from something that works? Copy an example into `src/` instead,
e.g. `examples/P006_pH/P006.cpp`, which needs no library.

## What the harness shows

```
[P900] Soil moisture - Capacitive soil probe   (wire name SOIL, bus analog)
  settings (portal form):
    adc_pin        pin      Analog pin (GPIO)              = 36
    dry            number   Reading in dry air             = 3000
    wet            number   Reading in water               = 1300
    MQTT_SLOT1     text     MQTT topic                     = "AutoSense/SOIL/Value"   (applied without restarting the module)
    METRIC1        text     Metric name                    = "SOIL"   (applied without restarting the module)
  channels (dashboard cards):
    moisture       Moisture                 %      kind=soil_moisture OLED(prio 3, "SOIL") MQTT slot 1
  schema OK

[P900] begin() -> true

---- 5 s ------------------------------------------------
[P900] read #1 (20 ms)
  moisture               41 %
  MQTT  AutoSense/SOIL/Value  {"sensorType":"SOIL","location":"Lab 1","manufacturer":"Autosense","unit":"%","metric":"SOIL","value":41.18,"rawJson":null,"mac":"24:6F:28:..."}
  OLED  [SOIL 41 %]   (128x64: first 3 lines, 128x32: first 2)
```

(This is the soil-moisture example from the [plugin guide](sdk/docs/PLUGIN_GUIDE.md#2-fill-in-the-four-parts).)

- **Schema:** what the portal will build from your `describe()`, with problems
  flagged (`!!`).
- **Every few seconds:** each `read()` with its duration, the values, the exact
  MQTT message the firmware publishes, and the OLED lines.

Commands in the Serial Monitor (type and press Enter):

| Command | |
|---|---|
| `help` | list of commands |
| `list` | schema and current settings |
| `set P900 dry 2900` | change a setting; it's validated like the portal and the module restarts (`begin()` again) |
| `read`, `interval 2`, `pause`, `resume` | control the reads |
| `quiet` / `verbose` | hide or show the MQTT and OLED previews |
| `i2c` | scan the I²C bus |
| `cal P006`, `capture`, `save`, `cancel` | run the calibration, as the portal wizard and the MQTT hub do |

The harness keeps settings in RAM. They're back to their defaults after a reset.

## Rules for plugins

These are checked by `check_plugin.py` and by the GitHub workflow:

- Include only `<AutoSensePlugin.h>` from AutoSense, plus your sensor library.
- No EEPROM, Wi‑Fi, web server, MQTT client, OLED or `Global.h`. Describe things and the core does the rest.
- `begin()` can be called again (after a setting change): free what the previous call created.
- `read()` is short: no `while(1)`, no `delay()` longer than a few milliseconds, `out.fail("why")` on errors.
- Values from other sensors come from `ctx.latest("kind")`, not from globals.
- Setting keys are at most 15 characters and never change after release.
- Exactly one `AUTOSENSE_REGISTER("P9xx", YourClass)`.

## Submit your plugin

See [CONTRIBUTING.md](CONTRIBUTING.md). In short: open a pull request (or a
[discussion](https://github.com/marcogagliardi/Autosense-Community/discussions/2))
with your one `.cpp` file, the `lib_deps` line, the output of `check_plugin.py`
and a Serial Monitor log from real hardware.

## Upgrading a plugin written for AutoSense 1.x

The old `PluginInterface.h` / `PluginRegistry.h` API (HTML tiles, `pluginSettings`
maps, `MQTTsend`, `DisplayText`) is gone. Most of a 1.x plugin's code was HTML and
plumbing; in 2.x only the sensor logic stays:

| 1.x | 2.x |
|---|---|
| `registerSettings()` / `registerSettingsExtra()` with `"<code>_key"` | `d.number("key", F("Label"), default)` etc. in `describe()`; read with `ctx.num("key")` |
| `createHTMLTile()`, `createHTMLSettingsForm()`, `createJSFooter()`, `createReadingsHTML()` | nothing: the portal is generated from `describe()` |
| `init()` + `isEnabled()` | `begin(ctx)`, only called when the module is enabled |
| `read()` + `MQTTsend()` + `DisplayText()` + SSE | `read(ctx, out)` with `out.set("key", value)`; MQTT, OLED and the portal are automatic |
| `latestTemperature` style globals | `.kind("temperature")` on the channel, `ctx.latest("temperature")` in other plugins |
| REST routes / MQTT subscriptions for calibration | `d.calibration(...)` + the four `calibration*` hooks |
| `__attribute__((constructor))` + `PluginRegistry` | `AUTOSENSE_REGISTER("P9xx", Class)` |

Keep your old setting keys (`d.number("rzero", ...)`) so devices keep their values;
renamed keys can be imported with `.legacy("old_key")`.

# P002 - DHT11 / DHT22 temperature and humidity

A **single-pin digital sensor**. This is the plugin the AutoSense firmware ships.

What it shows:
- A GPIO setting (`d.pin`) and a choice between DHT11 and DHT22 (`d.choice(...).option(...)`).
- Two readings, each published on its own MQTT topic (`d.mqtt(1, ...)`, `d.mqtt(2, ...)`, `.mqtt(slot)`).
- Semantic kinds (`.kind("temperature")`) so other plugins can use the value with `ctx.latest("temperature")`.
- OLED priority 1 and 2: temperature and humidity are the first lines on the display.
- Hardware objects created in `begin()` and released before a new `begin()`.
- `out.fail(...)` when the sensor does not answer.

Wiring: data pin to GPIO 14 (default), 10 kOhm pull-up to 3.3 V.

```ini
lib_deps =
    ${common.lib_deps}
    adafruit/DHT sensor library @ ^1.4.6
    adafruit/Adafruit Unified Sensor @ ^1.1.14
```

Try it: copy `P002.cpp` into `src/`, add the libraries, Upload and Monitor.

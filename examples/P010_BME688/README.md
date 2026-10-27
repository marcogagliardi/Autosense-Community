# P010 - Bosch BME688 / BME680 (I2C)

An **I2C multi-value sensor**: temperature, humidity, pressure and gas resistance.
This is the plugin the AutoSense firmware ships.

What it shows:
- An I2C address setting edited in hex (`d.address("i2c_addr", ..., 0x77)`).
- `begin()` returning `false` when the sensor does not answer ("Sensor not found" in the portal).
- All values in **one MQTT JSON message** (`d.mqttJson(1)`).
- A dashboard-only value (`.localOnly()`, gas in kOhm) and an MQTT-only value (`.hidden()`, gas in ohm).
- Three OLED lines (T, H, G).

Wiring: SDA/SCL to the board's I2C pins (ESP32 21/22, ESP8266 D2/D1 = GPIO 4/5),
address 0x77 (0x76 when SDO is tied to GND). Type `i2c` in the harness to scan the bus.

```ini
lib_deps =
    ${common.lib_deps}
    adafruit/Adafruit BME680 Library @ ^2.0.6
    adafruit/Adafruit Unified Sensor @ ^1.1.14
```

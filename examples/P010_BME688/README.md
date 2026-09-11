# P010 - BME688 Environmental Sensor Plugin

This example demonstrates how to implement an **I2C multi-variable sensor** (Temperature, Humidity, Pressure, Gas Resistance) for AutoSense.

### Features Demonstrated
- Configurable I2C address via Web UI settings (`P010_i2c_addr` defaulting to 119 / `0x77`).
- Sensor oversampling, filter setup, and gas heater control using the Adafruit BME680 library.
- Multi-line OLED screen rendering:
  - **64px screen (3 lines)**: Line 1 (Y=16) Temperature, Line 2 (Y=32) Humidity, Line 3 (Y=50) Gas resistance.
  - **32px screen (2 lines)**: Line 1 (Y=14) Temp & Hum, Line 2 (Y=24) Pressure & Gas.
- JSON-encoded MQTT telemetry.
- Real-time Server-Sent Events (SSE) updates to the Web UI dashboard with multiple fields.

### PlatformIO Dependencies
Add the following to your `lib_deps` when compiling:
```ini
lib_deps =
    adafruit/Adafruit BME680 Library @ ^2.0.4
    adafruit/Adafruit Unified Sensor @ ^1.1.14
```

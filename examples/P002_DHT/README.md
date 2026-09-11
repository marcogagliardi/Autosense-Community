# P002 - DHT11 / DHT22 Sensor Plugin

This example demonstrates how to implement a **single-pin digital GPIO sensor** for AutoSense.

### Features Demonstrated
- GPIO pin configuration via web settings (`P002_gpio_pin`).
- Dropdown select configuration (`P002_dht_type`: DHT11 vs DHT22).
- Dynamic sensor allocation (`new DHT_Unified`).
- Dual MQTT metric publication (Temperature & Humidity).
- OLED screen rendering for both 32px and 64px display heights.
- Real-time Server-Sent Events (SSE) updates to the Web UI dashboard.

### PlatformIO Dependencies
Add the following to your `lib_deps` when compiling:
```ini
lib_deps =
    adafruit/DHT sensor library @ ^1.4.6
    adafruit/Adafruit Unified Sensor @ ^1.1.14
```

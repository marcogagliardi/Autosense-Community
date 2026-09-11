# AutoSense Community Plugins

Welcome to the **AutoSense Community** repository!

This repository contains the official plugin development kit for extending [AutoSense](https://github.com/marcogagliardi/Autosense) with new sensors, modules, and hardware integrations.

Because the core AutoSense firmware is maintained in a private build repository, external contributors develop plugins modularly using this kit. Submitted plugins are reviewed, compiled, and merged directly into official firmware releases.

---

## 📁 Repository Structure

```
Autosense-Community/
├── include/
│   ├── PluginInterface.h       # Base class and API contracts for AutoSense plugins
│   ├── PluginRegistry.h        # Self-registration factory interface
│   └── display_helpers.h       # OLED screen helper functions (coordinates & drawing)
│
├── TEMPLATE/
│   ├── PXXX.h                  # Boilerplate header for new plugins
│   └── PXXX.cpp                # Boilerplate implementation with self-registration
│
└── examples/
    ├── P002_DHT/               # Example: Single-pin digital GPIO sensor (DHT11/DHT22)
    └── P010_BME688/            # Example: I2C environmental multi-sensor (BME688)
```

---

## 🚀 Quick Start: Building a Plugin

1. **Copy the Template**:
   Copy `TEMPLATE/PXXX.h` and `TEMPLATE/PXXX.cpp` into your new plugin folder (e.g. `P012_SCD30/`).
2. **Rename Class & Files**:
   Replace `PXXX` with your module ID (e.g. `P012`) and configure `MODULE_NAME`.
3. **Implement Required Methods**:
   - `init()`: Initialize hardware, I2C bus, or GPIO pins.
   - `read()`: Read sensor data, format OLED display lines, publish MQTT telemetry, and send Web UI updates.
   - `registerSettings()`: Define default settings (pins, I2C addresses, enable flags).
   - `isEnabled()`: Check if your plugin toggle is active.
   - `createHTMLTile()` & `createHTMLSettingsForm()`: Dashboard card and config form for the device web portal.
4. **Inspect the Examples**:
   - For GPIO/single-pin sensors, check [`examples/P002_DHT`](examples/P002_DHT).
   - For I2C/multi-variable sensors, check [`examples/P010_BME688`](examples/P010_BME688).

---

## 📺 OLED Display Guidelines

When outputting readings to the onboard OLED display, use the provided helper functions from `display_helpers.h`:

- **64px Screen (3 lines available)**:
  - Line 1: `Y = 16`
  - Line 2: `Y = 32`
  - Line 3: `Y = 50`
- **32px Screen (2 lines available)**:
  - Line 1: `Y = 14`
  - Line 2: `Y = 24`

---

## 📬 How to Submit Your Plugin

1. Open a new **Issue** or **Pull Request** in this repository.
2. Provide:
   - Plugin Name & Sensor Model.
   - Hardware requirements (I2C address, SPI pins, or GPIO requirements).
   - Required PlatformIO `lib_deps` (library name and version).
   - Your `PXXX.h` and `PXXX.cpp` files.
   - Test notes / verification logs confirming operation.
3. The AutoSense team will verify the build on ESP32 & ESP8266, validate functionality, and merge it into the next official AutoSense firmware release!

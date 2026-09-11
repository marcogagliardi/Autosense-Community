# AutoSense Community — Plugin Development Kit & Test Harness

Welcome to the **AutoSense Community** repository!

This repository contains the official plugin development kit for extending [AutoSense](https://github.com/marcogagliardi/Autosense) with new sensors, modules, and hardware integrations.

Because the core AutoSense firmware is maintained in a private build repository, external contributors develop and test plugins using this **self-contained PlatformIO Test Harness**. You can compile, flash to an ESP32 or ESP8266, and verify your sensor readings in real-time before submitting your code!

---

## 📁 Repository Structure

```
Autosense-Community/
├── platformio.ini              # PlatformIO environments for ESP32 and ESP8266
│
├── include/
│   ├── PluginInterface.h       # Base class and API contracts for AutoSense plugins
│   ├── PluginRegistry.h        # Self-registration factory interface
│   └── display_helpers.h       # OLED screen helper prototypes (coordinates & drawing)
│
├── src/
│   └── main.cpp                # 🚀 Test Harness Runner (simulates AutoSense runtime & loops)
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

## ⚡ How to Develop & Test on Hardware

You don't need the private core firmware to test your sensor code on a physical ESP32 or ESP8266. This repository is a ready-to-run PlatformIO project!

### Step 1: Clone the Repo & Open in VS Code / PlatformIO
```bash
git clone https://github.com/marcogagliardi/Autosense-Community.git
cd Autosense-Community
```
Open the folder in **VS Code** with the **PlatformIO IDE** extension installed.

### Step 2: Add your Plugin into `src/`
- Copy `TEMPLATE/PXXX.h` and `TEMPLATE/PXXX.cpp` into the `src/` directory.
- Rename them to your module name (e.g. `P012.h` and `P012.cpp`).
- *(Or test an existing example first: copy `examples/P002_DHT/*` into `src/`)*

### Step 3: Add Sensor Libraries to `platformio.ini`
In `platformio.ini`, add your sensor library under `lib_deps`. For example:
```ini
lib_deps =
    ${common.lib_deps}
    sparkfun/SparkFun SCD30 Arduino Library @ ^1.0.20
```

### Step 4: Flash & Monitor
1. Wire your sensor to your ESP32 or ESP8266 board.
2. Connect the board to your PC via USB.
3. In PlatformIO, select your environment (`esp32dev` or `nodemcuv2`) and click **Upload and Monitor**.
4. The Test Harness boots, registers your plugin, calls your `init()`, and runs your `read()` loop:
   ```text
   ==========================================================
        🚀 AutoSense Community — Plugin Test Harness        
   ==========================================================
   Target Board: ESP32
   Simulated OLED Height: 64 px

   --> Initializing Plugin [P012]...
     Enabled flag: module_P012 = 1
     Calling plugin->init()...
   ✅ Plugin [P012] (SCD30) initialized successfully!

   Starting sensor read loop (every 2000 ms)...
   [2000 ms] --- Reading P012 (SCD30) ---
   [P012] CO2: 420 ppm | Temp: 22.5 °C | Hum: 45.0 %
     [MOCK MQTT] Topic: 'AutoSense/SCD30/Data' | Value: '420' [ppm] | Metric: CO2
     [MOCK OLED] (X=0, Y=16, Size=1): "CO2: 420 ppm"
     [MOCK SSE] Event: 'P012update' -> {"co2": 420, "temp": 22.5}
   ```

---

## 📺 OLED Display Guidelines

When outputting readings to the onboard OLED display, use the helpers from `display_helpers.h`:

- **64px Screen (3 lines available)**:
  - Line 1: `Y = 16`
  - Line 2: `Y = 32`
  - Line 3: `Y = 50`
- **32px Screen (2 lines available)**:
  - Line 1: `Y = 14`
  - Line 2: `Y = 24`

---

## 📬 How to Submit Your Plugin

Once your plugin is tested and working on your hardware:

1. Go to the [AutoSense Community Discussions / Issues](https://github.com/marcogagliardi/Autosense-Community/discussions/2).
2. Share:
   - Plugin Name & Sensor Model.
   - Hardware requirements (I2C address, SPI pins, or GPIO requirements).
   - Required PlatformIO `lib_deps` entry.
   - Your `PXXX.h` and `PXXX.cpp` files.
   - A snippet of your Serial Monitor output confirming that readings are working.
3. The AutoSense team will verify the build against the private core and include it in the next official firmware release!

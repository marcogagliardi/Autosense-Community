# Autosense Firmware — Changelog

All notable changes to the Autosense firmware are documented here.  
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [2.0.0] — 2026-10-25

### Portal
- New web portal (Dashboard, Modules, Settings, System) replacing the server-rendered pages, written in Preact + TypeScript and built with Vite into three gzipped files on LittleFS (~22 KB). Data comes over a JSON API and one SSE stream.
- Settings forms, dashboard cards and calibration wizards are generated from the plugin description — a new plugin needs no portal code.
- Calibration wizard for P003, P006, P008, P011: waits for a stable value, captures each point, shows the result before saving, can be discarded.
- Field-level validation with messages (e.g. ESP32 analog pins must be GPIO 32–39, I²C addresses in hex).
- Modules are switched on/off and reconfigured without a reboot; a banner lists device settings that still need one.
- Light/dark mode, mobile layout, I²C scanner with device hints, configuration download.
- All colours in `web/src/theme/tokens.css`; custom dashboard cards with `registerCard()` in `web/src/cards/custom.tsx`.
- Portal tooling: `npm run dev` (hot reload against a simulated or real device), Vitest tests, type check, ESLint/Prettier; VS Code tasks in `.vscode/tasks.json`. PlatformIO rebuilds the portal automatically when `web/` changed (needs Node.js; without it the committed `data/*.gz` is used).

### Plugin SDK
- Plugins include only `sdk/include/AutoSensePlugin.h` and implement `describe()`, `begin()`, `read()` (+ optional calibration hooks). One `.cpp` per plugin in `src/Plugins/`.
- `sdk/tools/new_plugin.py` (scaffold) and `sdk/tools/check_plugin.py` (contract rules, schema preview, simulated reads/calibration without the core).
- Cross-plugin values through semantic kinds (`ctx.latest("liquid_temperature")`) instead of globals.

### Core
- Module settings stored as a compact, CRC-checked EEPROM block holding only non-default values (the 1.x JSON layout could overflow its 512-byte slot).
- One calibration engine shared by the portal, `/api/cal/*`, the legacy `/api/P006/calibration/*` routes and the MQTT hub; 15-minute idle timeout.
- OLED shows the most important values of all enabled modules (priority based) instead of modules overwriting each other's lines.
- I²C is started before the modules initialise (fixes the 1.0.1 known issue).
- ESP32: plugin state shared between the web server task and the main loop is locked; MQTT is only used from the main loop.
- Host test (`tools/hosttest/`) and mock device (`tools/mock-server/`) for development without hardware.
- Release tooling: `scripts/make_flash_kit.py` builds the end-user Flash Kit (firmware, bootloader, LittleFS image of the portal, flash and portal-update scripts, documentation from `flashkit/`); `scripts/sync_community.py` publishes `sdk/` and the example plugins to the Autosense-Community repository.

### Fixed
- P007 TDS: median filter swapped the wrong elements (`i++` in the comparison) and voltage used /1024 on the 12-bit ESP32 ADC.
- P007 TDS: OLED label showed "pH".
- P006 pH: 32 px OLED showed the raw ADC value with unit "ppm".
- P008 MQ-9: hung the device (`while(1)`) when R0 was 0; assumed a 10-bit ADC on ESP32; OLED label said "CO2"; temperature compensation used a stale value when no DHT was present.
- P004 DS18B20: a disconnected probe (-127 °C) is reported as an error instead of a reading.
- P005 O2: no more blocking retries with `delay(1000)` at boot.

### Compatibility
- Settings of 1.x (enabled modules, pins, calibration values, MQTT topics/metrics) are imported on the first boot.
- `A_index` settings became `adc_pin` (a GPIO number): P003/P007/P008 convert the old index (A0 + index), P006/P011 keep the stored GPIO. On ESP8266 the pin is always A0.
- MQTT topics, envelope and the hub capability manifest are unchanged (P006 still sends unit `ppm`, P002 still uses two topics).
- `plugins.json` is no longer used; every compiled plugin appears in the portal.
- SSE events are now `reading` and `cal`; the per-plugin events (`P002update`, `P006update-calibration`, ...) are gone.
- The `<code>_calibration` toggle settings were removed (the wizard replaces them).
- Removed: `PageManager`, `html_*`, `settings_handler`, `PluginManager`, `PluginRegistry`, `PluginSettingsManager`, `PluginInterface.h`, `PluginCapability.h`, `scanner`, `debug_EEPROM`, `page.html`, the per-plugin folders and `TEMPLATE/` (now `sdk/templates/PXXX.cpp`), unused `DFRobot_MICS` sources.

---

## [1.0.3] — Analog pin fixes for ESP32

On ESP32, some analog sensor plugins read from pins that stop working once Wi-Fi is on. The serial log filled with `ADC2 is in use by Wi-Fi` errors and the sensors reported wrong values. This release only reads valid analog pins and prints a single warning when a pin is set wrong.

### Fixed
**P006 (pH) and P011 (MiCS-5524)**: with the default analog setting `0`, both plugins read GPIO0 on ESP32. GPIO0 is on ADC2, which can't be read while Wi-Fi is on, so every sample failed with `ESP_ERR_TIMEOUT: ADC2 is in use by Wi-Fi`.
- **ESP32:** the setting is now the GPIO number and must be an ADC1 pin, GPIO32–39. Any other value is rejected at startup with one warning, and no analog read is attempted.
- **ESP8266:** the plugins always read `A0`, the board's only analog input.

### Improved
**P003 (MQ135), P007 (TDS), P008 (MQ-9)**: these plugins already worked with the default index `0`. Before, a higher index could point at a pin that doesn't exist or isn't connected. Now an invalid index is rejected with a warning and the sensor isn't read.
- **ESP32:** index 0–3 (GPIO36–39)
- **ESP8266:** index 0 only

### ⚠️ After updating: ESP32 boards with P006 or P011
Open the plugin settings and enter the GPIO number the sensor is wired to in **Analog Index**. GPIO34, 35, 36 and 39 are input-only and a good fit. If both sensors are connected, give each one its own pin. Until you change the setting, the plugin reads 0 V and the values it reports are not real.

### Valid analog settings
| Board | P006, P011 | P003, P007, P008 |
|---|---|---|
| ESP32 | GPIO number, 32–39 | Index 0–3 (GPIO36–39) |
| ESP8266 | Ignored, always A0 | Index 0 only (A0) |

On ESP32, never wire an analog sensor to an ADC2 pin (GPIO0, 2, 4, 12–15, 25–27). Those pins can't be read while Wi-Fi is on.

### Known issues
- If P003 or P008 has an invalid index, the "sensor pin is not set correctly" message repeats once per reading cycle.
- P008's copy of that message says "MQ135" when it should say "MQ-9". It's still the MQ-9 plugin.

---

## [1.0.2] — 2026-09-23

**Targets:** ESP32 (`esp32dev`) · ESP8266 (`nodemcuv2`)  
**Download:** `AutoSense_Flash_Kit_1.0.2.zip`

This release renames the firmware's MQTT topics to the **AutoSense** name, fixes OLED and I2C start-up problems, and removes the unfinished P009 plugin.

### ⚠️ Breaking change: MQTT topics renamed

The firmware name used to build MQTT topics changed from `SmartWine` to `Autosense`. **Any dashboard, Hub, Node-RED flow or broker ACL using the old topics must be updated.**

| Purpose | Before (1.0.1) | Now (1.0.2) |
|---------|----------------|-------------|
| Capability manifest (retained) | `SmartWineDevice/capabilities` | `AutosenseDevice/capabilities` |
| Device status / heartbeat / LWT | `SmartWineDevice/Status` | `AutosenseDevice/Status` |
| Per-device root (commands, calibration readings) | `smartwine/{mac}/...` | `autosense/{mac}/...` |

Examples of the new per-device topics:

```text
autosense/{mac}/cmd/reboot
autosense/{mac}/cmd/set_interval
autosense/{mac}/cmd/calibrate_ph
autosense/{mac}/cmd/calibrate_mics
autosense/{mac}/calibration/ph/reading
autosense/{mac}/calibration/mics/reading
```

> The retained manifest on the old `SmartWineDevice/capabilities` topic stays on the broker until it is cleared. To remove it, publish an empty retained message to that topic.

Plugin data topics (`_MQTT_SLOT1`, e.g. `%s/SENSOR/Data`) are set per plugin and **do not change**.

### 🐛 Fixes

#### I2C is initialised before plugins start
`Wire.begin()` now runs with your configured SDA/SCL pins **before** plugins are initialised. Before this fix, I2C plugins (OLED, O₂, BME688, MiCS-5524) could start on the wrong bus or fail to detect their hardware.  
*This fixes the boot-order known issue listed in 1.0.1.*

#### OLED (P001)
- **No more freeze at boot:** if the display fails to initialise, the firmware logs the error and continues. Before, it stopped in an endless loop and the whole device hung.
- **Uses your I2C address:** the duplicate initialisation hard-coded to `0x3C` is gone. The OLED now uses only the address set in the plugin settings.
- **Keeps your I2C pins:** the display library no longer resets the I2C bus to default pins, so custom SDA/SCL wiring keeps working.

### 🧹 Removed
- **P009 (ProtH)** has been removed from the firmware. The plugin ID is reserved for a future sensor.

### 🔧 Other changes
- The `plugins.json` included in the flash kit now enables **P001 (OLED)** and **P006 (pH)** by default (previously P001 and P002). To choose different plugins, upload your own `plugins.json` at `:1981/upload` and reboot.

### ⬆️ Upgrading from 1.0.1
1. Flash `1.0.2` using the flash kit scripts, or with OTA at `:1981/update`.
2. Update your Hub or dashboard subscriptions to the new `Autosense…` / `autosense/…` topics (see the table above).
3. If your Mosquitto broker uses ACLs, allow the new patterns:
   - subscribe: `AutosenseDevice/#`, `autosense/+/calibration/#`
   - publish: `autosense/+/cmd/#`
4. Optional: clear the old retained manifest on `SmartWineDevice/capabilities`.

Your settings (WiFi, MQTT broker, device name, GPIO pins, plugin settings) are stored in EEPROM and are kept after the upgrade.

---

## [1.0.1] — 2026-08-24

### Architecture
- Implemented plugin auto-registration via `__attribute__((constructor))` in all 11 plugins (P001–P011).  
  Adding a new plugin now requires only dropping `PXXX/PXXX.cpp` and `PXXX/PXXX.h` into `src/Plugins/` and recompiling — no other file in the project needs to be modified.
- Introduced `include/display_helpers.h` to decouple OLED display function declarations from the P001 plugin header.  
  Plugins that write to the OLED no longer carry a hard `#include "P001/P001.h"` dependency.
- Moved OLED (`SSD1306`) hardware initialisation out of `main.cpp` and into `P001::init()`.  
  P001 is now fully self-contained: if it is absent from `plugins.json` the firmware boots cleanly without an OLED.
- Added `src/Plugins/TEMPLATE/` — a copy-paste starting point (`PXXX.h` + `PXXX.cpp`) for new plugin development.

### Known Issues
- `PluginManager::initPlugins()` runs before `Wire.begin()` in the boot sequence, meaning `display.begin()` and other I2C-dependent plugin initialisations execute before I2C is configured. This is a pre-existing ordering issue shared across P001, P005, P010, and P011 — not a regression introduced in this release. Tracked for a future fix.

---

## [1.0.0] — 2026-08-23

### Initial Release
- ESP32 and ESP8266 firmware for multi-sensor data acquisition and publishing.
- WiFi station mode with fallback to Access Point mode for initial setup.
- Web UI served over `AsyncWebServer` on port 1980, OTA update server on port 1981.
- mDNS hostname support (`http://[device-name].local:1980`).
- MQTT publishing with capability manifest (`SmartWineDevice/capabilities`) published on connect.
- LittleFS-based configuration: `plugins.json` controls which plugins are active at boot.
- EEPROM-backed user settings (SSID, password, MQTT broker, device name, GPIO pins, sample frequency).
- Plugin system with `PluginInterface` base class, `PluginRegistry` factory, and `PluginManager` lifecycle orchestration.
- 11 built-in plugins:

  | ID   | Sensor / Module       |
  |------|-----------------------|
  | P001 | OLED display (SSD1306) |
  | P002 | DHT22 temperature & humidity |
  | P003 | MQ135 CO₂             |
  | P004 | DS18B20 temperature   |
  | P005 | DFRobot O₂            |
  | P006 | pH sensor with calibration |
  | P007 | TDS (total dissolved solids) |
  | P008 | CO sensor             |
  | P009 | ProtH                 |
  | P010 | BME688 environmental  |
  | P011 | MiCS-5524 gas (with calibration) |

- MQTT capability manifest with step-based calibration workflow support (`steps`, `readingTopic`, `readingFields`).
- I2C bus scanner endpoint (`/scanner`).
- SSE (Server-Sent Events) for live sensor readings in the web UI.
- Per-plugin enable/disable toggle via web UI with settings saved to EEPROM.

#include <Arduino.h>
#include <map>
#include <string>
#include <vector>
#include "PluginInterface.h"
#include "PluginRegistry.h"
#include "display_helpers.h"

// =============================================================================
// AutoSense Community — Plugin Test Harness Runner
// =============================================================================
// This test harness allows you to develop, compile, flash, and test your new
// AutoSense plugins on physical hardware (ESP32 or ESP8266) using PlatformIO.
//
// You do NOT need the private core firmware to test your sensor logic!
// =============================================================================

// Global settings stores (simulates AutoSense core settings manager)
std::map<std::string, float> pluginSettings;
std::map<std::string, String> pluginSettingsExtra;

// ─── PluginRegistry Implementation ───────────────────────────────────────────
PluginRegistry& PluginRegistry::getInstance() {
    static PluginRegistry instance;
    return instance;
}

void PluginRegistry::registerFactory(const std::string& id, PluginFactory factory) {
    factories[id] = factory;
}

Plugin* PluginRegistry::createPlugin(const std::string& id) {
    auto it = factories.find(id);
    if (it != factories.end()) {
        return it->second();
    }
    return nullptr;
}

// ─── Web UI Server-Sent Events (SSE) Mock ────────────────────────────────────
void SendSSEUpdate(const String& eventName, const String& jsonPayload) {
    Serial.print(F("  [MOCK SSE] Event: '"));
    Serial.print(eventName);
    Serial.print(F("' -> "));
    Serial.println(jsonPayload);
}

// ─── OLED Display Helpers Mock ───────────────────────────────────────────────
static int mockOledHeight = 64; // Set to 32 or 64 to simulate different displays

int GetOledHeight() {
    return mockOledHeight;
}

void DisplayText(const String text, int x, int y, int textSize) {
    Serial.printf("  [MOCK OLED] (X=%d, Y=%d, Size=%d): \"%s\"\n", x, y, textSize, text.c_str());
}

void DisplayClear() {}
void DisplayClearLine(int y, int lines, int lineLength) { (void)y; (void)lines; (void)lineLength; }

void DisplayMultipleLines(const std::vector<String>& lines) {
    for (size_t i = 0; i < lines.size(); i++) {
        Serial.printf("  [MOCK OLED] Line %u: \"%s\"\n", (unsigned int)(i + 1), lines[i].c_str());
    }
}

void DisplayVersion() {
    Serial.println(F("  [MOCK OLED] AutoSense Test Harness"));
}

// ─── Active Test Plugins List ────────────────────────────────────────────────
static std::vector<Plugin*> activePlugins;
static unsigned long lastReadTime = 0;
static const unsigned long READ_INTERVAL_MS = 2000; // Simulated read frequency

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println(F("=========================================================="));
    Serial.println(F("     🚀 AutoSense Community — Plugin Test Harness        "));
    Serial.println(F("=========================================================="));
    Serial.printf("Target Board: %s\n", 
#if defined(ESP32)
        "ESP32"
#elif defined(ESP8266)
        "ESP8266"
#else
        "Unknown"
#endif
    );
    Serial.printf("Simulated OLED Height: %d px\n", mockOledHeight);
    Serial.println();

    const auto& factories = PluginRegistry::getInstance().getFactories();

    if (factories.empty()) {
        Serial.println(F("⚠️  NO PLUGINS DETECTED IN src/!"));
        Serial.println(F("----------------------------------------------------------"));
        Serial.println(F("How to test your plugin:"));
        Serial.println(F("1. Copy your plugin files (.h and .cpp) into the 'src/' folder."));
        Serial.println(F("   (Or try an example: copy examples/P002_DHT/* into src/)"));
        Serial.println(F("2. Add any required sensor libraries into platformio.ini (lib_deps)."));
        Serial.println(F("3. Hit 'PlatformIO: Upload and Monitor' to test your sensor!"));
        Serial.println(F("----------------------------------------------------------"));
        return;
    }

    Serial.printf("Found %u registered plugin(s):\n", (unsigned int)factories.size());

    for (const auto& pair : factories) {
        const std::string& id = pair.first;
        Serial.printf("\n--> Initializing Plugin [%s]...\n", id.c_str());

        Plugin* plugin = pair.second();
        if (!plugin) {
            Serial.printf("❌ Error: Factory failed to instantiate [%s]!\n", id.c_str());
            continue;
        }

        // 1. Register default settings
        plugin->registerSettings(pluginSettings);
        plugin->registerSettingsExtra(pluginSettingsExtra);

        // 2. Automatically enable the plugin for testing
        std::string enableKey = "module_" + id;
        pluginSettings[enableKey] = 1.0f;

        // 3. Print loaded settings
        Serial.printf("  Enabled flag: %s = 1\n", enableKey.c_str());

        // 4. Call init()
        Serial.println(F("  Calling plugin->init()..."));
        plugin->init();
        Serial.printf("✅ Plugin [%s] (%s) initialized successfully!\n", id.c_str(), plugin->getModuleName().c_str());

        // 5. Preview Web UI Tile HTML
        String tileHtml = plugin->createHTMLTile();
        Serial.printf("  Web UI Tile size: %u bytes\n", (unsigned int)tileHtml.length());

        activePlugins.push_back(plugin);
    }

    Serial.println();
    Serial.println(F("=========================================================="));
    Serial.printf("Starting sensor read loop (every %lu ms)...\n", READ_INTERVAL_MS);
    Serial.println(F("Watch below for simulated sensor readings, MQTT, and OLED:"));
    Serial.println(F("==========================================================\n"));
}

void loop() {
    unsigned long now = millis();
    if (now - lastReadTime >= READ_INTERVAL_MS) {
        lastReadTime = now;

        if (activePlugins.empty()) {
            Serial.println(F("Waiting for plugin... (Place PXXX.h and PXXX.cpp into src/ and re-flash)"));
            delay(3000);
            return;
        }

        for (Plugin* plugin : activePlugins) {
            Serial.printf("\n[%lu ms] --- Reading %s (%s) ---\n", now, plugin->getModuleCode().c_str(), plugin->getModuleName().c_str());
            plugin->read();
        }
    }

    yield();
}

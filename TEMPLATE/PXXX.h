#pragma once
#include <Arduino.h>
#include <map>
#include <string>
#include "PluginInterface.h"

class PXXX : public Plugin {
public:
    PXXX();

    void init() override;
    void read() override;
    void registerSettings(std::map<std::string, float>& settings) override;
    void registerSettingsExtra(std::map<std::string, String>& settings) override;
    String createHTMLTile() override;
    String createJSFooter() override;
    String createReadingsHTML() override;
    String createHTMLSettingsForm() override;
    bool isEnabled() const override;

    // Optional overrides — remove if not needed:
    // void registerRoutes(AsyncWebServer& server) override;
    // void registerSubscriptions(PubSubClient& client) override;
    // bool handleMqttMessage(const String& topic, const String& payload) override;
    // void getCapabilities(std::vector<PluginCapability>& caps) const override;

private:
    static constexpr const char* MODULE_NAME = "YourSensorName";
    static constexpr const char* MODULE_CODE = "PXXX";
};

// Do NOT declare a global instance here (e.g. `extern PXXX pxxx;`).
// The plugin is owned by PluginRegistry/PluginManager via the factory
// registered in PXXX.cpp — a global instance is dead weight left over
// from the pre-factory pattern. If you need to reach this plugin from
// another plugin, add an accessor function to the module instead
// (see GetOledHeight() in display_helpers.h for the pattern).

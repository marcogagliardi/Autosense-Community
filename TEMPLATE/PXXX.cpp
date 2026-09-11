#include "PXXX.h"
#include "PluginInterface.h"
#include "PluginRegistry.h"
#include "display_helpers.h"   // For DisplayText() etc. — remove if not writing to OLED

// ─── Self-registration ─────────────────────────────────────────────────────
// This is the ONLY wiring required. No other file needs to be modified.
__attribute__((constructor)) void registerPXXXFactory() {
    PluginRegistry::getInstance().registerFactory("PXXX", []() -> Plugin* {
        return new PXXX();
    });
}
// ───────────────────────────────────────────────────────────────────────────

PXXX::PXXX() : Plugin(MODULE_NAME, MODULE_CODE) {}

void PXXX::init() {
    // Called once at boot. Set up hardware, configure pins, etc.
    if (!isEnabled()) return;
}

void PXXX::read() {
    // Called every `user_settings.frequency` ms.
    // Read sensor, publish via MQTTsend(), update SSE, etc.
}

void PXXX::registerSettings(std::map<std::string, float>& settings) {
    settings["module_PXXX"] = 0;   // 0 = disabled by default
    // Add other float settings here
}

void PXXX::registerSettingsExtra(std::map<std::string, String>& settings) {
    // Add String settings here if needed
    (void)settings;
}

bool PXXX::isEnabled() const {
    return pluginSettings.count("module_PXXX") &&
           pluginSettings.at("module_PXXX") == 1;
}

String PXXX::createHTMLTile() {
    String html = createTileHeader();
    // TODO: add sensor-specific tile content
    html += F("</div>");
    return html;
}

String PXXX::createJSFooter()         { return ""; }
String PXXX::createReadingsHTML()     { return createReadings("Label", "N/A"); }
String PXXX::createHTMLSettingsForm() { return openFormForUserSettings() + closeFormForUserSettings(); }

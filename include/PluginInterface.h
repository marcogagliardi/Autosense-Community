#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <string>
#include <vector>

// Forward declarations for optional network handlers
class AsyncWebServer;
class PubSubClient;

// Global settings maps provided by the AutoSense core runtime
extern std::map<std::string, float> pluginSettings;
extern std::map<std::string, String> pluginSettingsExtra;

// Helper function to dispatch Server-Sent Events (SSE) updates to the web dashboard
void SendSSEUpdate(const String& eventName, const String& jsonPayload);

class Plugin
{
protected:
    char moduleName[32];
    char key[32];
    char moduleCode[32];

public:
    Plugin(const char *moduleName, const char *moduleCode)
    {
        strncpy(this->moduleName, moduleName, sizeof(this->moduleName) - 1);
        this->moduleName[sizeof(this->moduleName) - 1] = '\0';

        snprintf(this->key, sizeof(this->key), "module_%s", moduleCode);

        strncpy(this->moduleCode, moduleCode, sizeof(this->moduleCode) - 1);
        this->moduleCode[sizeof(this->moduleCode) - 1] = '\0';
    }

    virtual ~Plugin() = default;

    // --- Lifecycle hooks ---
    virtual void init() = 0;
    virtual void read() = 0;
    virtual bool isEnabled() const = 0;

    // --- Configuration hooks ---
    virtual void registerSettings(std::map<std::string, float> &settings) = 0;
    virtual void registerSettingsExtra(std::map<std::string, String> &settings) = 0;

    // --- Web UI hooks ---
    virtual String createHTMLTile() = 0;
    virtual String createReadingsHTML() = 0;
    virtual String createHTMLSettingsForm() = 0;
    virtual String createJSFooter() = 0;

    // --- Accessors ---
    virtual String getModuleName() const { return String(moduleName); }
    virtual String getModuleCode() const { return String(moduleCode); }

    // --- Optional overrides ---
    virtual void registerRoutes(AsyncWebServer &server) { (void)server; }
    virtual void registerSubscriptions(PubSubClient &mqttClient) { (void)mqttClient; }
    virtual bool handleMqttMessage(const String &topic, const String &payload) { (void)topic; (void)payload; return false; }

    // --- UI & Helper Generators ---
    String createReadings(String label, String valueStr)
    {
        String readingsStr = "";
        if (isEnabled())
        {
            readingsStr += String(F("    <div id='")) + String(moduleCode) + String(F("'>"));
            readingsStr += String(F("        <span class=''> <label>")) + label + String(F(":</label>"));
            readingsStr += String(F("            <span id='")) + String(moduleCode) + String(F("Value' class='readings-value'>"));
            readingsStr += valueStr;
            readingsStr += String(F("</span></span></div>"));
        }
        return readingsStr;
    }

    String createTileHeader(bool calibratingButton = false)
    {
        String tileHTML;
        tileHTML.reserve(1024);

        tileHTML += String(F("<div class='tile'><div class='tile-header'>"));
        tileHTML += String(F("<h2>")) + String(moduleName) + String(F(" Module</h2>"));
        if (isEnabled())
        {
            tileHTML += String(F("<button class='edit-settings-button' onclick=\"toggleForm('")) + String(moduleCode) + String(F("-form')\">Edit</button>"));
            if (calibratingButton)
            {
                tileHTML += String(F("<button class='edit-settings-button' onclick=\"calibrating")) + String(moduleCode) + String(F("()\">Calibration</button>"));
            }
        }
        String module_Toggle = (isEnabled()) ? F(" checked") : F("");

        tileHTML += String(F("<label class='switch'><input type='checkbox' id='")) + String(moduleCode) + String(F("-toggle' onclick=\"toggle('")) + String(moduleCode) + String(F("')\"")) + module_Toggle + String(F(">"));
        tileHTML += String(F("<span class='slider'></span></label></div>"));
        tileHTML += String(F("<span class='plugin-info'>")) + String(moduleCode) + String(F("</span>"));
        return tileHTML;
    }

    String openFormForUserSettings()
    {
        return String(F("<div id='")) + String(moduleCode) + String(F("-form' style='display:none;'><form action='/update-settings' method='get'>"));
    }

    String closeFormForUserSettings()
    {
        return String(F("<br><button type='submit'>Save</button></form></div>"));
    }

    String createJSEventListener(String symbol)
    {
        if (!isEnabled()) return "";
        String js = F("eventSource.addEventListener('");
        js += String(moduleCode) + F("update', function (event) {\n");
        js += F("    const parsedData = JSON.parse(event.data);\n");
        js += F("    const value = parsedData.value;\n");
        js += String(F("    document.getElementById('")) + String(moduleCode) + String(F("Value').innerText = `${value}")) + symbol + String(F("`;\n});\n"));
        return js;
    }

    String createJSDisplay()
    {
        if (!isEnabled()) return "";
        String js = String(F("function display")) + String(moduleCode) + String(F("() {\n"));
        js += String(F("  const readingsDiv = document.getElementById('")) + String(moduleCode) + String(F("');\n"));
        js += String(F("  readingsDiv.style.display = userSettings.pluginSettings.")) + String(key) + String(F(" === 1 ? 'block' : 'none';\n}\n"));
        return js;
    }

    // Publishes telemetry to the MQTT broker.
    // In this Community Test Harness, this prints the simulated MQTT payload to Serial.
    // In the official firmware, the core runtime dispatches this to the real MQTT broker.
    virtual void MQTTsend(String topic, String value, String unit = "ppm", String metric = "", String sensorType = "AutosenseType", String location = "Lab 1")
    {
        Serial.print(F("  [MOCK MQTT] Topic: '"));
        Serial.print(topic);
        Serial.print(F("' | Value: '"));
        Serial.print(value);
        Serial.print(F("'"));
        if (unit.length() > 0) {
            Serial.print(F(" ["));
            Serial.print(unit);
            Serial.print(F("]"));
        }
        if (metric.length() > 0) {
            Serial.print(F(" | Metric: "));
            Serial.print(metric);
        }
        Serial.println();
    }
};

#include "P010.h"
#include "PluginInterface.h"
#include "PluginRegistry.h"
#include "display_helpers.h"

#define SEALEVELPRESSURE_HPA (1013.25)

// ─── Self-Registration ────────────────────────────────────────────────────────
__attribute__((constructor)) void registerP010Factory()
{
    PluginRegistry::getInstance().registerFactory("P010", []() -> Plugin * {
        return new P010();
    });
}
// ─────────────────────────────────────────────────────────────────────────────

P010::P010() : Plugin(MODULE_NAME, MODULE_CODE)
{
}

void P010::init()
{
    if (isEnabled())
    {
        setupSensor();
    }
}

void P010::read()
{
    if (isEnabled())
    {
        readSensor();
    }
}

void P010::registerSettings(std::map<std::string, float> &settings)
{
    if (settings.find(key) == settings.end() || (settings[key] != 0.0f && settings[key] != 1.0f))
    {
        settings[key] = 0.0f; // Disabled by default
    }

    char i2cAddrKey[32];
    snprintf(i2cAddrKey, sizeof(i2cAddrKey), "%s_i2c_addr", moduleCode);
    if (settings.find(i2cAddrKey) == settings.end())
    {
        settings[i2cAddrKey] = 119.0f; // Default I2C address is 0x77 (119 in decimal)
    }
}

void P010::registerSettingsExtra(std::map<std::string, String> &settings)
{
    char mqttSlot1[32];
    snprintf(mqttSlot1, sizeof(mqttSlot1), "%s_MQTT_SLOT1", moduleCode);

    if (settings.find(mqttSlot1) == settings.end())
    {
        settings[mqttSlot1] = "AutoSense/BME688/Data";
    }

    char metric1Key[32];
    snprintf(metric1Key, sizeof(metric1Key), "%s_METRIC1", moduleCode);
    if (settings.find(metric1Key) == settings.end())
    {
        settings[metric1Key] = MODULE_NAME;
    }
}

bool P010::isEnabled() const
{
    return pluginSettings.count(key) && pluginSettings.at(key) != 0.0f;
}

String P010::createHTMLSettingsForm()
{
    char i2cAddrKey[32];
    snprintf(i2cAddrKey, sizeof(i2cAddrKey), "%s_i2c_addr", MODULE_CODE);
    char i2cAddrVal[16];
    snprintf(i2cAddrVal, sizeof(i2cAddrVal), "%d", (int)pluginSettings[i2cAddrKey]);

    char mqttSlot1[32];
    snprintf(mqttSlot1, sizeof(mqttSlot1), "%s_MQTT_SLOT1", moduleCode);

    String formHTML;
    formHTML.reserve(1024);
    formHTML = String(F("<h2>")) + moduleName + String(F(" Settings</h2>"));

    formHTML += String(F("<label class='label' for='")) + i2cAddrKey + String(F("'>I2C Address (decimal, 119=0x77, 118=0x76): </label>"));
    formHTML += String(F("<input type='text' id='")) + i2cAddrKey + String(F("' name='")) + i2cAddrKey + String(F("' class='form-control' placeholder='119' value='")) + String(i2cAddrVal) + String(F("'>"));
    formHTML += String(F("<br>"));

    formHTML += String(F("<label class='label' for='")) + mqttSlot1 + String(F("'>MQTT TOPIC: </label>"));
    formHTML += String(F("<input type='text' id='")) + mqttSlot1 + String(F("' name='")) + mqttSlot1 + String(F("' class='form-control' value='")) + pluginSettingsExtra[mqttSlot1] + String(F("' />"));
    formHTML += String(F("<br>"));

    return formHTML;
}

String P010::createHTMLTile()
{
    String tileHTML;
    tileHTML.reserve(2048);
    tileHTML += Plugin::createTileHeader();
    if (isEnabled())
    {
        char i2cAddrKey[32];
        snprintf(i2cAddrKey, sizeof(i2cAddrKey), "%s_i2c_addr", MODULE_CODE);
        tileHTML += String(F("<p><span class='label'>I2C Address:</span> 0x")) + String((int)pluginSettings[i2cAddrKey], HEX) + String(F("</p>"));

        tileHTML += Plugin::openFormForUserSettings();
        tileHTML += createHTMLSettingsForm();
        tileHTML += Plugin::closeFormForUserSettings();
    }
    tileHTML += String(F("</div>"));
    return tileHTML;
}

String P010::createReadingsHTML()
{
    String readingsStr = "";
    if (isEnabled())
    {
        readingsStr += String(F("    <div id='")) + String(moduleCode) + String(F("'>"));
        readingsStr += String(F("        <span class=''> <label>BME688</label>: "));
        readingsStr += String(F("            <span id='bme688_temp' class='readings-value'>")) + (isnan(latestTemperature) ? "N/A" : String(latestTemperature, 1) + " °C") + String(F("</span> / "));
        readingsStr += String(F("            <span id='bme688_hum' class='readings-value'>")) + (isnan(latestHumidity) ? "N/A" : String(latestHumidity, 1) + " %") + String(F("</span> / "));
        readingsStr += String(F("            <span id='bme688_pres' class='readings-value'>")) + (isnan(latestPressure) ? "N/A" : String(latestPressure, 0) + " hPa") + String(F("</span> / "));
        readingsStr += String(F("            <span id='bme688_gas' class='readings-value'>")) + (isnan(latestGasResistance) ? "N/A" : String(latestGasResistance / 1000.0f, 1) + " KOhms") + String(F("</span>"));
        readingsStr += String(F("        </span>"));
        readingsStr += String(F("    </div>"));
    }
    return readingsStr;
}

String P010::createJSFooter()
{
    String jsEvents = "";
    if (!isEnabled()) return jsEvents;

    jsEvents += Plugin::createJSDisplay();
    jsEvents += String(F("eventSource.addEventListener('"));
    jsEvents += String(moduleCode);
    jsEvents += String(F("update', function (event) {\n"));
    jsEvents += String(F("    const parsedData = JSON.parse(event.data);\n"));
    jsEvents += String(F("    document.getElementById('bme688_temp').innerText = `${parsedData.temp} °C`;\n"));
    jsEvents += String(F("    document.getElementById('bme688_hum').innerText = `${parsedData.hum} %`;\n"));
    jsEvents += String(F("    document.getElementById('bme688_pres').innerText = `${parsedData.pres} hPa`;\n"));
    jsEvents += String(F("    document.getElementById('bme688_gas').innerText = `${parsedData.gas} KOhms`;\n"));
    jsEvents += String(F("});\n"));
    return jsEvents;
}

void P010::setupSensor()
{
    char i2cAddrKey[32];
    snprintf(i2cAddrKey, sizeof(i2cAddrKey), "%s_i2c_addr", moduleCode);
    uint8_t address = (uint8_t)pluginSettings[i2cAddrKey];

    if (!bme.begin(address))
    {
        Serial.printf("[P010] Error: Could not find BME688 at address 0x%02X!\n", address);
        isInitialized = false;
        return;
    }

    // Set up oversampling and filter configuration
    bme.setTemperatureOversampling(BME680_OS_8X);
    bme.setHumidityOversampling(BME680_OS_2X);
    bme.setPressureOversampling(BME680_OS_4X);
    bme.setIIRFilterSize(BME680_FILTER_SIZE_3);
    bme.setGasHeater(320, 150); // 320°C for 150 ms

    isInitialized = true;
    Serial.printf("[P010] BME688 initialized successfully at 0x%02X.\n", address);
}

void P010::readSensor()
{
    if (!isInitialized)
    {
        setupSensor();
        if (!isInitialized) return;
    }

    if (!bme.performReading())
    {
        Serial.println(F("[P010] Failed to perform BME688 reading."));
        return;
    }

    float t = bme.temperature;
    float h = bme.humidity;
    float p = bme.pressure / 100.0f; // Pa to hPa
    float g = bme.gas_resistance / 1000.0f; // Ohms to KOhms

    latestTemperature = t;
    latestHumidity = h;
    latestPressure = p;
    latestGasResistance = g;

    Serial.printf("[P010] Temp: %.2f °C, Hum: %.2f %%, Pres: %.2f hPa, Gas: %.2f KOhms\n", t, h, p, g);

    // Build JSON payload for MQTT telemetry
    char jsonBuffer[256];
    snprintf(jsonBuffer, sizeof(jsonBuffer),
             "{\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"gas_resistance\":%.2f}",
             t, h, p, g);

    char mqttSlot1[32];
    snprintf(mqttSlot1, sizeof(mqttSlot1), "%s_MQTT_SLOT1", moduleCode);
    char metric1Key[32];
    snprintf(metric1Key, sizeof(metric1Key), "%s_METRIC1", moduleCode);

    MQTTsend(pluginSettingsExtra[mqttSlot1], jsonBuffer, "multi", pluginSettingsExtra[metric1Key], getModuleName(), "Environmental");

    yield();

    // OLED display output
    if (GetOledHeight() == 32)
    {
        DisplayClearLine(14);
        DisplayText("T:" + String(t, 1) + "C H:" + String(h, 0) + "%", 0, 14);

        DisplayClearLine(24);
        DisplayText("P:" + String(p, 0) + " G:" + String(g, 1) + "K", 0, 24);
    }
    else if (GetOledHeight() == 64)
    {
        DisplayClearLine(16, 2);
        DisplayText("T", 0, 16, 1);
        DisplayText(String(t, 1) + "\xB0" + "C", 12, 16, 2);

        DisplayClearLine(32, 2);
        DisplayText("H", 0, 32, 1);
        DisplayText(String(h, 1) + "%", 12, 32, 2);

        DisplayClearLine(50, 2);
        DisplayText("G", 0, 50, 1);
        DisplayText(String(g, 1) + " KOhms", 12, 50, 2);
    }

    yield();

    // Send Web UI updates
    sendSSEUpdates(t, h, p, g);
}

void P010::sendSSEUpdates(float temp, float hum, float pres, float gas)
{
    char tempStr[8];
    char humStr[8];
    char presStr[10];
    char gasStr[10];

    snprintf(tempStr, sizeof(tempStr), "%.2f", temp);
    snprintf(humStr, sizeof(humStr), "%.2f", hum);
    snprintf(presStr, sizeof(presStr), "%.2f", pres);
    snprintf(gasStr, sizeof(gasStr), "%.2f", gas);

    JsonDocument json;
    json["temp"] = tempStr;
    json["hum"] = humStr;
    json["pres"] = presStr;
    json["gas"] = gasStr;

    String jsonString;
    serializeJson(json, jsonString);

    SendSSEUpdate(String(moduleCode) + "update", jsonString);
}

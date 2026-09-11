#include "P002.h"
#include "PluginInterface.h"
#include "PluginRegistry.h"
#include "display_helpers.h"

// ─── Self-Registration ────────────────────────────────────────────────────────
__attribute__((constructor)) void registerP002Factory()
{
    PluginRegistry::getInstance().registerFactory("P002", []() -> Plugin * {
        return new P002();
    });
}
// ─────────────────────────────────────────────────────────────────────────────

P002::P002() : Plugin(MODULE_NAME, MODULE_CODE)
{
}

P002::~P002()
{
    if (dht) {
        delete dht;
        dht = nullptr;
    }
}

void P002::init()
{
    if (isEnabled())
    {
        setupSensor();
    }
}

void P002::read()
{
    if (isEnabled())
    {
        readSensor();
    }
}

void P002::registerSettings(std::map<std::string, float> &settings)
{
    if (settings.find(key) == settings.end() || (settings[key] != 0.0f && settings[key] != 1.0f))
    {
        settings[key] = 0.0f; // Disabled by default
    }

    char gpio[32];
    snprintf(gpio, sizeof(gpio), "%s_gpio_pin", MODULE_CODE);
    if (settings.find(gpio) == settings.end())
    {
        settings[gpio] = 14.0f; // Default GPIO pin (D5 on NodeMCU ESP8266)
    }

    char dhtTypeKey[32];
    snprintf(dhtTypeKey, sizeof(dhtTypeKey), "%s_dht_type", MODULE_CODE);
    if (settings.find(dhtTypeKey) == settings.end())
    {
        settings[dhtTypeKey] = 22.0f; // Default to DHT22
    }
}

void P002::registerSettingsExtra(std::map<std::string, String> &settings)
{
    char mqttSlot1[32];
    snprintf(mqttSlot1, sizeof(mqttSlot1), "%s_MQTT_SLOT1", moduleCode);
    char mqttSlot2[32];
    snprintf(mqttSlot2, sizeof(mqttSlot2), "%s_MQTT_SLOT2", moduleCode);

    if (settings.find(mqttSlot1) == settings.end())
    {
        settings[mqttSlot1] = "AutoSense/DHT/Temperature";
    }
    if (settings.find(mqttSlot2) == settings.end())
    {
        settings[mqttSlot2] = "AutoSense/DHT/Humidity";
    }

    char metric1Key[32];
    snprintf(metric1Key, sizeof(metric1Key), "%s_METRIC1", moduleCode);
    char metric2Key[32];
    snprintf(metric2Key, sizeof(metric2Key), "%s_METRIC2", moduleCode);

    if (settings.find(metric1Key) == settings.end())
    {
        settings[metric1Key] = "Temperature";
    }
    if (settings.find(metric2Key) == settings.end())
    {
        settings[metric2Key] = "Humidity";
    }
}

bool P002::isEnabled() const
{
    return pluginSettings.count(key) && pluginSettings.at(key) != 0.0f;
}

String P002::createHTMLSettingsForm()
{
    String gpio = String(moduleCode) + "_gpio_pin";
    String mqttSlot1 = String(moduleCode) + "_MQTT_SLOT1";
    String mqttSlot2 = String(moduleCode) + "_MQTT_SLOT2";

    char gpioPin[8];
    snprintf(gpioPin, sizeof(gpioPin), "%d", (int)pluginSettings[gpio.c_str()]);

    String formHTML;
    formHTML.reserve(1024);
    formHTML = String(F("<h2>")) + moduleName + String(F(" Settings</h2>"));
    formHTML += String(F("<label class='label' for='")) + gpio + String(F("'>GPIO Pin: </label>"));
    formHTML += String(F("<input type='text' id='")) + gpio + String(F("' name='")) + gpio + String(F("' class='form-control' placeholder='Enter GPIO Pin' value='")) + String(gpioPin) + String(F("'>"));
    formHTML += String(F("<br>"));

    formHTML += String(F("<label class='label' for='")) + mqttSlot1 + String(F("'>MQTT TEMP TOPIC: </label>"));
    formHTML += String(F("<input type='text' id='")) + mqttSlot1 + String(F("' name='")) + mqttSlot1 + String(F("' class='form-control' value='")) + pluginSettingsExtra[mqttSlot1.c_str()] + String(F("' />"));
    formHTML += String(F("<br>"));

    formHTML += String(F("<label class='label' for='")) + mqttSlot2 + String(F("'>MQTT HUM TOPIC: </label>"));
    formHTML += String(F("<input type='text' id='")) + mqttSlot2 + String(F("' name='")) + mqttSlot2 + String(F("' class='form-control' value='")) + pluginSettingsExtra[mqttSlot2.c_str()] + String(F("' />"));
    formHTML += String(F("<br>"));

    String metric1Key = String(moduleCode) + "_METRIC1";
    String metric2Key = String(moduleCode) + "_METRIC2";
    formHTML += String(F("<label class='label' for='")) + metric1Key + String(F("'>METRIC 1 NAME: </label>"));
    formHTML += String(F("<input type='text' id='")) + metric1Key + String(F("' name='")) + metric1Key + String(F("' class='form-control' value='")) + pluginSettingsExtra[metric1Key.c_str()] + String(F("' />"));
    formHTML += String(F("<br>"));
    formHTML += String(F("<label class='label' for='")) + metric2Key + String(F("'>METRIC 2 NAME: </label>"));
    formHTML += String(F("<input type='text' id='")) + metric2Key + String(F("' name='")) + metric2Key + String(F("' class='form-control' value='")) + pluginSettingsExtra[metric2Key.c_str()] + String(F("' />"));
    formHTML += String(F("<br>"));

    String dhtTypeKey = String(moduleCode) + "_dht_type";
    int currentType = (int)pluginSettings[dhtTypeKey.c_str()];
    formHTML += String(F("<label class='label' for='")) + dhtTypeKey + String(F("'>DHT Type: </label>"));
    formHTML += String(F("<select id='")) + dhtTypeKey + String(F("' name='")) + dhtTypeKey + String(F("' class='form-control'>"));
    formHTML += String(F("<option value='11'")) + (currentType == 11 ? String(F(" selected")) : String(F(""))) + String(F(">DHT11</option>"));
    formHTML += String(F("<option value='22'")) + (currentType == 22 ? String(F(" selected")) : String(F(""))) + String(F(">DHT22</option>"));
    formHTML += String(F("</select>"));

    return formHTML;
}

String P002::createHTMLTile()
{
    String tileHTML;
    tileHTML.reserve(2048);
    tileHTML += Plugin::createTileHeader();
    if (isEnabled())
    {
        char gpio[32];
        snprintf(gpio, sizeof(gpio), "%s_gpio_pin", MODULE_CODE);
        tileHTML += String(F("<p><span class='label'>GPIO Pin:</span> ")) + String((int)pluginSettings[gpio]) + String(F("</p>"));

        tileHTML += Plugin::openFormForUserSettings();
        tileHTML += createHTMLSettingsForm();
        tileHTML += Plugin::closeFormForUserSettings();
    }
    tileHTML += String(F("</div>"));
    return tileHTML;
}

String P002::createReadingsHTML()
{
    String temperature = isnan(latestTemperature) ? "N/A" : String(latestTemperature, 2) + " °C";
    String humidity = isnan(latestHumidity) ? "N/A" : String(latestHumidity, 2) + " %";
    String readingsStr = "";
    if (isEnabled())
    {
        readingsStr += String(F("    <div id='")) + String(moduleCode) + String(F("'>"));
        readingsStr += String(F("        <span class=''> <label>Temperature</label>"));
        readingsStr += String(F("            <span id='temperature' class='readings-value'>")) + temperature + String(F("</span>"));
        readingsStr += String(F("        </span>"));
        readingsStr += String(F("        <span class=''><label> / Humidity</label>"));
        readingsStr += String(F("            <span id='humidity' class='readings-value'>")) + humidity + String(F("</span>"));
        readingsStr += String(F("        </span>"));
        readingsStr += String(F("    </div>"));
    }
    return readingsStr;
}

String P002::createJSFooter()
{
    String jsEvents = "";
    if (!isEnabled()) return jsEvents;

    jsEvents += Plugin::createJSDisplay();
    jsEvents += String(F("eventSource.addEventListener('"));
    jsEvents += String(moduleCode);
    jsEvents += String(F("update', function (event) {\n"));
    jsEvents += String(F("    const parsedData = JSON.parse(event.data);\n"));
    jsEvents += String(F("    const temperatureValue = parsedData.temperature;\n"));
    jsEvents += String(F("    const humidityValue = parsedData.humidity;\n"));
    jsEvents += String(F("    document.getElementById('temperature').innerText = `${temperatureValue}°`;\n"));
    jsEvents += String(F("    document.getElementById('humidity').innerText = `${humidityValue}%`;\n"));
    jsEvents += String(F("});\n"));
    return jsEvents;
}

void P002::setupSensor()
{
    char gpioKey[32];
    snprintf(gpioKey, sizeof(gpioKey), "%s_gpio_pin", moduleCode);
    int gpioPin = (int)pluginSettings[gpioKey];

    char dhtTypeKey[32];
    snprintf(dhtTypeKey, sizeof(dhtTypeKey), "%s_dht_type", moduleCode);
    int dhtType = (int)pluginSettings[dhtTypeKey];
    if (dhtType != 11 && dhtType != 22) {
        dhtType = 22;
    }

    if (dht != nullptr) {
        delete dht;
    }
    dht = new DHT_Unified(gpioPin, dhtType);
    dht->begin();
    Serial.println(F("[P002] DHT sensor initialized."));
}

void P002::readSensor()
{
    if (dht == nullptr) return;

    sensors_event_t event;
    float t = nanf("");
    float h = nanf("");

    dht->temperature().getEvent(&event);
    if (!isnan(event.temperature)) {
        t = event.temperature;
    }

    dht->humidity().getEvent(&event);
    if (!isnan(event.relative_humidity)) {
        h = event.relative_humidity;
    }

    latestTemperature = t;
    latestHumidity = h;

    char tempStr[8];
    char humStr[8];
    dtostrf(t, 5, 2, tempStr);
    dtostrf(h, 5, 2, humStr);

    Serial.printf("[P002] Temp: %.2f °C, Hum: %.2f %%\n", t, h);

    // Publish to MQTT
    String mqttSlot1 = String(moduleCode) + "_MQTT_SLOT1";
    String mqttSlot2 = String(moduleCode) + "_MQTT_SLOT2";
    String metric1Key = String(moduleCode) + "_METRIC1";
    String metric2Key = String(moduleCode) + "_METRIC2";
    MQTTsend(pluginSettingsExtra[mqttSlot1.c_str()], tempStr, "C", pluginSettingsExtra[metric1Key.c_str()], getModuleName(), "Temperature");
    MQTTsend(pluginSettingsExtra[mqttSlot2.c_str()], humStr, "%", pluginSettingsExtra[metric2Key.c_str()], getModuleName(), "Humidity");

    yield();

    // OLED display output
    if (GetOledHeight() == 32)
    {
        DisplayClearLine(14);
        DisplayText("TEMP: " + String(t, 1) + "\xB0" + "C", 0, 14);

        DisplayClearLine(24);
        DisplayText("HUM:  " + String(h, 1) + "%", 0, 24);
    }
    else if (GetOledHeight() == 64)
    {
        DisplayClearLine(16, 2);
        DisplayText("T", 0, 16, 1);
        DisplayText(String(t, 1) + "\xB0" + "C", 12, 16, 2);

        DisplayClearLine(32, 2);
        DisplayText("H", 0, 32, 1);
        DisplayText(String(h, 1) + "%", 12, 32, 2);
    }

    yield();

    // Send Web UI update
    sendSSEUpdates(t, h);
}

void P002::sendSSEUpdates(float temp, float hum)
{
    char tempStr[8];
    char humStr[8];
    snprintf(tempStr, sizeof(tempStr), "%.2f", temp);
    snprintf(humStr, sizeof(humStr), "%.2f", hum);

    JsonDocument json;
    json["temperature"] = tempStr;
    json["humidity"] = humStr;

    String jsonString;
    serializeJson(json, jsonString);

    SendSSEUpdate(String(moduleCode) + "update", jsonString);
}

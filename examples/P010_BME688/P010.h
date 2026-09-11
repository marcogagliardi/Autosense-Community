#pragma once

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>
#include "PluginInterface.h"

class P010 : public Plugin {
public:
    P010();
    virtual ~P010() = default;

    void init() override;
    void read() override;

    void registerSettings(std::map<std::string, float>& settings) override;
    void registerSettingsExtra(std::map<std::string, String>& settings) override;

    String createHTMLTile() override;
    String createJSFooter() override;
    String createReadingsHTML() override;
    String createHTMLSettingsForm() override;
    bool isEnabled() const override;

private:
    static constexpr const char *MODULE_NAME = "BME688";
    static constexpr const char *MODULE_CODE = "P010";

    Adafruit_BME680 bme;
    bool isInitialized = false;

    float latestTemperature = 0.0f;
    float latestHumidity = 0.0f;
    float latestPressure = 0.0f;
    float latestGasResistance = 0.0f;

    void setupSensor();
    void readSensor();
    void sendSSEUpdates(float temp, float hum, float pres, float gas);
};

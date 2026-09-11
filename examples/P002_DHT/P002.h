#pragma once

#include <Arduino.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#include "PluginInterface.h"

class P002 : public Plugin {
public:
    P002();
    virtual ~P002();

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
    static constexpr const char *MODULE_NAME = "DHT";
    static constexpr const char *MODULE_CODE = "P002";

    DHT_Unified *dht = nullptr;
    float latestTemperature = 0.0f;
    float latestHumidity = 0.0f;

    void setupSensor();
    void readSensor();
    void sendSSEUpdates(float temp, float hum);
};

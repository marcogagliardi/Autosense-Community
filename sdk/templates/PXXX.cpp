// =============================================================================
//  PXXX  —  <Sensor name>
//  <One line: what it measures and how it is wired.>
//
//  Author:  <you>          License: <same as AutoSense>
//
//  This file is the whole plugin.  It includes ONLY <AutoSensePlugin.h> (plus
//  the sensor's own library).  Check it with:
//      python sdk/tools/check_plugin.py <path to this file>
// =============================================================================
#include <AutoSensePlugin.h>
// #include <SomeSensorLibrary.h>

class PXXX : public autosense::Plugin
{
public:
    // ---- 1. describe: settings, readings, (calibration) -----------------------
    void describe(autosense::Descriptor &d) override
    {
        //      wire name (MQTT sensorType)   title in the portal   one-line summary          bus
        d.info("MYSENSOR", F("My sensor"), F("What it measures"), autosense::Bus::Analog);
        d.hint(F("Optional wiring tip shown in the settings panel."));

        // settings -> generated portal form, validated by the core, stored in EEPROM
        d.pin("adc_pin", F("Analog pin (GPIO)"), A0).adc1();
        d.number("factor", F("Scale factor"), 1.0f).range(0.01f, 100).step(0.01f);

        // MQTT topic + metric settings for slot 1 (default topic "<device>/MYSENSOR/Value")
        d.mqtt(1, "MYSENSOR/Value", "MYSENSOR");

        // readings -> dashboard card, OLED line, MQTT message
        d.channel("value", F("Value"), "ppm", 1)   // key, label, unit, decimals
            .kind("my_value")                      // semantic id other plugins can read with ctx.latest()
            .oled(3, "VAL")                        // OLED priority (1 = most important) and short tag
            .mqtt(1);                              // publish on MQTT slot 1
    }

    // ---- 2. begin: hardware init (also called again after settings change) ----
    bool begin(autosense::Context &ctx) override
    {
        pin_ = (int)ctx.num("adc_pin");
        return true; // false = "sensor not found" in the portal
    }

    // ---- 3. read: called every reading interval ----------------------------------
    void read(autosense::Context &ctx, autosense::Readings &out) override
    {
        float raw = autosense::analogAverage(pin_, 10, 2);
        out.set("value", raw / autosense::adcMax() * ctx.num("factor"));
        // on failure:  out.fail("no answer from the sensor");
    }

    // ---- 4. (optional) calibration: describe it with d.calibration(...) and
    //         implement calibrationStart / Tick / Capture / Finish.
    //         See sdk/docs/PLUGIN_GUIDE.md and src/Plugins/P006.cpp.

private:
    int pin_ = -1;
};

AUTOSENSE_REGISTER("PXXX", PXXX)

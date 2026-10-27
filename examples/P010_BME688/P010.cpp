// =============================================================================
//  P010  —  Bosch BME688 / BME680: temperature, humidity, pressure, gas (I2C)
//  The MQTT payload is one JSON object (temperature, humidity, pressure,
//  gasResistance in ohm).  The dashboard and OLED show the gas value in kOhm.
// =============================================================================
#include <AutoSensePlugin.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME680.h>

class P010 : public autosense::Plugin
{
public:
    ~P010() { release(); }

    void describe(autosense::Descriptor &d) override
    {
        d.info("BME688", F("BME688"), F("Temperature, humidity, pressure and gas"), autosense::Bus::I2C);
        d.hint(F("0x77 by default, 0x76 when SDO is tied to GND."));

        d.address("i2c_addr", F("I2C address"), 0x77);
        d.mqtt(1, "BME688/Data", "BME688");
        d.mqttJson(1);

        d.channel("temperature", F("Temperature"), "\xC2\xB0" "C", 1).kind("temperature").oled(1, "T", "TEMP");
        d.channel("humidity", F("Humidity"), "%", 1).kind("humidity").oled(2, "H", "HUM");
        d.channel("pressure", F("Pressure"), "hPa", 1).kind("pressure");
        d.channel("gas", F("Gas resistance"), "k\xCE\xA9", 1).localOnly().oled(3, "G", "GAS");
        d.channel("gasResistance", F("Gas resistance"), "\xCE\xA9", 0).hidden();
    }

    bool begin(autosense::Context &ctx) override
    {
        release();
        bme_ = new Adafruit_BME680(&Wire);
        if (!bme_->begin((uint8_t)ctx.num("i2c_addr")))
        {
            release();
            return false;
        }
        bme_->setTemperatureOversampling(BME680_OS_8X);
        bme_->setHumidityOversampling(BME680_OS_2X);
        bme_->setPressureOversampling(BME680_OS_4X);
        bme_->setIIRFilterSize(BME680_FILTER_SIZE_3);
        bme_->setGasHeater(320, 150); // 320 C for 150 ms
        return true;
    }

    void read(autosense::Context &, autosense::Readings &out) override
    {
        if (!bme_)
        {
            out.fail("sensor not found");
            return;
        }
        if (!bme_->performReading())
        {
            out.fail("reading failed");
            return;
        }
        out.set("temperature", bme_->temperature);
        out.set("humidity", bme_->humidity);
        out.set("pressure", bme_->pressure / 100.0f);
        out.set("gas", bme_->gas_resistance / 1000.0f);
        out.set("gasResistance", (float)bme_->gas_resistance);
    }

private:
    Adafruit_BME680 *bme_ = nullptr;
    void release()
    {
        delete bme_;
        bme_ = nullptr;
    }
};

AUTOSENSE_REGISTER("P010", P010)

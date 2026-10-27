// =============================================================================
//  P002  —  DHT11 / DHT22 temperature + humidity
// =============================================================================
#include <AutoSensePlugin.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

class P002 : public autosense::Plugin
{
public:
    ~P002() { release(); }

    void describe(autosense::Descriptor &d) override
    {
        d.info("DHT", F("DHT"), F("Temperature and humidity (DHT11 / DHT22)"), autosense::Bus::Digital);

        d.pin("gpio_pin", F("Data pin (GPIO)"), 14);
        d.choice("dht_type", F("Sensor type"), 22).option(11, F("DHT11")).option(22, F("DHT22"));
        d.mqtt(1, "DHT/Temperature", "Temperature", F("temperature"));
        d.mqtt(2, "DHT/Humidity", "Humidity", F("humidity"));

        d.channel("temperature", F("Temperature"), "\xC2\xB0" "C", 1)
            .kind("temperature").oled(1, "T", "TEMP").mqtt(1).mqttLocation("Temperature").mqttUnit("C");
        d.channel("humidity", F("Humidity"), "%", 1)
            .kind("humidity").oled(2, "H", "HUM").mqtt(2).mqttLocation("Humidity");
    }

    bool begin(autosense::Context &ctx) override
    {
        release();
        int type = (int)ctx.num("dht_type");
        if (type != 11 && type != 22)
            type = 22;
        dht_ = new DHT_Unified((uint8_t)ctx.num("gpio_pin"), (uint8_t)type);
        dht_->begin();
        return true;
    }

    void read(autosense::Context &, autosense::Readings &out) override
    {
        if (!dht_)
        {
            out.fail("not initialised");
            return;
        }
        sensors_event_t ev;
        dht_->temperature().getEvent(&ev);
        float t = ev.temperature;
        dht_->humidity().getEvent(&ev);
        float h = ev.relative_humidity;
        if (isnan(t) && isnan(h))
        {
            out.fail("no answer from the DHT");
            return;
        }
        out.set("temperature", t);
        out.set("humidity", h);
    }

private:
    DHT_Unified *dht_ = nullptr;
    void release()
    {
        delete dht_;
        dht_ = nullptr;
    }
};

AUTOSENSE_REGISTER("P002", P002)

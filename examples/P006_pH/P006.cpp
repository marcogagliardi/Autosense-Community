// =============================================================================
//  P006  —  pH probe (analog) with two-point buffer calibration
//
//  pH = slope * volt + intercept, slope from the two reference points
//  (MIN_REF/MIN and MAX_REF/MAX), temperature-compensated with the
//  "liquid_temperature" of any module that offers one (e.g. the DS18B20).
// =============================================================================
#include <AutoSensePlugin.h>

class P006 : public autosense::Plugin
{
public:
    void describe(autosense::Descriptor &d) override
    {
        d.info("PH", F("pH"), F("pH probe with two-point calibration"), autosense::Bus::Analog);
        d.hint(F("Use the calibration wizard with two buffer solutions to set MIN and MAX."));

        d.pin("adc_pin", F("Analog pin (GPIO)"), A0)
            .adc1().legacy("A_index", autosense::legacyAnalogGpio);
        d.number("VREF", F("ADC reference voltage"), 3.3f).range(1.0f, 5.5f).step(0.1f).unit("V");
        d.number("MIN", F("Voltage at low buffer"), 1.92f).range(0, 5.5f).step(0.01f).unit("V").group("Calibration");
        d.number("MAX", F("Voltage at high buffer"), 2.53f).range(0, 5.5f).step(0.01f).unit("V").group("Calibration");
        d.number("MIN_REF", F("Low buffer pH"), 4.01f).range(0, 14).step(0.01f).group("Calibration");
        d.number("MAX_REF", F("High buffer pH"), 6.86f).range(0, 14).step(0.01f).group("Calibration");
        d.mqtt(1, "PH/Value", "PH");

        d.channel("ph", F("pH"), "", 2).kind("ph").oled(3, "pH").mqtt(1).mqttUnit("ppm");

        autosense::Calibration &c = d.calibration(F("Calibrate pH"));
        c.intro(F("You need two buffer solutions (by default pH 4.01 and pH 6.86). Rinse the probe "
                  "between buffers and wait for the voltage to settle before capturing."))
            .live("voltage")
            .stable(5, 0.005f)
            .mqtt("calibrate_ph", F("Calibrate pH"), "calibration/ph/reading",
                  F("Two-point pH buffer calibration. Place probe in pH 4.01 buffer, capture min; "
                    "then in pH 6.86 buffer, capture max."))
            .step("min", F("Low buffer"),
                  F("Put the probe in the LOW buffer (pH 4.01 by default) and wait until the voltage is stable."),
                  "min", F("Capture Min (pH 4)"))
            .step("max", F("High buffer"),
                  F("Rinse the probe, put it in the HIGH buffer (pH 6.86 by default) and wait until the voltage is stable."),
                  "max", F("Capture Max (pH 7)"))
            .field("running", F("Calibrating"), "boolean", "", 0, false)
            .field("voltage", F("Live Voltage"), "number", "V", 3, true)
            .field("minCaptured", F("Min Captured"), "boolean", "", 0, true)
            .field("maxCaptured", F("Max Captured"), "boolean", "", 0, true)
            .field("voltageMin", F("Voltage Min"), "number", "V", 3, true)
            .field("voltageMax", F("Voltage Max"), "number", "V", 3, true)
            .field("P006_MIN_REF", F("pH Min Reference"), "number", "pH", 2, true)
            .field("P006_MAX_REF", F("pH Max Reference"), "number", "pH", 2, true);
    }

    void read(autosense::Context &ctx, autosense::Readings &out) override
    {
        float ph = phFromVoltage(ctx, voltage(ctx, 10, 5));
        if (isnan(ph) || isinf(ph))
        {
            out.fail("invalid calibration (MIN = MAX?)");
            return;
        }
        out.set("ph", ph);
    }

    // ---- calibration -------------------------------------------------------
    bool calibrationStart(autosense::Context &ctx, autosense::Status &st) override
    {
        st.set("voltage", NAN);
        st.set("minCaptured", false);
        st.set("maxCaptured", false);
        st.set("voltageMin", 0.0f);
        st.set("voltageMax", 0.0f);
        st.set("P006_MIN_REF", ctx.num("MIN_REF"));
        st.set("P006_MAX_REF", ctx.num("MAX_REF"));
        return true;
    }

    void calibrationTick(autosense::Context &ctx, autosense::Status &st) override
    {
        last_ = voltage(ctx, 8, 2);
        st.set("voltage", last_);
    }

    bool calibrationCapture(autosense::Context &ctx, const char *stepId, autosense::Status &st) override
    {
        last_ = voltage(ctx, 10, 2);
        st.set("voltage", last_);
        if (strcmp(stepId, "min") == 0)
        {
            st.set("voltageMin", last_);
            st.set("minCaptured", true);
        }
        else
        {
            st.set("voltageMax", last_);
            st.set("maxCaptured", true);
        }
        return true;
    }

    bool calibrationFinish(autosense::Context &ctx, bool save, autosense::Status &st) override
    {
        if (!save)
            return true;
        if (!st.flag("minCaptured") || !st.flag("maxCaptured"))
        {
            st.message("Capture both buffers first");
            return false;
        }
        float lo = st.num("voltageMin"), hi = st.num("voltageMax");
        if (fabsf(hi - lo) < 0.01f)
        {
            st.message("The two voltages are too close");
            return false;
        }
        ctx.setNum("MIN", lo);
        ctx.setNum("MAX", hi);
        ctx.save();
        return true;
    }

private:
    float last_ = 0;

    static float voltage(autosense::Context &ctx, uint8_t samples, uint8_t spacing)
    {
        float vref = ctx.num("VREF");
        if (vref <= 0)
            vref = 3.3f;
        float raw = autosense::analogAverage((int)ctx.num("adc_pin"), samples, spacing);
        if (raw <= 0)
            raw = 1;
        float v = raw / autosense::adcMax() * vref;
        return v < 0.01f ? 0.01f : v;
    }

    static float phFromVoltage(autosense::Context &ctx, float v)
    {
        float lo = ctx.num("MIN"), hi = ctx.num("MAX");
        float loRef = ctx.num("MIN_REF"), hiRef = ctx.num("MAX_REF");
        if (hi == lo)
            return NAN;
        float slope = (hiRef - loRef) / (hi - lo);
        float t = ctx.latest("liquid_temperature");
        if (!isnan(t) && t > 0)
            slope = slope * (t + 273.15f) / 298.15f;
        return slope * v + (hiRef - slope * hi);
    }
};

AUTOSENSE_REGISTER("P006", P006)

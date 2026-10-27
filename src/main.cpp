// =============================================================================
//  AutoSense Community - on-device plugin test harness  (SDK v1, AutoSense 2.x)
// =============================================================================
//  Runs your plugin on a real ESP32 / ESP8266 exactly the way the AutoSense
//  firmware does - without the private core:
//
//    * registers every plugin in src/ (AUTOSENSE_REGISTER)
//    * prints the schema the AutoSense portal will generate from describe()
//    * calls begin() and then read() every few seconds
//    * shows the values, the MQTT message the firmware would publish and the
//      lines the OLED would show
//    * lets you change settings and run the calibration from the Serial Monitor
//
//  You never need to edit this file.  Put your plugin in src/ (see README.md),
//  open the Serial Monitor at 115200 baud and type `help`.
// =============================================================================
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <AutoSensePlugin.h>
#include <algorithm>
#include <string>
#include <vector>

#if defined(ESP32)
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif

using namespace autosense;

// ---- harness configuration (override with build_flags in platformio.ini) ----
#ifndef HARNESS_INTERVAL_MS
#define HARNESS_INTERVAL_MS 5000
#endif
#ifndef HARNESS_I2C_SDA
#if defined(ESP32)
#define HARNESS_I2C_SDA 21
#define HARNESS_I2C_SCL 22
#else
#define HARNESS_I2C_SDA 4
#define HARNESS_I2C_SCL 5
#endif
#endif
#ifndef HARNESS_DEVICE_NAME
#define HARNESS_DEVICE_NAME "AutoSense"
#endif

// =============================================================================
//  Registry - the core's side of AUTOSENSE_REGISTER
// =============================================================================
struct Registration
{
    const char *code;
    PluginFactory factory;
};

static std::vector<Registration> &registrations()
{
    static std::vector<Registration> r; // function-local: plugins register before globals exist
    return r;
}

void autosense::registerPlugin(const char *code, PluginFactory factory)
{
    registrations().push_back({code, factory});
}

// =============================================================================
//  Plugin slots
// =============================================================================
struct SettingValue
{
    const SettingSpec *spec;
    float num;
    std::string text;
};

class SlotContext;

struct Slot
{
    std::string code;
    Plugin *plugin = nullptr;
    Descriptor *desc = nullptr;
    SlotContext *ctx = nullptr;
    std::vector<SettingValue> settings;
    std::vector<float> values; // one per channel
    bool beginOk = false;
    bool failed = false;
    std::string error;
    uint32_t reads = 0;
};

static std::vector<Slot *> g_slots;
static uint32_t g_interval = HARNESS_INTERVAL_MS;
static bool g_paused = false;
static bool g_verbose = true; // print MQTT + OLED previews on every read

static const char *str(const __FlashStringHelper *f) { return f ? (const char *)f : ""; }

static SettingValue *findSetting(Slot &s, const char *key)
{
    for (auto &v : s.settings)
        if (strcmp(v.spec->key, key) == 0)
            return &v;
    return nullptr;
}

static int channelIndex(Slot &s, const char *key)
{
    for (int i = 0; i < s.desc->channelCount; i++)
        if (strcmp(s.desc->channels[i].s.key, key) == 0)
            return i;
    return -1;
}

static Slot *findSlot(const String &code)
{
    for (auto *s : g_slots)
        if (code.equalsIgnoreCase(s->code.c_str()))
            return s;
    return nullptr;
}

/// Context handed to the plugin: settings in RAM, values of the other plugins.
class SlotContext : public Context
{
public:
    explicit SlotContext(Slot *s) : s_(s) {}

    float num(const char *key) override
    {
        SettingValue *v = findSetting(*s_, key);
        if (!v || v->spec->type == SettingType::Text)
        {
            Serial.printf("  !! [%s] ctx.num(\"%s\"): no such numeric setting\n", s_->code.c_str(), key);
            return 0;
        }
        return v->num;
    }
    const char *text(const char *key) override
    {
        SettingValue *v = findSetting(*s_, key);
        if (!v || v->spec->type != SettingType::Text)
        {
            Serial.printf("  !! [%s] ctx.text(\"%s\"): no such text setting\n", s_->code.c_str(), key);
            return "";
        }
        return v->text.c_str();
    }
    bool setNum(const char *key, float value) override
    {
        SettingValue *v = findSetting(*s_, key);
        if (!v || v->spec->type == SettingType::Text)
        {
            Serial.printf("  !! [%s] ctx.setNum(\"%s\"): no such numeric setting\n", s_->code.c_str(), key);
            return false;
        }
        v->num = value;
        Serial.printf("  [%s] setting %s = %g\n", s_->code.c_str(), key, value);
        return true;
    }
    void save() override
    {
        Serial.printf("  [%s] ctx.save(): the firmware would write the settings to EEPROM now "
                      "(the harness keeps them in RAM until reset)\n",
                      s_->code.c_str());
    }
    float latest(const char *kind) override
    {
        for (auto *o : g_slots)
            for (int i = 0; i < o->desc->channelCount; i++)
                if (strcmp(o->desc->channels[i].s.kind, kind) == 0 && !isnan(o->values[i]))
                    return o->values[i];
        return NAN;
    }

private:
    Slot *s_;
};

/// Collects what read() reports.
class SlotReadings : public Readings
{
public:
    explicit SlotReadings(Slot &s) : s_(s) {}
    void set(const char *key, float value) override
    {
        int i = channelIndex(s_, key);
        if (i < 0)
        {
            Serial.printf("  !! [%s] out.set(\"%s\"): channel not declared in describe()\n", s_.code.c_str(), key);
            return;
        }
        s_.values[i] = value;
    }
    void fail(const char *reason) override
    {
        s_.failed = true;
        s_.error = reason ? reason : "failed";
    }

private:
    Slot &s_;
};

// =============================================================================
//  Schema report (what the portal, MQTT and OLED will be built from)
// =============================================================================
static const char *typeName(SettingType t)
{
    switch (t)
    {
    case SettingType::Number: return "number";
    case SettingType::Pin: return "pin";
    case SettingType::Address: return "address";
    case SettingType::Text: return "text";
    case SettingType::Choice: return "choice";
    case SettingType::Toggle: return "toggle";
    }
    return "?";
}

static const char *busName(Bus b)
{
    switch (b)
    {
    case Bus::Analog: return "analog";
    case Bus::Digital: return "digital";
    case Bus::I2C: return "I2C";
    case Bus::OneWire: return "1-Wire";
    case Bus::Display: return "display";
    default: return "-";
    }
}

static void printValue(const SettingValue &v)
{
    if (v.spec->type == SettingType::Text)
        Serial.printf("\"%s\"", v.text.c_str());
    else if (v.spec->type == SettingType::Address)
        Serial.printf("0x%02X", (int)v.num);
    else
        Serial.printf("%g", v.num);
}

static int checkSchema(Slot &s)
{
    Descriptor &d = *s.desc;
    int problems = 0;
    auto bad = [&](const char *fmt, const char *a, const char *b = "") {
        Serial.print("  !! ");
        Serial.printf(fmt, a, b);
        Serial.println();
        problems++;
    };
    if (!*d.wireName) bad("d.info(...) is missing%s%s", "", "");
    if (d.settingCount >= Descriptor::MAX_SETTINGS) bad("too many settings%s%s", "", "");
    if (d.channelCount >= Descriptor::MAX_CHANNELS) bad("too many channels%s%s", "", "");
    for (int i = 0; i < d.settingCount; i++)
    {
        const SettingSpec &a = d.settings[i].s;
        if (strlen(a.key) > 15) bad("setting key \"%s\" is longer than 15 characters%s", a.key);
        for (int j = 0; j < i; j++)
            if (strcmp(a.key, d.settings[j].s.key) == 0) bad("setting \"%s\" declared twice%s", a.key);
        if (a.type == SettingType::Choice && a.optionCount == 0) bad("choice \"%s\" has no .option()%s", a.key);
    }
    for (int i = 0; i < d.channelCount; i++)
    {
        const ChannelSpec &c = d.channels[i].s;
        for (int j = 0; j < i; j++)
            if (strcmp(c.key, d.channels[j].s.key) == 0) bad("channel \"%s\" declared twice%s", c.key);
        if (c.mqttSlot && !findSetting(s, c.mqttSlot == 1 ? "MQTT_SLOT1" : "MQTT_SLOT2"))
            bad("channel \"%s\" publishes on an MQTT slot without d.mqtt(...)%s", c.key);
    }
    if (d.hasCal)
    {
        bool live = false;
        for (int i = 0; i < d.cal.fieldCount; i++)
            if (strcmp(d.cal.fields[i].key, d.cal.liveKey) == 0) live = true;
        if (!live) bad("calibration .live(\"%s\") is not one of its .field()s%s", d.cal.liveKey);
        if (d.cal.stepCount == 0) bad("calibration has no .step()%s%s", "", "");
    }
    return problems;
}

static void printSchema(Slot &s)
{
    Descriptor &d = *s.desc;
    Serial.printf("\n[%s] %s - %s   (wire name %s, bus %s)\n", s.code.c_str(), str(d.titleText), str(d.summaryText),
                  d.wireName, busName(d.busType));
    if (d.hintText) Serial.printf("  hint: %s\n", str(d.hintText));
    Serial.println("  settings (portal form):");
    for (auto &v : s.settings)
    {
        Serial.printf("    %-14s %-8s %-30s = ", v.spec->key, typeName(v.spec->type), str(v.spec->label));
        printValue(v);
        if (v.spec->unit && *v.spec->unit) Serial.printf(" %s", v.spec->unit);
        if (v.spec->live) Serial.print("   (applied without restarting the module)");
        Serial.println();
    }
    Serial.println("  channels (dashboard cards):");
    for (int i = 0; i < d.channelCount; i++)
    {
        const ChannelSpec &c = d.channels[i].s;
        Serial.printf("    %-14s %-24s %-6s", c.key, str(c.label), c.unit);
        if (*c.kind) Serial.printf(" kind=%s", c.kind);
        if (c.oledPriority) Serial.printf(" OLED(prio %u, \"%s\")", c.oledPriority, c.oledTag);
        if (c.mqttSlot) Serial.printf(" MQTT slot %u", c.mqttSlot);
        if (c.hidden) Serial.print(" hidden");
        if (c.localOnly) Serial.print(" local-only");
        Serial.println();
    }
    if (d.jsonSlot) Serial.printf("  MQTT: all channels as one JSON object on slot %u\n", d.jsonSlot);
    if (d.hasCal)
    {
        Serial.printf("  calibration: %s - %u step(s):", str(d.cal.title), d.cal.stepCount);
        for (int i = 0; i < d.cal.stepCount; i++) Serial.printf(" %s", d.cal.steps[i].id);
        Serial.printf("   (type: cal %s)\n", s.code.c_str());
    }
    int p = checkSchema(s);
    Serial.println(p ? "  schema has problems (see !! above)" : "  schema OK");
}

// =============================================================================
//  Outputs - previews of what the firmware does with the values
// =============================================================================
static std::string fmt(float v, uint8_t dec)
{
    if (isnan(v)) return "-";
    char b[24];
    snprintf(b, sizeof(b), "%.*f", dec, v);
    return b;
}

static const char *settingText(Slot &s, const char *base, uint8_t slot)
{
    char key[16];
    snprintf(key, sizeof(key), "%s%u", base, slot);
    SettingValue *v = findSetting(s, key);
    return v ? v->text.c_str() : "";
}

/// Same envelope the firmware publishes (src/core/Outputs.cpp in AutoSense).
static void mqttPreview(Slot &s)
{
    Descriptor &d = *s.desc;
    auto envelope = [&](const char *location, const char *unit, const char *metric) {
        JsonDocument doc;
        doc["sensorType"] = d.wireName;
        doc["location"] = location;
        doc["manufacturer"] = "Autosense";
        doc["unit"] = unit;
        doc["metric"] = *metric ? metric : d.wireName;
        return doc;
    };
    if (d.jsonSlot)
    {
        JsonDocument doc = envelope("Lab 1", "", settingText(s, "METRIC", d.jsonSlot));
        JsonObject raw = doc["rawJson"].to<JsonObject>();
        for (int i = 0; i < d.channelCount; i++)
        {
            const ChannelSpec &c = d.channels[i].s;
            if (c.localOnly) continue;
            if (isnan(s.values[i])) raw[c.key] = nullptr;
            else raw[c.key] = serialized(String(fmt(s.values[i], c.decimals).c_str()));
        }
        doc["mac"] = WiFi.macAddress();
        Serial.printf("  MQTT  %s  ", settingText(s, "MQTT_SLOT", d.jsonSlot));
        serializeJson(doc, Serial);
        Serial.println();
        return;
    }
    for (int i = 0; i < d.channelCount; i++)
    {
        const ChannelSpec &c = d.channels[i].s;
        if (!c.mqttSlot || c.localOnly || isnan(s.values[i])) continue;
        JsonDocument doc = envelope(c.mqttLocation ? c.mqttLocation : "Lab 1",
                                    c.mqttUnit ? c.mqttUnit : c.unit, settingText(s, "METRIC", c.mqttSlot));
        doc["value"] = serialized(String(fmt(s.values[i], c.decimals < 2 ? 2 : c.decimals).c_str()));
        doc["rawJson"] = nullptr;
        doc["mac"] = WiFi.macAddress();
        Serial.printf("  MQTT  %s  ", settingText(s, "MQTT_SLOT", c.mqttSlot));
        serializeJson(doc, Serial);
        Serial.println();
    }
}

/// The OLED shows the 3 most important values of ALL modules (priority 1 first).
static void oledPreview()
{
    struct Line
    {
        uint8_t prio;
        std::string text;
    };
    std::vector<Line> lines;
    for (auto *s : g_slots)
        for (int i = 0; i < s->desc->channelCount; i++)
        {
            const ChannelSpec &c = s->desc->channels[i].s;
            if (!c.oledPriority) continue;
            lines.push_back({c.oledPriority, std::string(c.oledTag) + " " + fmt(s->values[i], c.decimals) + " " + c.unit});
        }
    if (lines.empty()) return;
    std::stable_sort(lines.begin(), lines.end(), [](const Line &a, const Line &b) { return a.prio < b.prio; });
    Serial.print("  OLED  ");
    for (size_t i = 0; i < lines.size() && i < 3; i++) Serial.printf("%s[%s]", i ? "  " : "", lines[i].text.c_str());
    Serial.println("   (128x64: first 3 lines, 128x32: first 2)");
}

// =============================================================================
//  Life cycle
// =============================================================================
static void beginSlot(Slot &s)
{
    s.failed = false;
    s.error.clear();
    s.beginOk = s.plugin->begin(*s.ctx);
    Serial.printf("[%s] begin() -> %s\n", s.code.c_str(), s.beginOk ? "true" : "false (the portal shows \"Sensor not found\")");
}

static void readSlot(Slot &s)
{
    for (auto &v : s.values) v = NAN;
    s.failed = false;
    s.error.clear();
    SlotReadings out(s);
    uint32_t t0 = millis();
    s.plugin->read(*s.ctx, out);
    uint32_t took = millis() - t0;
    s.reads++;

    Serial.printf("[%s] read #%lu (%lu ms)", s.code.c_str(), (unsigned long)s.reads, (unsigned long)took);
    if (s.failed) Serial.printf("  FAILED: %s", s.error.c_str());
    Serial.println();
    for (int i = 0; i < s.desc->channelCount; i++)
    {
        const ChannelSpec &c = s.desc->channels[i].s;
        Serial.printf("  %-14s %10s %s\n", c.key, fmt(s.values[i], c.decimals).c_str(), c.unit);
    }
    if (took > 200) Serial.println("  !! read() took more than 200 ms - keep it short, it blocks the whole device");
    if (g_verbose && !s.failed) mqttPreview(s);
}

static void readAll()
{
    Serial.printf("\n---- %lu s ------------------------------------------------\n", (unsigned long)(millis() / 1000));
    for (auto *s : g_slots) readSlot(*s);
    if (g_verbose) oledPreview();
}

// =============================================================================
//  Calibration session (portal wizard / MQTT hub in the real firmware)
// =============================================================================
struct CalSession
{
    Slot *slot = nullptr;
    Status st;
    std::vector<float> live;
    uint32_t lastTick = 0;
    uint8_t step = 0;
} g_cal;

static bool calStable()
{
    const Calibration &c = g_cal.slot->desc->cal;
    if (g_cal.live.size() < c.stableWindow) return false;
    float lo = *std::min_element(g_cal.live.end() - c.stableWindow, g_cal.live.end());
    float hi = *std::max_element(g_cal.live.end() - c.stableWindow, g_cal.live.end());
    return hi - lo <= c.stableTol;
}

static void calPrintStatus()
{
    Status &st = g_cal.st;
    Serial.print("  status:");
    for (uint8_t i = 0; i < st.count(); i++)
    {
        if (st.isBoolAt(i)) Serial.printf(" %s=%s", st.keyAt(i), st.valueAt(i) ? "yes" : "no");
        else Serial.printf(" %s=%s", st.keyAt(i), fmt(st.valueAt(i), 3).c_str());
    }
    if (*st.messageText()) Serial.printf("  msg=\"%s\"", st.messageText());
    Serial.println();
}

static void calNextHint()
{
    const Calibration &c = g_cal.slot->desc->cal;
    if (g_cal.step < c.stepCount)
    {
        const CalStepSpec &s = c.steps[g_cal.step];
        Serial.printf("\n  Step %u/%u: %s\n  %s\n  -> type `capture` when the value is stable (`cancel` to stop)\n",
                      g_cal.step + 1, c.stepCount, str(s.title), str(s.text));
    }
    else
        Serial.println("\n  All steps captured -> type `save` to apply the result or `cancel` to discard it");
}

static void calStart(Slot &s)
{
    if (!s.desc->hasCal)
    {
        Serial.printf("[%s] has no calibration\n", s.code.c_str());
        return;
    }
    if (g_cal.slot) Serial.println("(a calibration was already running - it is replaced)");
    g_cal = CalSession();
    g_cal.slot = &s;
    if (!s.plugin->calibrationStart(*s.ctx, g_cal.st))
    {
        Serial.printf("[%s] calibrationStart() refused: %s\n", s.code.c_str(), g_cal.st.messageText());
        g_cal.slot = nullptr;
        return;
    }
    const Calibration &c = s.desc->cal;
    Serial.printf("\n[%s] %s started\n", s.code.c_str(), str(c.title));
    if (c.introText) Serial.printf("  %s\n", str(c.introText));
    Serial.printf("  live value: %s  (stable after %u samples within %g)\n", c.liveKey, c.stableWindow, c.stableTol);
    g_cal.lastTick = millis();
    calNextHint();
}

static void calTick()
{
    if (!g_cal.slot || millis() - g_cal.lastTick < 1000) return;
    g_cal.lastTick = millis();
    Slot &s = *g_cal.slot;
    s.plugin->calibrationTick(*s.ctx, g_cal.st);
    float v = g_cal.st.num(s.desc->cal.liveKey);
    if (!isnan(v)) g_cal.live.push_back(v);
    if (g_cal.live.size() > 50) g_cal.live.erase(g_cal.live.begin());
    Serial.printf("  [cal %s] %s = %s  %s\n", s.code.c_str(), s.desc->cal.liveKey, fmt(v, 4).c_str(),
                  calStable() ? "STABLE" : "settling...");
}

static void calCapture(const String &which)
{
    if (!g_cal.slot) return (void)Serial.println("no calibration running (type `cal <code>`)");
    Slot &s = *g_cal.slot;
    const Calibration &c = s.desc->cal;
    const CalStepSpec *step = nullptr;
    if (which.length())
    {
        for (int i = 0; i < c.stepCount; i++)
            if (which == c.steps[i].id || (c.steps[i].point && which == c.steps[i].point)) step = &c.steps[i];
    }
    else if (g_cal.step < c.stepCount)
        step = &c.steps[g_cal.step];
    if (!step) return (void)Serial.println("unknown step");
    if (!calStable()) Serial.println("  (note: the portal only enables Capture once the value is stable)");
    g_cal.st.message("");
    bool ok = s.plugin->calibrationCapture(*s.ctx, step->id, g_cal.st);
    Serial.printf("[%s] calibrationCapture(\"%s\") -> %s\n", s.code.c_str(), step->id, ok ? "ok" : "failed");
    calPrintStatus();
    if (ok)
    {
        g_cal.live.clear();
        g_cal.step++;
    }
    calNextHint();
}

static void calStop(bool save)
{
    if (!g_cal.slot) return (void)Serial.println("no calibration running");
    Slot &s = *g_cal.slot;
    g_cal.st.message("");
    bool ok = s.plugin->calibrationFinish(*s.ctx, save, g_cal.st);
    if (save && !ok)
    {
        Serial.printf("[%s] calibrationFinish(save) refused: %s  (fix and `save` again, or `cancel`)\n", s.code.c_str(),
                      g_cal.st.messageText());
        return;
    }
    Serial.printf("[%s] calibration %s\n", s.code.c_str(), save ? "saved" : "cancelled");
    calPrintStatus();
    g_cal.slot = nullptr;
}

// =============================================================================
//  Serial console
// =============================================================================
static void help()
{
    Serial.println(
        "\nCommands:\n"
        "  help                      this list\n"
        "  list                      schema and current settings of every plugin\n"
        "  read                      read all plugins now\n"
        "  set <code> <key> <value>  change a setting (validated like the portal, restarts the module)\n"
        "  interval <seconds>        reading interval (0 = only on `read`)\n"
        "  quiet / verbose           hide / show the MQTT and OLED previews\n"
        "  pause / resume            stop / restart the automatic reads\n"
        "  i2c                       scan the I2C bus\n"
        "  cal <code>                start the calibration of a plugin\n"
        "  capture [step]            capture the current (or named) calibration step\n"
        "  save / cancel             finish the calibration\n");
}

static bool parseValue(const SettingSpec &sp, const String &raw, float &out, std::string &err)
{
    String t = raw;
    t.trim();
    t.replace(",", ".");
    if (sp.type == SettingType::Toggle)
    {
        t.toLowerCase();
        out = (t == "1" || t == "on" || t == "true") ? 1 : 0;
        return true;
    }
    char *end = nullptr;
    out = (sp.type == SettingType::Address && t.startsWith("0x")) ? (float)strtol(t.c_str() + 2, &end, 16)
                                                                   : strtof(t.c_str(), &end);
    if (!t.length() || (end && *end)) return err = "enter a number", false;
    if (sp.type == SettingType::Pin)
    {
        if (out != floorf(out)) return err = "enter a whole GPIO number", false;
        if (sp.adc1 && !isAdc1Pin((int)out)) return err = "ESP32 can only read analog on GPIO 32-39 while Wi-Fi is on", false;
    }
    if (sp.type == SettingType::Choice && sp.optionCount)
    {
        bool known = false;
        for (int i = 0; i < sp.optionCount; i++) known |= sp.options[i].value == out;
        if (!known) return err = "not one of the options", false;
    }
    if ((!isnan(sp.min) && out < sp.min) || (!isnan(sp.max) && out > sp.max))
    {
        char b[64];
        snprintf(b, sizeof(b), "must be between %g and %g", sp.min, sp.max);
        return err = b, false;
    }
    return true;
}

static void cmdSet(const String &code, const String &key, const String &value)
{
    Slot *s = findSlot(code);
    if (!s) return (void)Serial.printf("unknown plugin %s\n", code.c_str());
    SettingValue *v = findSetting(*s, key.c_str());
    if (!v) return (void)Serial.printf("[%s] has no setting %s (type `list`)\n", s->code.c_str(), key.c_str());
    if (v->spec->type == SettingType::Text)
    {
        if (value.length() > v->spec->maxLen) return (void)Serial.printf("too long (max %u)\n", v->spec->maxLen);
        v->text = value.c_str();
    }
    else
    {
        float f;
        std::string err;
        if (!parseValue(*v->spec, value, f, err)) return (void)Serial.printf("[%s] %s: %s\n", s->code.c_str(), key.c_str(), err.c_str());
        v->num = f;
    }
    Serial.printf("[%s] %s = ", s->code.c_str(), key.c_str());
    printValue(*v);
    Serial.println();
    if (!v->spec->live) beginSlot(*s); // the firmware restarts the module after a non-live change
}

static void i2cScan()
{
    Serial.printf("I2C scan (SDA %d, SCL %d):", HARNESS_I2C_SDA, HARNESS_I2C_SCL);
    int n = 0;
    for (uint8_t a = 1; a < 127; a++)
    {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) Serial.printf(" 0x%02X", a), n++;
    }
    Serial.println(n ? "" : " nothing answered");
}

static void handleLine(String line)
{
    line.trim();
    if (!line.length()) return;
    String w[4];
    int n = 0, start = 0;
    for (int i = 0; i <= (int)line.length() && n < 4; i++)
    {
        if (i == (int)line.length() || (line[i] == ' ' && n < 3))
        {
            if (i > start) w[n++] = line.substring(start, i);
            start = i + 1;
        }
    }
    String cmd = w[0];
    cmd.toLowerCase();
    if (cmd == "help") help();
    else if (cmd == "list") for (auto *s : g_slots) printSchema(*s);
    else if (cmd == "read") readAll();
    else if (cmd == "set" && n == 4) cmdSet(w[1], w[2], w[3]);
    else if (cmd == "interval" && n == 2) g_interval = (uint32_t)(w[1].toFloat() * 1000), Serial.printf("interval %lu ms\n", (unsigned long)g_interval);
    else if (cmd == "quiet") g_verbose = false;
    else if (cmd == "verbose") g_verbose = true;
    else if (cmd == "pause") g_paused = true, Serial.println("paused");
    else if (cmd == "resume") g_paused = false, Serial.println("resumed");
    else if (cmd == "i2c") i2cScan();
    else if (cmd == "cal" && n == 2)
    {
        Slot *s = findSlot(w[1]);
        if (s) calStart(*s);
        else Serial.println("unknown plugin");
    }
    else if (cmd == "capture") calCapture(n > 1 ? w[1] : String());
    else if (cmd == "save") calStop(true);
    else if (cmd == "cancel") calStop(false);
    else Serial.printf("unknown command \"%s\" - type help\n", line.c_str());
}

static void pollSerial()
{
    static String buf;
    while (Serial.available())
    {
        char c = (char)Serial.read();
        if (c == '\r') continue;
        if (c == '\n')
        {
            Serial.printf("> %s\n", buf.c_str());
            handleLine(buf);
            buf = "";
        }
        else if (buf.length() < 120)
            buf += c;
    }
}

// =============================================================================
//  Arduino entry points
// =============================================================================
static uint32_t g_lastRead = 0;

void setup()
{
    Serial.begin(115200);
    delay(1500);
    Serial.println("\n==========================================================");
    Serial.println("  AutoSense Community - plugin test harness");
    Serial.println("==========================================================");
#if defined(ESP32)
    Serial.print("Board: ESP32");
#else
    Serial.print("Board: ESP8266");
#endif
    Serial.printf("   SDK v%d   I2C SDA %d / SCL %d   interval %lu ms\n", AUTOSENSE_SDK_VERSION, HARNESS_I2C_SDA,
                  HARNESS_I2C_SCL, (unsigned long)g_interval);

    // same order as the firmware: I2C first, then the plugins
    Wire.begin(HARNESS_I2C_SDA, HARNESS_I2C_SCL);

    std::vector<Registration> regs = registrations();
    std::sort(regs.begin(), regs.end(), [](const Registration &a, const Registration &b) { return strcmp(a.code, b.code) < 0; });
    if (regs.empty())
    {
        Serial.println("\nNo plugin found in src/.  Copy one in, e.g.:\n"
                       "   python sdk/tools/new_plugin.py auto \"My sensor\"\n"
                       "   or copy examples/P002_DHT/P002.cpp into src/\n"
                       "then Upload and Monitor again.");
        return;
    }

    for (auto &r : regs)
    {
        Slot *s = new Slot();
        s->code = r.code;
        s->plugin = r.factory();
        s->desc = new Descriptor();
        s->plugin->describe(*s->desc);
        for (int i = 0; i < s->desc->settingCount; i++)
        {
            const SettingSpec &sp = s->desc->settings[i].s;
            SettingValue v{&sp, sp.def, ""};
            if (sp.type == SettingType::Text)
                v.text = sp.deviceTopic ? std::string(HARNESS_DEVICE_NAME) + "/" + sp.defText : std::string(sp.defText);
            s->settings.push_back(v);
        }
        s->values.assign(s->desc->channelCount, NAN);
        s->ctx = new SlotContext(s);
        g_slots.push_back(s);
        printSchema(*s);
    }
    Serial.println();
    for (auto *s : g_slots) beginSlot(*s);
    help();
}

void loop()
{
    pollSerial();
    calTick();
    if (!g_slots.empty() && !g_paused && !g_cal.slot && g_interval && millis() - g_lastRead >= g_interval)
    {
        g_lastRead = millis();
        readAll();
    }
    delay(5);
}

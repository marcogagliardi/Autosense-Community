// =============================================================================
//  AutoSense Plugin SDK  —  version 1
// =============================================================================
//  This is the ONLY AutoSense header a plugin is allowed to include.
//
//  A plugin never builds HTML, never touches EEPROM, never talks to the web
//  server or to MQTT.  It DESCRIBES itself (settings, readings, calibration)
//  and the AutoSense core does the rest:
//
//      describe()  ->  portal settings form, dashboard card, OLED lines,
//                      MQTT topics, hub capability manifest, calibration wizard
//      begin()     ->  one-time hardware init (enabled plugins only)
//      read()      ->  periodic sampling; push values with out.set("key", v)
//
//  RULES
//   * String arguments (keys, units, tags, kinds) must be string LITERALS.
//     Labels / help texts should be wrapped in F("...") so they live in flash
//     (this matters on ESP8266).
//   * Do not call delay() for more than a few ms in read(), begin() or the
//     calibration hooks.  Keep every call short and non-blocking.
//   * Chain the setter calls right after the declaration:
//         d.number("vref", F("Reference voltage"), 3.3f).unit("V").range(1, 5.5f);
//     Do not keep the returned reference around.
//   * Setting keys are limited to 15 characters.
//
//  See sdk/docs/PLUGIN_GUIDE.md for a step-by-step walkthrough.
// =============================================================================
#pragma once

#include <Arduino.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define AUTOSENSE_SDK_VERSION 1

namespace autosense
{

// -----------------------------------------------------------------------------
//  Small shared types
// -----------------------------------------------------------------------------

/// How the sensor is wired (shown as a badge in the portal).
enum class Bus : uint8_t
{
    None = 0,
    Analog,
    Digital,
    I2C,
    OneWire,
    Display
};

/// What kind of input a setting renders as in the portal.
enum class SettingType : uint8_t
{
    Number = 0, ///< decimal number          (stored as float)
    Pin,        ///< GPIO pin number         (stored as float, validated)
    Address,    ///< I2C address, shown hex  (stored as float)
    Text,       ///< free text               (stored as string)
    Choice,     ///< one of a few numbers    (stored as float)
    Toggle      ///< on / off                (stored as 0 / 1)
};

struct Option
{
    float value;
    const __FlashStringHelper *label;
};

// -----------------------------------------------------------------------------
//  Setting  —  one editable value of the plugin
// -----------------------------------------------------------------------------
struct SettingSpec
{
    const char *key = nullptr; ///< short key, e.g. "vref"  (stored as "<code>_<key>")
    const __FlashStringHelper *label = nullptr;
    const __FlashStringHelper *help = nullptr;
    const char *unit = "";
    const char *group = "";          ///< optional heading in the form
    const char *defText = "";        ///< text default
    const __FlashStringHelper *suffix = nullptr; ///< appended to the label, e.g. "temperature"
    const char *legacyKey = nullptr; ///< old key to import from firmware < 2.0
    float (*legacyConvert)(float) = nullptr;
    float def = 0;
    float min = NAN;
    float max = NAN;
    float step = NAN;
    Option options[4];
    uint8_t optionCount = 0;
    uint8_t maxLen = 48;
    SettingType type = SettingType::Number;
    bool live = false; ///< applied immediately; otherwise a reboot is required
    bool deviceTopic = false; ///< default text is "<device name>/<defText>"
    bool adc1 = false; ///< ESP32: pin must be an ADC1 pin (GPIO 32-39)
};

class Setting
{
public:
    SettingSpec s;

    Setting &range(float lo, float hi)
    {
        s.min = lo;
        s.max = hi;
        return *this;
    }
    Setting &step(float st)
    {
        s.step = st;
        return *this;
    }
    Setting &unit(const char *u)
    {
        s.unit = u;
        return *this;
    }
    Setting &help(const __FlashStringHelper *h)
    {
        s.help = h;
        return *this;
    }
    Setting &group(const char *g)
    {
        s.group = g;
        return *this;
    }
    /// Value takes effect immediately (no reboot needed).
    Setting &live()
    {
        s.live = true;
        return *this;
    }
    /// Pin must be usable for analogRead() while Wi-Fi is on (ESP32: GPIO 32-39).
    Setting &adc1()
    {
        s.adc1 = true;
        return *this;
    }
    Setting &maxLength(uint8_t n)
    {
        s.maxLen = n;
        return *this;
    }
    Setting &option(float value, const __FlashStringHelper *label)
    {
        if (s.optionCount < 4)
        {
            s.options[s.optionCount].value = value;
            s.options[s.optionCount].label = label;
            s.optionCount++;
        }
        return *this;
    }
    /// Import the value stored by an older firmware under <code>_<oldKey>.
    Setting &legacy(const char *oldKey, float (*convert)(float) = nullptr)
    {
        s.legacyKey = oldKey;
        s.legacyConvert = convert;
        return *this;
    }
};

// -----------------------------------------------------------------------------
//  Channel  —  one value the plugin measures
// -----------------------------------------------------------------------------
struct ChannelSpec
{
    const char *key = nullptr; ///< e.g. "temperature"
    const __FlashStringHelper *label = nullptr;
    const char *unit = "";
    const char *kind = "";         ///< semantic id other plugins can look up (ctx.latest)
    const char *oledTag = "";      ///< short tag drawn on the OLED, e.g. "T"
    const char *oledTag32 = nullptr; ///< label on 32 px displays (default: oledTag)
    const char *mqttLocation = nullptr;
    const char *mqttUnit = nullptr; ///< unit sent to MQTT when it differs from `unit`
    uint8_t decimals = 1;
    uint8_t oledPriority = 0; ///< 0 = not on the OLED; 1 = most important
    uint8_t mqttSlot = 0; ///< 0 = none; n = publish alone on setting MQTT_SLOT<n>
    bool hidden = false;  ///< not shown on the dashboard (still in MQTT JSON)
    bool localOnly = false; ///< never sent to MQTT
};

class Channel
{
public:
    ChannelSpec s;

    /// Semantic id, e.g. "temperature", "humidity", "liquid_temperature", "ph".
    Channel &kind(const char *k)
    {
        s.kind = k;
        return *this;
    }
    /// Offer this value to the OLED display.  The display has 3 lines (128x64) or
    /// 2 lines (128x32); the core fills them with the most important values of
    /// all enabled modules: `priority` 1 first (temperature), then 2, then 3.
    /// `tag` is the short label ("T", "CO2"); 32 px screens print `tag32`
    /// (default: the same tag) as "TEMP : 23.4 C".
    Channel &oled(uint8_t priority, const char *tag, const char *tag32 = nullptr)
    {
        s.oledPriority = priority;
        s.oledTag = tag;
        s.oledTag32 = tag32;
        return *this;
    }
    /// Publish this value on its own MQTT topic (setting MQTT_SLOT<slot>).
    Channel &mqtt(uint8_t slot)
    {
        s.mqttSlot = slot;
        return *this;
    }
    Channel &mqttLocation(const char *loc)
    {
        s.mqttLocation = loc;
        return *this;
    }
    /// Unit written to MQTT when it must differ from the display unit.
    Channel &mqttUnit(const char *u)
    {
        s.mqttUnit = u;
        return *this;
    }
    /// Not drawn on the dashboard (it is still part of the MQTT JSON).
    Channel &hidden()
    {
        s.hidden = true;
        return *this;
    }
    /// Only for the dashboard / OLED, never published to MQTT.
    Channel &localOnly()
    {
        s.localOnly = true;
        return *this;
    }
};

// -----------------------------------------------------------------------------
//  Calibration  —  optional guided procedure
// -----------------------------------------------------------------------------
//  The portal renders a wizard from this description, and the same description
//  is published to the MQTT hub as a capability.  The plugin only implements
//  the four hooks (calibrationStart / Tick / Capture / Finish).
// -----------------------------------------------------------------------------
struct CalStepSpec
{
    const char *id = nullptr;
    const __FlashStringHelper *title = nullptr;
    const __FlashStringHelper *text = nullptr;
    const char *point = nullptr; ///< MQTT "point" value, e.g. "min"
    const __FlashStringHelper *hubLabel = nullptr;
};

struct CalFieldSpec
{
    const char *key = nullptr;
    const __FlashStringHelper *label = nullptr;
    const char *type = "number"; ///< "number" | "boolean"  (hub reading schema)
    const char *unit = "";
    uint8_t decimals = 2;
    bool ui = false; ///< show in the wizard (measure panel and review)
};

class Calibration
{
public:
    static const uint8_t MAX_STEPS = 4;
    static const uint8_t MAX_FIELDS = 10;

    const __FlashStringHelper *title = nullptr;
    const __FlashStringHelper *introText = nullptr;
    const char *liveKey = "";
    float stableTol = 0.005f; ///< absolute tolerance on the live value
    uint8_t stableWindow = 5; ///< consecutive samples inside the tolerance
    // MQTT hub capability (leave mqttId null to not publish one)
    const char *mqttId = nullptr; ///< e.g. "calibrate_ph"
    const __FlashStringHelper *mqttLabel = nullptr;
    const __FlashStringHelper *mqttDescription = nullptr;
    const char *mqttReadingPath = nullptr; ///< e.g. "calibration/ph/reading"
    CalStepSpec steps[MAX_STEPS];
    uint8_t stepCount = 0;
    CalFieldSpec fields[MAX_FIELDS];
    uint8_t fieldCount = 0;

    Calibration &intro(const __FlashStringHelper *t)
    {
        introText = t;
        return *this;
    }
    /// Status field that carries the live number the user watches.
    Calibration &live(const char *key)
    {
        liveKey = key;
        return *this;
    }
    /// The live value counts as "stable" after `window` samples within `tol`.
    Calibration &stable(uint8_t window, float tol)
    {
        stableWindow = window;
        stableTol = tol;
        return *this;
    }
    Calibration &mqtt(const char *id, const __FlashStringHelper *label,
                      const char *readingPath, const __FlashStringHelper *description = nullptr)
    {
        mqttId = id;
        mqttLabel = label;
        mqttReadingPath = readingPath;
        mqttDescription = description;
        return *this;
    }
    /// A capture step.  `point` is the value the hub sends ({"action":"capture","point":"min"}).
    Calibration &step(const char *id, const __FlashStringHelper *title, const __FlashStringHelper *text,
                      const char *point = nullptr, const __FlashStringHelper *hubLabel = nullptr)
    {
        if (stepCount < MAX_STEPS)
        {
            CalStepSpec &st = steps[stepCount++];
            st.id = id;
            st.title = title;
            st.text = text;
            st.point = point;
            st.hubLabel = hubLabel;
        }
        return *this;
    }
    /// Declares one key of the calibration Status.  `ui` = show it in the wizard.
    Calibration &field(const char *key, const __FlashStringHelper *label, const char *type,
                       const char *unit, uint8_t decimals, bool ui)
    {
        if (fieldCount < MAX_FIELDS)
        {
            CalFieldSpec &f = fields[fieldCount++];
            f.key = key;
            f.label = label;
            f.type = type;
            f.unit = unit;
            f.decimals = decimals;
            f.ui = ui;
        }
        return *this;
    }
};

// -----------------------------------------------------------------------------
//  Descriptor  —  filled by Plugin::describe()
// -----------------------------------------------------------------------------
class Descriptor
{
public:
    static const uint8_t MAX_SETTINGS = 14;
    static const uint8_t MAX_CHANNELS = 8;

    // ---- identity ----------------------------------------------------------
    /// `name` is the short wire name used as MQTT sensorType and default metric.
    Descriptor &info(const char *name, const __FlashStringHelper *title,
                     const __FlashStringHelper *summary, Bus bus)
    {
        wireName = name;
        titleText = title;
        summaryText = summary;
        busType = bus;
        return *this;
    }
    /// Optional one-line note shown in the settings panel (wiring tips...).
    Descriptor &hint(const __FlashStringHelper *text)
    {
        hintText = text;
        return *this;
    }

    // ---- settings ----------------------------------------------------------
    Setting &number(const char *key, const __FlashStringHelper *label, float def)
    {
        return add(key, label, SettingType::Number, def);
    }
    Setting &pin(const char *key, const __FlashStringHelper *label, int def)
    {
        Setting &s = add(key, label, SettingType::Pin, (float)def);
        s.s.min = 0;
        s.s.max = 39;
        s.s.step = 1;
        return s;
    }
    /// I2C address, edited as hex (0x3C).
    Setting &address(const char *key, const __FlashStringHelper *label, uint8_t def)
    {
        Setting &s = add(key, label, SettingType::Address, (float)def);
        s.s.min = 1;
        s.s.max = 127;
        s.s.step = 1;
        return s;
    }
    Setting &text(const char *key, const __FlashStringHelper *label, const char *def)
    {
        Setting &s = add(key, label, SettingType::Text, 0);
        s.s.defText = def;
        return s;
    }
    /// A dropdown of numeric values:  .choice(...).option(11, F("DHT11")).option(22, F("DHT22"))
    Setting &choice(const char *key, const __FlashStringHelper *label, float def)
    {
        return add(key, label, SettingType::Choice, def);
    }
    Setting &toggle(const char *key, const __FlashStringHelper *label, bool def)
    {
        return add(key, label, SettingType::Toggle, def ? 1.0f : 0.0f);
    }

    /// Declares the standard MQTT settings for publishing slot `slot` (1..2):
    ///   MQTT_SLOT<slot>  default "<device name>/<topicTail>"
    ///   METRIC<slot>     default `metric`
    /// `what` (optional) is appended to the labels, e.g. F("temperature").
    void mqtt(uint8_t slot, const char *topicTail, const char *metric,
              const __FlashStringHelper *what = nullptr)
    {
        if (slot < 1 || slot > 2)
            return;
        Setting &t = add(slot == 1 ? "MQTT_SLOT1" : "MQTT_SLOT2", F("MQTT topic"), SettingType::Text, 0);
        t.s.defText = topicTail;
        t.s.deviceTopic = true;
        t.s.live = true;
        t.s.group = "MQTT";
        t.s.maxLen = 64;
        t.s.suffix = what;
        Setting &m = add(slot == 1 ? "METRIC1" : "METRIC2", F("Metric name"), SettingType::Text, 0);
        m.s.defText = metric;
        m.s.live = true;
        m.s.group = "MQTT";
        m.s.maxLen = 32;
        m.s.suffix = what;
    }

    // ---- readings ----------------------------------------------------------
    Channel &channel(const char *key, const __FlashStringHelper *label, const char *unit, uint8_t decimals = 1)
    {
        if (channelCount >= MAX_CHANNELS)
            return channels[MAX_CHANNELS - 1];
        Channel &c = channels[channelCount++];
        c.s = ChannelSpec();
        c.s.key = key;
        c.s.label = label;
        c.s.unit = unit;
        c.s.decimals = decimals;
        return c;
    }
    /// Publish ALL channels (also hidden ones) as one JSON object on MQTT slot `slot`.
    Descriptor &mqttJson(uint8_t slot)
    {
        jsonSlot = slot;
        return *this;
    }

    // ---- calibration -------------------------------------------------------
    Calibration &calibration(const __FlashStringHelper *title)
    {
        cal.title = title;
        hasCal = true;
        return cal;
    }

    // ---- filled in by describe(), read by the core --------------------------
    const char *wireName = "";
    const __FlashStringHelper *titleText = nullptr;
    const __FlashStringHelper *summaryText = nullptr;
    const __FlashStringHelper *hintText = nullptr;
    Bus busType = Bus::None;
    Setting settings[MAX_SETTINGS];
    uint8_t settingCount = 0;
    Channel channels[MAX_CHANNELS];
    uint8_t channelCount = 0;
    uint8_t jsonSlot = 0;
    bool hasCal = false;
    Calibration cal;

private:
    Setting &add(const char *key, const __FlashStringHelper *label, SettingType type, float def)
    {
        if (settingCount >= MAX_SETTINGS)
            return settings[MAX_SETTINGS - 1];
        Setting &s = settings[settingCount++];
        s.s = SettingSpec();
        s.s.key = key;
        s.s.label = label;
        s.s.type = type;
        s.s.def = def;
        return s;
    }
};

// -----------------------------------------------------------------------------
//  Runtime services handed to the plugin
// -----------------------------------------------------------------------------

/// Settings, sibling values and persistence.  Implemented by the core.
class Context
{
public:
    virtual ~Context() {}
    /// Numeric setting of THIS plugin (key without the "<code>_" prefix).
    virtual float num(const char *key) = 0;
    /// Text setting of THIS plugin.
    virtual const char *text(const char *key) = 0;
    /// Change a numeric setting in RAM (call save() to persist it).
    virtual bool setNum(const char *key, float value) = 0;
    /// Persist all settings to EEPROM.
    virtual void save() = 0;
    /// Latest valid value published by ANY plugin channel of this semantic
    /// `kind` (see Channel::kind), or NAN if there is none yet.
    virtual float latest(const char *kind) = 0;
};

/// Where read() delivers its measurements.  Implemented by the core.
class Readings
{
public:
    virtual ~Readings() {}
    /// Set a channel value.  Use NAN when the measurement failed.
    virtual void set(const char *channelKey, float value) = 0;
    /// The whole sensor failed (not wired, no answer...).  Shown in the portal.
    virtual void fail(const char *reason) = 0;
};

/// Key/value state of a running calibration.  Persisted by the core between
/// calls, so a plugin can read back what it stored earlier.
class Status
{
public:
    static const uint8_t MAX = 12;

    void clear()
    {
        n = 0;
        msg[0] = 0;
    }
    void set(const char *key, float v)
    {
        Entry *e = slot(key);
        if (e)
        {
            e->v = v;
            e->isBool = false;
        }
    }
    void set(const char *key, bool v)
    {
        Entry *e = slot(key);
        if (e)
        {
            e->v = v ? 1.0f : 0.0f;
            e->isBool = true;
        }
    }
    void set(const char *key, int v) { set(key, (float)v); }
    /// Human readable message shown in the wizard (errors, hints).
    void message(const char *text)
    {
        strncpy(msg, text ? text : "", sizeof(msg) - 1);
        msg[sizeof(msg) - 1] = 0;
    }
    float num(const char *key, float def = NAN) const
    {
        for (uint8_t i = 0; i < n; i++)
            if (strcmp(e[i].key, key) == 0)
                return e[i].v;
        return def;
    }
    bool flag(const char *key) const { return num(key, 0) != 0; }

    // iteration (used by the core)
    uint8_t count() const { return n; }
    const char *keyAt(uint8_t i) const { return e[i].key; }
    float valueAt(uint8_t i) const { return e[i].v; }
    bool isBoolAt(uint8_t i) const { return e[i].isBool; }
    const char *messageText() const { return msg; }

private:
    struct Entry
    {
        char key[16];
        float v;
        bool isBool;
    };
    Entry e[MAX];
    uint8_t n = 0;
    char msg[64] = {0};

    Entry *slot(const char *key)
    {
        for (uint8_t i = 0; i < n; i++)
            if (strcmp(e[i].key, key) == 0)
                return &e[i];
        if (n >= MAX)
            return nullptr;
        strncpy(e[n].key, key, sizeof(e[n].key) - 1);
        e[n].key[sizeof(e[n].key) - 1] = 0;
        return &e[n++];
    }
};

// -----------------------------------------------------------------------------
//  Plugin  —  the class a contributor implements
// -----------------------------------------------------------------------------
class Plugin
{
public:
    virtual ~Plugin() {}

    /// Describe settings, readings and (optionally) calibration.  Called by the
    /// core whenever it needs the schema; it must have no side effects.
    virtual void describe(Descriptor &d) = 0;

    /// Hardware init.  Called at boot for enabled plugins (after the I2C bus is
    /// up), when the plugin is switched on and after its settings changed.  Must
    /// be safe to call again: release/replace what a previous call created.
    /// Return false if the sensor does not answer; the portal then shows
    /// "sensor not found" until a read succeeds.
    virtual bool begin(Context &ctx)
    {
        (void)ctx;
        return true;
    }

    /// Periodic sampling (every "reading interval").  Push values with out.set().
    virtual void read(Context &ctx, Readings &out) = 0;

    // ---- optional calibration hooks (only if describe() declared one) -------
    /// A session starts.  Reset accumulators.  Return false to refuse.
    virtual bool calibrationStart(Context &ctx, Status &st)
    {
        (void)ctx;
        (void)st;
        return false;
    }
    /// Called about once a second while a session runs.  Take a sample and
    /// update the live value(s) in `st`.
    virtual void calibrationTick(Context &ctx, Status &st)
    {
        (void)ctx;
        (void)st;
    }
    /// The user captured step `stepId`.  Return true on success; on failure
    /// call st.message("why") and return false.
    virtual bool calibrationCapture(Context &ctx, const char *stepId, Status &st)
    {
        (void)ctx;
        (void)stepId;
        (void)st;
        return false;
    }
    /// The session ends.  If `save` is true apply and persist the result
    /// (ctx.setNum + ctx.save).  Return false (with st.message) if the result is
    /// not acceptable; nothing must be saved in that case.
    virtual bool calibrationFinish(Context &ctx, bool save, Status &st)
    {
        (void)ctx;
        (void)save;
        (void)st;
        return true;
    }
};

// -----------------------------------------------------------------------------
//  Registration
// -----------------------------------------------------------------------------
typedef Plugin *(*PluginFactory)();

/// Implemented by the core.  Use the AUTOSENSE_REGISTER macro instead of calling it.
void registerPlugin(const char *code, PluginFactory factory);

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

/// True if `pin` can be used with analogRead() while Wi-Fi is on.
inline bool isAdc1Pin(int pin)
{
#if defined(ESP32)
    return pin >= 32 && pin <= 39;
#else
    (void)pin;
    return true; // ESP8266 has a single ADC (A0)
#endif
}

/// Average of `samples` analogRead() calls, `spacingMs` apart.
inline float analogAverage(int pin, uint8_t samples = 10, uint8_t spacingMs = 2)
{
    if (samples == 0)
        return 0.0f;
    float sum = 0;
    for (uint8_t i = 0; i < samples; i++)
    {
#if defined(ESP32)
        sum += analogRead(pin);
#else
        (void)pin;
        sum += analogRead(A0);
#endif
        if (spacingMs)
            delay(spacingMs);
    }
    return sum / samples;
}

/// Full-scale ADC reading of the board (4095 on ESP32, 1023 on ESP8266).
inline float adcMax()
{
#if defined(ESP32)
    return 4095.0f;
#else
    return 1023.0f;
#endif
}

/// Migration helper: firmware < 2.0 stored an analog INDEX (A0 + index) for some
/// plugins.  Use it as   .legacy("A_index", autosense::legacyAnalogIndexToGpio)
inline float legacyAnalogIndexToGpio(float index)
{
    return (float)(A0 + (int)index);
}

/// Migration helper for plugins that already stored a GPIO number on ESP32
/// (ESP8266 only has A0).
inline float legacyAnalogGpio(float stored)
{
#if defined(ESP32)
    return stored;
#else
    (void)stored;
    return (float)A0;
#endif
}

} // namespace autosense

/// Registers a plugin class under its code.  Put ONE of these at the bottom of
/// your plugin's .cpp file:   AUTOSENSE_REGISTER("P012", MyPlugin)
#define AUTOSENSE_REGISTER(CODE, CLASS)                                             \
    static autosense::Plugin *autosense_make_##CLASS() { return new CLASS(); }      \
    __attribute__((constructor)) static void autosense_register_##CLASS()           \
    {                                                                               \
        autosense::registerPlugin(CODE, autosense_make_##CLASS);                    \
    }

// AutoSense plugin simulator  —  runs a plugin WITHOUT the AutoSense core.
// Built and started by sdk/tools/check_plugin.py.  Prints the plugin schema
// as JSON, validates it, then runs begin() and a few read() cycles with a
// simulated analog input (AUTOSENSE_SIM_ADC, default mid-scale).
#include <AutoSensePlugin.h>
#include <Wire.h>
#include <SPI.h>
#include <map>
#include <string>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>

using namespace autosense;

unsigned long g_millis = 1000;
int g_adc = 2048;
bool g_serialEcho = true;
HardwareSerial Serial;
TwoWire Wire;
SPIClass SPI;
EspClass ESP;

// function-local: plugin constructors run before ordinary globals are built
static std::vector<std::pair<std::string, PluginFactory>> &regs()
{
    static std::vector<std::pair<std::string, PluginFactory>> r;
    return r;
}
void autosense::registerPlugin(const char *code, PluginFactory f) { regs().push_back({code, f}); }

static int g_errors = 0, g_warnings = 0;
#define ERR(...) do { printf("  ERROR   "); printf(__VA_ARGS__); printf("\n"); g_errors++; } while (0)
#define WARN(...) do { printf("  WARNING "); printf(__VA_ARGS__); printf("\n"); g_warnings++; } while (0)

static const char *S(const __FlashStringHelper *f) { return f ? (const char *)f : ""; }
static std::string js(const char *s)
{
    std::string o = "\"";
    for (; s && *s; s++) { if (*s == '"' || *s == '\\') o += '\\'; o += *s; }
    return o + "\"";
}

struct SimContext : Context
{
    std::map<std::string, float> nums;
    std::map<std::string, std::string> texts;
    float num(const char *k) override
    {
        auto it = nums.find(k);
        if (it == nums.end()) { ERR("ctx.num(\"%s\"): no such numeric setting", k); return 0; }
        return it->second;
    }
    const char *text(const char *k) override
    {
        auto it = texts.find(k);
        if (it == texts.end()) { ERR("ctx.text(\"%s\"): no such text setting", k); return ""; }
        return it->second.c_str();
    }
    bool setNum(const char *k, float v) override
    {
        if (!nums.count(k)) { ERR("ctx.setNum(\"%s\"): no such numeric setting", k); return false; }
        nums[k] = v; printf("  ctx.setNum(%s, %g)\n", k, v); return true;
    }
    void save() override { printf("  ctx.save()\n"); }
    float latest(const char *kind) override
    {
        if (!strcmp(kind, "temperature") || !strcmp(kind, "liquid_temperature")) return 22.0f;
        if (!strcmp(kind, "humidity")) return 50.0f;
        return NAN;
    }
};

struct SimReadings : Readings
{
    Descriptor *d;
    void set(const char *k, float v) override
    {
        bool known = false;
        for (int i = 0; i < d->channelCount; i++) if (!strcmp(d->channels[i].s.key, k)) known = true;
        if (!known) ERR("out.set(\"%s\"): channel not declared in describe()", k);
        printf("    %-14s = %g\n", k, v);
    }
    void fail(const char *r) override { printf("    fail(\"%s\")\n", r); }
};

static sigjmp_buf g_jmp;
static void onCrash(int sig) { signal(sig, SIG_DFL); siglongjmp(g_jmp, 1); }

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0); // keep the output if library code crashes
    if (getenv("AUTOSENSE_SIM_ADC")) g_adc = atoi(getenv("AUTOSENSE_SIM_ADC"));
    if (regs().size() != 1) { printf("  ERROR   expected exactly one AUTOSENSE_REGISTER, found %d\n", (int)regs().size()); return 2; }
    const char *code = regs()[0].first.c_str();
    if (strlen(code) != 4 || code[0] != 'P' || !isdigit(code[1]) || !isdigit(code[2]) || !isdigit(code[3]))
        WARN("plugin code \"%s\" should look like P012", code);

    Plugin *p = regs()[0].second();
    Descriptor *d = new Descriptor();
    p->describe(*d);

    // ---- schema as JSON -------------------------------------------------------
    printf("SCHEMA {\"code\":%s,\"name\":%s,\"title\":%s,\"summary\":%s,\"settings\":[", js(code).c_str(), js(d->wireName).c_str(), js(S(d->titleText)).c_str(), js(S(d->summaryText)).c_str());
    for (int i = 0; i < d->settingCount; i++)
    {
        const SettingSpec &s = d->settings[i].s;
        printf("%s{\"key\":%s,\"label\":%s,\"type\":%d,\"default\":", i ? "," : "", js(s.key).c_str(), js(S(s.label)).c_str(), (int)s.type);
        if (s.type == SettingType::Text) printf("%s", js(s.defText).c_str()); else printf("%g", s.def);
        printf("}");
    }
    printf("],\"channels\":[");
    for (int i = 0; i < d->channelCount; i++)
    {
        const ChannelSpec &c = d->channels[i].s;
        printf("%s{\"key\":%s,\"label\":%s,\"unit\":%s,\"kind\":%s,\"oled\":%d,\"mqttSlot\":%d}", i ? "," : "", js(c.key).c_str(), js(S(c.label)).c_str(), js(c.unit).c_str(), js(c.kind).c_str(), c.oledPriority, c.mqttSlot);
    }
    printf("],\"calibration\":%s}\n", d->hasCal ? js(S(d->cal.title)).c_str() : "null");

    // ---- validation ----------------------------------------------------------------
    printf("\nChecks\n");
    if (!*d->wireName) ERR("d.info(...) missing (wire name)");
    if (!d->titleText) ERR("d.info(...) missing (title)");
    if (d->settingCount >= Descriptor::MAX_SETTINGS) ERR("too many settings (max %d)", Descriptor::MAX_SETTINGS - 1);
    if (d->channelCount >= Descriptor::MAX_CHANNELS) ERR("too many channels (max %d)", Descriptor::MAX_CHANNELS - 1);
    std::map<std::string, int> seen;
    for (int i = 0; i < d->settingCount; i++)
    {
        const SettingSpec &s = d->settings[i].s;
        if (!s.key || !*s.key) { ERR("setting %d has no key", i); continue; }
        if (strlen(s.key) > 15) ERR("setting key \"%s\" is longer than 15 characters", s.key);
        if (seen[s.key]++) ERR("setting key \"%s\" declared twice", s.key);
        if (!s.label) ERR("setting \"%s\" has no label", s.key);
        if (s.type == SettingType::Choice && s.optionCount == 0) ERR("choice \"%s\" has no .option(...)", s.key);
        if (s.type != SettingType::Text && !isnan(s.min) && !isnan(s.max) && (s.def < s.min || s.def > s.max))
            ERR("default of \"%s\" (%g) is outside its range %g..%g", s.key, s.def, s.min, s.max);
    }
    std::map<std::string, int> cseen;
    for (int i = 0; i < d->channelCount; i++)
    {
        const ChannelSpec &c = d->channels[i].s;
        if (cseen[c.key]++) ERR("channel \"%s\" declared twice", c.key);
        if (c.mqttSlot)
        {
            const char *need = c.mqttSlot == 1 ? "MQTT_SLOT1" : "MQTT_SLOT2";
            if (!seen.count(need)) ERR("channel \"%s\" uses MQTT slot %d but d.mqtt(%d, ...) is missing", c.key, c.mqttSlot, c.mqttSlot);
        }
        if (c.oledPriority && strlen(c.oledTag) > 4) WARN("OLED tag \"%s\" is long; 1-4 characters fit best", c.oledTag);
        if (!c.label) ERR("channel \"%s\" has no label", c.key);
    }
    if (d->jsonSlot && !seen.count(d->jsonSlot == 1 ? "MQTT_SLOT1" : "MQTT_SLOT2")) ERR("mqttJson(%d) without d.mqtt(%d, ...)", d->jsonSlot, d->jsonSlot);
    if (d->hasCal)
    {
        const Calibration &c = d->cal;
        if (c.stepCount == 0) ERR("calibration has no .step(...)");
        bool live = false;
        for (int i = 0; i < c.fieldCount; i++) if (!strcmp(c.fields[i].key, c.liveKey)) live = true;
        if (!live) ERR("calibration .live(\"%s\") is not one of its .field(...)s", c.liveKey);
        for (int i = 0; i < c.fieldCount; i++) if (strlen(c.fields[i].key) > 15) ERR("calibration field \"%s\" longer than 15 characters", c.fields[i].key);
    }
    if (!g_errors) printf("  schema OK\n");

    // ---- run -------------------------------------------------------------------------
    SimContext ctx;
    for (int i = 0; i < d->settingCount; i++)
    {
        const SettingSpec &s = d->settings[i].s;
        if (s.type == SettingType::Text) ctx.texts[s.key] = s.deviceTopic ? std::string("AutoSense/") + s.defText : s.defText;
        else ctx.nums[s.key] = s.def;
    }
    SimReadings out; out.d = d;
    signal(SIGSEGV, onCrash);
    signal(SIGABRT, onCrash);
    signal(SIGILL, onCrash);
#ifdef SIGBUS
    signal(SIGBUS, onCrash);
#endif
    if (sigsetjmp(g_jmp, 1))
    {
        printf("\n  (stopped: the plugin called sensor-library code that cannot run in the simulator;\n   describe() and the checks above are still valid)\n");
        fflush(stdout);
        _exit(g_errors ? 1 : 0); // the heap may be damaged: leave without running destructors
    }
    printf("\nSimulation (analog input = %d)\n", g_adc);
    bool began = p->begin(ctx);
    printf("  begin() -> %s\n", began ? "true" : "false (sensor not found)");
    for (int r = 0; r < 3; r++) { printf("  read() #%d\n", r + 1); g_millis += 30000; p->read(ctx, out); }
    if (d->hasCal)
    {
        Status st;
        printf("  calibrationStart() -> %s\n", p->calibrationStart(ctx, st) ? "true" : "false");
        for (int t = 0; t < 6; t++) { g_millis += 1000; p->calibrationTick(ctx, st); }
        for (int i = 0; i < d->cal.stepCount; i++, g_adc += 500) // each buffer reads differently
            printf("  calibrationCapture(\"%s\") -> %s %s\n", d->cal.steps[i].id, p->calibrationCapture(ctx, d->cal.steps[i].id, st) ? "true" : "false", st.messageText());
        bool ok = p->calibrationFinish(ctx, true, st);
        printf("  calibrationFinish(save) -> %s %s\n  status:", ok ? "true" : "false", st.messageText());
        for (int i = 0; i < st.count(); i++) printf(" %s=%g", st.keyAt(i), st.valueAt(i));
        printf("\n");
    }
    printf("\n%d error(s), %d warning(s)\n", g_errors, g_warnings);
    return g_errors ? 1 : 0;
}

#pragma once

#include <functional>
#include <map>
#include <string>
#include "PluginInterface.h"

// PluginRegistry handles self-registration for all AutoSense plugins.
// Plugins register themselves using the factory hook:
// __attribute__((constructor)) void registerYourPluginFactory() {
//     PluginRegistry::getInstance().registerFactory("PXXX", []() -> Plugin* {
//         return new PXXX();
//     });
// }

class PluginRegistry {
public:
    using PluginFactory = std::function<Plugin*()>;

    static PluginRegistry& getInstance();

    void registerFactory(const std::string& id, PluginFactory factory);
    Plugin* createPlugin(const std::string& id);
    const std::map<std::string, PluginFactory>& getFactories() const { return factories; }

private:
    std::map<std::string, PluginFactory> factories;
};

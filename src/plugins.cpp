/**
  Author: sascha_lammers@gmx.de
*/

#include "plugins.h"
#include <Form.h>
#include <algorithm>
#include <TableDumper.h>
#include "RTCMemoryManager.h"
#include "misc.h"
#ifndef DISABLE_EVENT_SCHEDULER
#include <EventScheduler.h>
#endif

#define PLUGIN_RTC_MEM_MAX_ID       255

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

PluginsVector plugins;

// we need to store the plugins somewhere until the vector is initialized otherwise it will discard already registered plugins
static PluginsVector *pluginsPtr = nullptr;

void register_plugin(PluginComponent *plugin) {
    if (plugins.size()) {
        debug_println(F("Cannot register plugins anymore"));
        panic();
    }
    _debug_printf_P(PSTR("register_plugin %s priority %d\n"), plugin->getName(), plugin->getSetupPriority());
    if (!pluginsPtr) {
        pluginsPtr = new PluginsVector();
        pluginsPtr->reserve(24);
    }
    pluginsPtr->push_back(plugin);
}

void dump_plugin_list(Print &output) {

#if DEBUG

    constexpr const TableDumper::Length_t __defaultLength = 6;

    TableDumper dumper(Serial);
    dumper.setStrBoolValues(F("yes"), F("no"));
    dumper.setStrNullValue(F("<none>"));

    dumper.addColumn(F("Name"), __defaultLength + 3);
    dumper.addColumn(F("Priority"), __defaultLength, TableDumperColumn::RIGHT);
    dumper.addColumn(F("Safe mode"), __defaultLength);
    dumper.addColumn(F("Auto Wake-Up"), __defaultLength);
    dumper.addColumn(F("RTC Mem Id"), __defaultLength, TableDumperColumn::RIGHT);
    dumper.addColumn(F("Setup"), __defaultLength);
    dumper.addColumn(F("Status"), __defaultLength);
    dumper.addColumn(F("Conf Form"), __defaultLength);
    dumper.addColumn(F("Reconfigure"), __defaultLength);
    dumper.addColumn(F("Reconfigure Deps"), __defaultLength);
    dumper.addColumn(F("Deep Sleep"), __defaultLength);
    dumper.addColumn(F("AT Mode"), __defaultLength);
    dumper.startTable();
    for(const auto plugin : plugins) {
        dumper.addData(plugin->getName());
        dumper.addData(plugin->getSetupPriority());
        dumper.addData(plugin->allowSafeMode());
        dumper.addData(plugin->autoSetupAfterDeepSleep());
        dumper.addData(plugin->getRtcMemoryId());
        // dumper.addStrBoolData(plugin->hasStatus());
        // dumper.addStrBoolData(plugin->rec);
        // dumper.addStrBoolData(plugin.getReconfigurePlugin());
        // dumper.addData(plugin.getReconfigurePluginDependeciesPSTR());
        // dumper.addStrBoolData(plugin.getPrepareDeepSleep());
        // dumper.addStrBoolData(plugin.getAtModeCommandHandler());
    }
    dumper.endTable();
#endif

}

void prepare_plugins() {

    // copy plugins and free temporary data
    plugins.reserve(pluginsPtr->size());
    for(auto plugin: *pluginsPtr) {
         plugins.push_back(plugin);
    }
    free(pluginsPtr);

    _debug_printf_P(PSTR("prepare_plugins() counter %d\n"), plugins.size());

    std::sort(plugins.begin(), plugins.end(), [](const PluginComponent *a, const PluginComponent *b) {
        return b->getSetupPriority() >= a->getSetupPriority();
    });

#if DEBUG
    // check for duplicate RTC memory ids
    uint8_t i = 0;
    for(auto plugin: plugins) {
#if DEBUG_PLUGINS
        _debug_printf_P(PSTR("%s prio %d\n"), plugin->getName(), plugin->getSetupPriority());
#endif

        if (plugin->getRtcMemoryId() > PLUGIN_RTC_MEM_MAX_ID) {
            _debug_printf_P(PSTR("WARNING! Plugin %s is using an invalid id %d\n"), plugin->getName(), plugin->getRtcMemoryId());
        }
        if (plugin->getRtcMemoryId()) {
            uint8_t j = 0;
            for(auto plugin2: plugins) {
                if (i != j && plugin2->getRtcMemoryId() && plugin->getRtcMemoryId() == plugin2->getRtcMemoryId()) {
                    _debug_printf_P(PSTR("WARNING! Plugin %s and %s use the same id (%d) to identify the RTC memory data\n"), plugin->getName(), plugin2->getName(), plugin->getRtcMemoryId());
                }
                j++;
            }
        }
        i++;
    }
#endif

}

void setup_plugins(PluginComponent::PluginSetupMode_t mode) {

    _debug_printf_P(PSTR("setup_plugins(%d) counter %d\n"), mode, plugins.size());

    for(auto plugin : plugins) {
        bool runSetup =
            (mode == PluginComponent::PLUGIN_SETUP_DEFAULT) ||
            (mode == PluginComponent::PLUGIN_SETUP_SAFE_MODE && plugin->allowSafeMode()) ||
            (mode == PluginComponent::PLUGIN_SETUP_AUTO_WAKE_UP && plugin->autoSetupAfterDeepSleep()) ||
            (mode == PluginComponent::PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP && !plugin->autoSetupAfterDeepSleep());
        _debug_printf_P(PSTR("setup_plugins(%d) %s priority %d run setup %d\n"), mode, plugin->getName(), plugin->getSetupPriority(), runSetup);
        if (runSetup) {
            plugin->setup(mode);
        }
    }

#ifndef DISABLE_EVENT_SCHEDULER
    if (mode == PluginComponent::PLUGIN_SETUP_AUTO_WAKE_UP) {
        Scheduler.addTimer(30000, false, [](EventScheduler::TimerPtr timer) {
            setup_plugins(PluginComponent::PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP);
        });
    }
#endif
}

// PluginConfiguration *get_plugin_by_form(const String &name) {
//     // custom filenames hack
// #if IOT_ATOMIC_SUN_V2
//     if (name.equals(F("dimmer_cfg"))) {
//         return get_plugin_by_name(PSTR("atomicsun"));
//     }
// #endif
// #if IOT_DIMMER_MODULE
//     if (name.equals(F("dimmer_cfg"))) {
//         return get_plugin_by_name(PSTR("dimmer"));
//     }
//     else if (name.equals(F("dimmer"))) {
//         return nullptr;
//     }
// #endif
//     return get_plugin_by_name(name);
// }

// PluginConfiguration *get_plugin_by_name(const String &name) {
//     for(auto &plugin: plugins) {
//         if (plugin.pluginNameEquals(name)) {
//             _debug_printf_P(PSTR("get_plugin_by_name(%s) = %p\n"), name.c_str(), &plugin);
//             return &plugin;
//         }
//     }
//     _debug_printf_P(PSTR("get_plugin_by_name(%s) = nullptr\n"), name.c_str());
//     return nullptr;
// }

// PluginConfiguration *get_plugin_by_rtc_memory_id(uint8_t id) {
//     if (id <= 0 || id > PLUGIN_RTC_MEM_MAX_ID) {
//         _debug_printf_P(PSTR("get_plugin_by_rtc_memory_id(%d): invalid id\n"), id);
//         return nullptr;
//     }
//     for(auto &plugin: plugins) {
//         if (plugin.getRtcMemoryId() == id) {
//             return &plugin;
//         }
//     }
//     return nullptr;
// }

// PluginConfiguration::PluginConfiguration() {
//     config = nullptr;
// }

// PluginConfiguration::PluginConfiguration(PGM_PLUGIN_CONFIG_P configPtr) {
//     config = configPtr;
// }

// bool PluginConfiguration::pluginNameEquals(const String & string) const {
//     return strcmp_P(string.c_str(), config->pluginName) == 0;
// }

// bool PluginConfiguration::pluginNameEquals(const __FlashStringHelper * string) const {
//     return strcmp_P_P(config->pluginName, reinterpret_cast<PGM_P>(string)) == 0;
// }

// String PluginConfiguration::getPluginName() const {
//     return FPSTR(config->pluginName);
// }

// PGM_P PluginConfiguration::getPluginNamePSTR() const {
//     return config->pluginName;
// }

// int8_t PluginConfiguration::getSetupPriority() const {
//     return (int8_t)pgm_read_byte(&config->setupPriority);
// }

// bool PluginConfiguration::isAllowSafeMode() const {
//     return pgm_read_byte(&config->allowSafeMode);
// }

// bool PluginConfiguration::isAutoSetupAfterDeepSleep() const {
//     return pgm_read_byte(&config->autoSetupAfterDeepSleep);
// }

// uint8_t PluginConfiguration::getRtcMemoryId() const {
//     return pgm_read_byte(&config->rtcMemoryId);
// }

// SetupPluginCallback PluginConfiguration::getSetupPlugin() const {
//     return config->setupPlugin;
// }

// void PluginConfiguration::callSetupPlugin() {
//     // auto callback = getSetupPlugin();
//     if (config->setupPlugin) {
//         config->setupPlugin();
//     }
// }

// StatusTemplateCallback PluginConfiguration::getStatusTemplate() const {
//     return config->statusTemplate;
// }

// ConfigureFormCallback PluginConfiguration::getConfigureForm() const {
//     return config->configureForm;
// }

// ReconfigurePluginCallback PluginConfiguration::getReconfigurePlugin() const {
//     return config->reconfigurePlugin;
// }

// void PluginConfiguration::callReconfigurePlugin(PGM_P source) {
//     // auto callback = getReconfigurePlugin();
//     if (config->reconfigurePlugin) {
//         config->reconfigurePlugin(source);
//         for(auto &plugin: plugins) {
//             if (plugin.getConfigureForm() && plugin.isDependency(getPluginNamePSTR())) {
//                 plugin.callReconfigurePlugin(getPluginNamePSTR());
//             }
//         }
//     }
// }

// void PluginConfiguration::callReconfigureSystem(PGM_P name) {
//     for(auto &plugin: plugins) {
//         if (plugin.getConfigureForm() && plugin.isDependency(name)) {
//             plugin.callReconfigurePlugin(name);
//         }
//     }
// }

// String PluginConfiguration::getReconfigurePluginDependecies() const {
//     if (!config->configureForm || !config->reconfigurePluginDependencies) {
//         return _sharedEmptyString;
//     }
//     return FPSTR(config->reconfigurePluginDependencies);
// }

// PGM_P PluginConfiguration::getReconfigurePluginDependeciesPSTR() const {
//     return config->reconfigurePluginDependencies;
// }

// bool PluginConfiguration::isDependency(PGM_P pluginName) const {
//     if (!config->reconfigurePluginDependencies) {
//         return false;
//     }
//     return stringlist_find_P_P(config->reconfigurePluginDependencies, pluginName) != -1;
// }

// PrepareDeepSleepCallback PluginConfiguration::getPrepareDeepSleep() const {
//     return config->prepareDeepSleep;
// }

// void PluginConfiguration::callPrepareDeepSleep(uint32_t time, RFMode mode) {
//     //auto callback = getPrepareDeepSleep();
//     if (config->prepareDeepSleep) {
//         config->prepareDeepSleep(time, mode);
//     }
// }

// #if AT_MODE_SUPPORTED

// AtModeCommandHandlerCallback PluginConfiguration::getAtModeCommandHandler() const {
//     return config->atModeCommandHandler;
//     //return reinterpret_cast<AtModeCommandHandlerCallback>(pgm_read_ptr(&config->atModeCommandHandler));
// }

// bool PluginConfiguration::callAtModeCommandHandler(Stream & serial, const String & command, int8_t argc, char ** argv) {
//     // auto callback = getAtModeCommandHandler();
//     if (config->atModeCommandHandler) {
//         return config->atModeCommandHandler(serial, command, argc, argv);
//     }
//     return false;
// }

// #endif

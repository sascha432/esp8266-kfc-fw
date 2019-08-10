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

void register_plugin(PGM_PLUGIN_CONFIG_P config) {
    PluginConfiguration plugin(config);
    _debug_printf_P(PSTR("register_plugin %s priority %d\n"), plugin.getPluginNamePSTR(), plugin.getSetupPriority());
    plugins.push_back(plugin);
}

PROGMEM_PLUGIN_CONFIG_DECL(rd);
PROGMEM_PLUGIN_CONFIG_DECL(cfg);
PROGMEM_PLUGIN_CONFIG_DECL(pinmon);
PROGMEM_PLUGIN_CONFIG_DECL(mdns);
PROGMEM_PLUGIN_CONFIG_DECL(esp8266at);
PROGMEM_PLUGIN_CONFIG_DECL(http);
PROGMEM_PLUGIN_CONFIG_DECL(syslog);
PROGMEM_PLUGIN_CONFIG_DECL(ntp);
PROGMEM_PLUGIN_CONFIG_DECL(mqtt);
PROGMEM_PLUGIN_CONFIG_DECL(pingmon);
PROGMEM_PLUGIN_CONFIG_DECL(http2ser);
PROGMEM_PLUGIN_CONFIG_DECL(ser2tcp);
PROGMEM_PLUGIN_CONFIG_DECL(hass);
PROGMEM_PLUGIN_CONFIG_DECL(hue);
PROGMEM_PLUGIN_CONFIG_DECL(atomicsun);
PROGMEM_PLUGIN_CONFIG_DECL(dimmer);
PROGMEM_PLUGIN_CONFIG_DECL(filemgr);
PROGMEM_PLUGIN_CONFIG_DECL(i2c_scan);
PROGMEM_PLUGIN_CONFIG_DECL(ssd1306);

void register_all_plugins() {
    _debug_println(F("register_all_plugins()"));

    register_plugin(SPGM_PLUGIN_CONFIG_P(rd));
    register_plugin(SPGM_PLUGIN_CONFIG_P(cfg));

#if MDNS_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(mdns));
#endif
#if PIN_MONITOR
    register_plugin(SPGM_PLUGIN_CONFIG_P(pinmon));
#endif
#if ESP8266_AT_MODE_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(esp8266at));
#endif
#if WEBSERVER_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(http));
#endif
#if SYSLOG
    register_plugin(SPGM_PLUGIN_CONFIG_P(syslog));
#endif
#if NTP_CLIENT
    register_plugin(SPGM_PLUGIN_CONFIG_P(ntp));
#endif
#if MQTT_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(mqtt));
#endif
#if PING_MONITOR
    register_plugin(SPGM_PLUGIN_CONFIG_P(pingmon));
#endif
#if HTTP2SERIAL
    register_plugin(SPGM_PLUGIN_CONFIG_P(http2ser));
#endif
#if SERIAL2TCP
    register_plugin(SPGM_PLUGIN_CONFIG_P(ser2tcp));
#endif
#if HOME_ASSISTANT_INTEGRATION
    register_plugin(SPGM_PLUGIN_CONFIG_P(hass));
#endif
#if HUE_EMULATION
    register_plugin(SPGM_PLUGIN_CONFIG_P(hue));
#endif
#if IOT_ATOMIC_SUN_V2
    register_plugin(SPGM_PLUGIN_CONFIG_P(atomicsun));
#endif
#if IOT_DIMMER_MODULE
    register_plugin(SPGM_PLUGIN_CONFIG_P(dimmer));
#endif
#if FILE_MANAGER
    register_plugin(SPGM_PLUGIN_CONFIG_P(filemgr));
#endif
#if I2CSCANNER_PLUGIN
    register_plugin(SPGM_PLUGIN_CONFIG_P(i2c_scan));
#endif
#if SSD1306_PLUGIN
    register_plugin(SPGM_PLUGIN_CONFIG_P(ssd1306));
#endif

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
    for(const auto &plugin : plugins) {
        dumper.addData(plugin.getPluginNamePSTR());
        dumper.addData(plugin.getSetupPriority());
        dumper.addData(plugin.isAllowSafeMode());
        dumper.addData(plugin.isAutoSetupAfterDeepSleep());
        dumper.addData(plugin.getRtcMemoryId());
        dumper.addStrBoolData(plugin.getSetupPlugin());
        dumper.addStrBoolData(plugin.getStatusTemplate());
        dumper.addStrBoolData(plugin.getConfigureForm());
        dumper.addStrBoolData(plugin.getReconfigurePlugin());
        dumper.addData(plugin.getReconfigurePluginDependeciesPSTR());
        dumper.addStrBoolData(plugin.getPrepareDeepSleep());
        dumper.addStrBoolData(plugin.getAtModeCommandHandler());
    }
    dumper.endTable();
#endif

}

void prepare_plugins() {

    _debug_printf_P(PSTR("prepare_plugins() counter %d\n"), plugins.size());

    std::sort(plugins.begin(), plugins.end(), [](const PluginConfiguration &a, const PluginConfiguration &b) {
        return b.getSetupPriority() >= a.getSetupPriority();
    });

#if DEBUG
    // check for duplicate RTC memory ids
    uint8_t i = 0;
    for(auto &plugin: plugins) {
#if DEBUG_PLUGINS
        _debug_printf_P(PSTR("%s prio %d\n"), plugin.getPluginNamePSTR(), plugin.getSetupPriority());
#endif

        if (plugin.getRtcMemoryId() > PLUGIN_RTC_MEM_MAX_ID) {
            _debug_printf_P(PSTR("WARNING! Plugin %s is using an invalid id %d\n"), plugin.getPluginNamePSTR(), plugin.getRtcMemoryId());
        }
        if (plugin.getRtcMemoryId()) {
            uint8_t j = 0;
            for(auto &plugin2: plugins) {
                if (i != j && plugin2.getRtcMemoryId() && plugin.getRtcMemoryId() == plugin2.getRtcMemoryId()) {
                    _debug_printf_P(PSTR("WARNING! Plugin %s and %s use the same id (%d) to identify the RTC memory data\n"), plugin.getPluginNamePSTR(), plugin2.getPluginNamePSTR(), plugin.getRtcMemoryId());
                }
                j++;
            }
        }
        i++;
    }
#endif

}

void setup_plugins(PluginSetupMode_t mode) {

    _debug_printf_P(PSTR("setup_plugins(%d) counter %d\n"), mode, plugins.size());

    for(auto &plugin : plugins) {
        bool runSetup =
            (mode == PLUGIN_SETUP_DEFAULT) ||
            (mode == PLUGIN_SETUP_SAFE_MODE && plugin.isAllowSafeMode()) ||
            (mode == PLUGIN_SETUP_AUTO_WAKE_UP && plugin.isAutoSetupAfterDeepSleep()) ||
            (mode == PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP && !plugin.isAutoSetupAfterDeepSleep());
        _debug_printf_P(PSTR("setup_plugins(%d) %s priority %d run setup %d\n"), mode, plugin.getPluginNamePSTR(), plugin.getSetupPriority(), runSetup);
        if (runSetup) {
            plugin.callSetupPlugin();
        }
    }

#ifndef DISABLE_EVENT_SCHEDULER
    if (mode == PLUGIN_SETUP_AUTO_WAKE_UP) {
        Scheduler.addTimer(30000, false, [](EventScheduler::TimerPtr timer) {
            setup_plugins(PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP);
        });
    }
#endif
}

PluginConfiguration *get_plugin_by_name(PGM_P name) {
    for(auto &plugin: plugins) {
        if (plugin.pluginNameEquals(FPSTR(name))) {
            _debug_printf_P(PSTR("get_plugin_by_name(%s) = %p\n"), name, &plugin);
            return &plugin;
        }
    }
    _debug_printf_P(PSTR("get_plugin_by_name(%s) = nullptr\n"), name);
    return nullptr;
}

PluginConfiguration *get_plugin_by_form(const String &name) {
    // custom filenames hack
#if IOT_ATOMIC_SUN_V2
    if (name.equals(F("dimmer_cfg"))) {
        return get_plugin_by_name(PSTR("atomicsun"));
    }
#endif
#if IOT_DIMMER_MODULE
    if (name.equals(F("dimmer_cfg"))) {
        return get_plugin_by_name(PSTR("dimmer"));
    }
    else if (name.equals(F("dimmer"))) {
        return nullptr;
    }
#endif
    return get_plugin_by_name(name);
}

PluginConfiguration *get_plugin_by_name(const String &name) {
    for(auto &plugin: plugins) {
        if (plugin.pluginNameEquals(name)) {
            _debug_printf_P(PSTR("get_plugin_by_name(%s) = %p\n"), name.c_str(), &plugin);
            return &plugin;
        }
    }
    _debug_printf_P(PSTR("get_plugin_by_name(%s) = nullptr\n"), name.c_str());
    return nullptr;
}

PluginConfiguration *get_plugin_by_rtc_memory_id(uint8_t id) {
    if (id <= 0 || id > PLUGIN_RTC_MEM_MAX_ID) {
        _debug_printf_P(PSTR("get_plugin_by_rtc_memory_id(%d): invalid id\n"), id);
        return nullptr;
    }
    for(auto &plugin: plugins) {
        if (plugin.getRtcMemoryId() == id) {
            return &plugin;
        }
    }
    return nullptr;
}

PluginConfiguration::PluginConfiguration() {
    config = nullptr;
}

PluginConfiguration::PluginConfiguration(PGM_PLUGIN_CONFIG_P configPtr) {
    config = configPtr;
}

bool PluginConfiguration::pluginNameEquals(const String & string) const {
    return strcmp_P(string.c_str(), config->pluginName) == 0;
}

bool PluginConfiguration::pluginNameEquals(const __FlashStringHelper * string) const {
    return strcmp_P_P(config->pluginName, reinterpret_cast<PGM_P>(string)) == 0;
}

String PluginConfiguration::getPluginName() const {
    return FPSTR(config->pluginName);
}

PGM_P PluginConfiguration::getPluginNamePSTR() const {
    return config->pluginName;
}

int8_t PluginConfiguration::getSetupPriority() const {
    return (int8_t)pgm_read_byte(&config->setupPriority);
}

bool PluginConfiguration::isAllowSafeMode() const {
    return pgm_read_byte(&config->allowSafeMode);
}

bool PluginConfiguration::isAutoSetupAfterDeepSleep() const {
    return pgm_read_byte(&config->autoSetupAfterDeepSleep);
}

uint8_t PluginConfiguration::getRtcMemoryId() const {
    return pgm_read_byte(&config->rtcMemoryId);
}

SetupPluginCallback PluginConfiguration::getSetupPlugin() const {
    return config->setupPlugin;
}

void PluginConfiguration::callSetupPlugin() {
    // auto callback = getSetupPlugin();
    if (config->setupPlugin) {
        config->setupPlugin();
    }
}

StatusTemplateCallback PluginConfiguration::getStatusTemplate() const {
    return config->statusTemplate;
}

ConfigureFormCallback PluginConfiguration::getConfigureForm() const {
    return config->configureForm;
}

ReconfigurePluginCallback PluginConfiguration::getReconfigurePlugin() const {
    return config->reconfigurePlugin;
}

void PluginConfiguration::callReconfigurePlugin(PGM_P source) {
    // auto callback = getReconfigurePlugin();
    if (config->reconfigurePlugin) {
        config->reconfigurePlugin(source);
        for(auto &plugin: plugins) {
            if (plugin.getConfigureForm() && plugin.isDependency(getPluginNamePSTR())) {
                plugin.callReconfigurePlugin(getPluginNamePSTR());
            }
        }
    }
}

void PluginConfiguration::callReconfigureSystem(PGM_P name) {
    for(auto &plugin: plugins) {
        if (plugin.getConfigureForm() && plugin.isDependency(name)) {
            plugin.callReconfigurePlugin(name);
        }
    }
}

String PluginConfiguration::getReconfigurePluginDependecies() const {
    if (!config->configureForm || !config->reconfigurePluginDependencies) {
        return _sharedEmptyString;
    }
    return FPSTR(config->reconfigurePluginDependencies);
}

PGM_P PluginConfiguration::getReconfigurePluginDependeciesPSTR() const {
    return config->reconfigurePluginDependencies;
}

bool PluginConfiguration::isDependency(PGM_P pluginName) const {
    if (!config->reconfigurePluginDependencies) {
        return false;
    }
    return stringlist_find_P_P(config->reconfigurePluginDependencies, pluginName) != -1;
}

PrepareDeepSleepCallback PluginConfiguration::getPrepareDeepSleep() const {
    return config->prepareDeepSleep;
}

void PluginConfiguration::callPrepareDeepSleep(uint32_t time, RFMode mode) {
    //auto callback = getPrepareDeepSleep();
    if (config->prepareDeepSleep) {
        config->prepareDeepSleep(time, mode);
    }
}

#if AT_MODE_SUPPORTED

AtModeCommandHandlerCallback PluginConfiguration::getAtModeCommandHandler() const {
    return config->atModeCommandHandler;
    //return reinterpret_cast<AtModeCommandHandlerCallback>(pgm_read_ptr(&config->atModeCommandHandler));
}

bool PluginConfiguration::callAtModeCommandHandler(Stream & serial, const String & command, int8_t argc, char ** argv) {
    // auto callback = getAtModeCommandHandler();
    if (config->atModeCommandHandler) {
        return config->atModeCommandHandler(serial, command, argc, argv);
    }
    return false;
}

#endif

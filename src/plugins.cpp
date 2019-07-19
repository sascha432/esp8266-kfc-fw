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

#define PLUGIN_RTC_MEM_MAX_ID       RTCMemoryManager::__maxId

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

PluginsVector plugins;

void register_plugin(PGM_PLUGIN_CONFIG_P config) {
    PluginConfiguration plugin(config);
    _debug_printf_P(PSTR("register_plugin %s priority %d\n"), plugin.getPluginName().c_str(), plugin.getSetupPriority());
    plugins.push_back(plugin);
}

PROGMEM_PLUGIN_CONFIG_DECL(rd);
PROGMEM_PLUGIN_CONFIG_DECL(cfg);
PROGMEM_PLUGIN_CONFIG_DECL(mdns);
PROGMEM_PLUGIN_CONFIG_DECL(esp8266atc);
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
PROGMEM_PLUGIN_CONFIG_DECL(filemgr);
PROGMEM_PLUGIN_CONFIG_DECL(i2c_scan);

void register_all_plugins() {

    register_plugin(SPGM_PLUGIN_CONFIG_P(rd));
    register_plugin(SPGM_PLUGIN_CONFIG_P(cfg));

#if MDNS_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(mdns));
#endif
#if ESP8266_AT_MODE_SUPPORT
    register_plugin(SPGM_PLUGIN_CONFIG_P(esp8266atc));
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
#if FILE_MANAGER
    register_plugin(SPGM_PLUGIN_CONFIG_P(filemgr));
#endif
#if I2CSCANNER_PLUGIN
    register_plugin(SPGM_PLUGIN_CONFIG_P(i2c_scan));
#endif
}

void dump_plugin_list(Print &output) {

#if DEBUG

    constexpr const TableDumper::Length_t __defaultLength = 6;

    TableDumper dumper(Serial);
    dumper.setStrBoolValues(F("yes"), F("no"));
    dumper.setStrNullValue(F("<none>"));

    dumper.startTable();
    dumper.addColumn(F("Name"), __defaultLength);
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
        if (plugin.getRtcMemoryId() > PLUGIN_RTC_MEM_MAX_ID) {
            _debug_printf_P(PSTR("WARNING! Plugin %s is using an invalid id %d\n"), plugin.getPluginName().c_str(), plugin.getRtcMemoryId());
        }
        if (plugin.getRtcMemoryId()) {
            uint8_t j = 0;
            for(auto &plugin2: plugins) {
                if (i != j && plugin2.getRtcMemoryId() && plugin.getRtcMemoryId() == plugin2.getRtcMemoryId()) {
                    _debug_printf_P(PSTR("WARNING! Plugin %s and %s use the same id (%d) to identify the RTC memory data\n"), plugin.getPluginName().c_str(), plugin2.getPluginName().c_str(), plugin.getRtcMemoryId());
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
        _debug_printf_P(PSTR("setup_plugins(%d) %s priority %d run setup %d\n"), mode, plugin.getPluginName().c_str(), plugin.getSetupPriority(), runSetup);
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
    auto name = reinterpret_cast<PGM_P>(pgm_read_ptr(&config->pluginName));
    return strcmp_P(string.c_str(), name) == 0;
}

bool PluginConfiguration::pluginNameEquals(const __FlashStringHelper * string) const {
    return strcmp_P_P(reinterpret_cast<PGM_P>(pgm_read_ptr(&config->pluginName)), reinterpret_cast<PGM_P>(string)) == 0;
}

String PluginConfiguration::getPluginName() const {
    PGM_P name = reinterpret_cast<PGM_P>(pgm_read_ptr(&config->pluginName));
    return FPSTR(name);
}

PGM_P PluginConfiguration::getPluginNamePSTR() const {
    return reinterpret_cast<PGM_P>(pgm_read_ptr(&config->pluginName));
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
    return reinterpret_cast<SetupPluginCallback>(pgm_read_ptr(&config->setupPlugin));
}

void PluginConfiguration::callSetupPlugin() {
    auto callback = getSetupPlugin();
    if (callback) {
        callback();
    }
}

StatusTemplateCallback PluginConfiguration::getStatusTemplate() const {
    return reinterpret_cast<StatusTemplateCallback>(pgm_read_ptr(&config->statusTemplate));
}

ConfigureFormCallback PluginConfiguration::getConfigureForm() const {
    return reinterpret_cast<ConfigureFormCallback>(pgm_read_ptr(&config->configureForm));
}

ReconfigurePluginCallback PluginConfiguration::getReconfigurePlugin() const {
    return reinterpret_cast<ReconfigurePluginCallback>(pgm_read_ptr(&config->reconfigurePlugin));
}

void PluginConfiguration::callReconfigurePlugin() {
    auto callback = getReconfigurePlugin();
    if (callback) {
        callback();
        for(auto &plugin: plugins) {
            if (plugin.getConfigureForm() && plugin.isDependency(getPluginNamePSTR())) {
                plugin.callReconfigurePlugin();
            }
        }
    }
}

String PluginConfiguration::getReconfigurePluginDependecies() const {
    if (!getConfigureForm()) {
        return _sharedEmptyString;
    }
    return FPSTR(pgm_read_ptr(&config->reconfigurePluginDependencies));
}

PGM_P PluginConfiguration::getReconfigurePluginDependeciesPSTR() const {
    return reinterpret_cast<PGM_P>(pgm_read_ptr(&config->reconfigurePluginDependencies));
}

bool PluginConfiguration::isDependency(PGM_P pluginName) const {
    return stringlist_find_P_P(reinterpret_cast<PGM_P>(pgm_read_ptr(&config->reconfigurePluginDependencies)), pluginName) != -1;
}

PrepareDeepSleepCallback PluginConfiguration::getPrepareDeepSleep() const {
    return reinterpret_cast<PrepareDeepSleepCallback>(pgm_read_ptr(&config->prepareDeepSleep));
}

void PluginConfiguration::callPrepareDeepSleep(uint32_t time, RFMode mode) {
    auto callback = getPrepareDeepSleep();
    if (callback) {
        callback(time, mode);
    }
}

#if AT_MODE_SUPPORTED

AtModeCommandHandlerCallback PluginConfiguration::getAtModeCommandHandler() const {
    return reinterpret_cast<AtModeCommandHandlerCallback>(pgm_read_ptr(&config->atModeCommandHandler));
}

bool PluginConfiguration::callAtModeCommandHandler(Stream & serial, const String & command, int8_t argc, char ** argv) {
    auto callback = getAtModeCommandHandler();
    if (callback) {
        return callback(serial, command, argc, argv);
    }
    return false;
}

#endif

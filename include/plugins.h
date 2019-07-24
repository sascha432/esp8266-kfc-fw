/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>

#ifndef DEBUG_PLUGINS
#define DEBUG_PLUGINS               1
#endif

#define PLUGIN_PRIO_RESET_DETECTOR  -127
#define PLUGIN_PRIO_CONFIG          -126
#define PLUGIN_PRIO_MDNS            -90
#define PLUGIN_PRIO_SYSLOG          -80
#define PLUGIN_PRIO_NTP             -70
#define PLUGIN_PRIO_HTTP            -60
#define PLUGIN_MAX_PRIORITY         0           // highest prio, -127 to -1 is reserved for the system
#define PLUGIN_DEFAULT_PRIORITY     64
#define PLUGIN_MIN_PRIORITY         127         // lowest

#define AT_MODE_QUERY_COMMAND       -1

typedef enum {
    PLUGIN_SETUP_DEFAULT = 0,                   // normal boot
    PLUGIN_SETUP_SAFE_MODE,                     // safe mode active
    PLUGIN_SETUP_AUTO_WAKE_UP,                  // wake up from deep sleep
    PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP,          // called after a delay to initialize services that have been skipped during wake up
} PluginSetupMode_t;


class AsyncWebServerRequest;
class Form;

typedef void(* SetupPluginCallback)();
typedef const String(* StatusTemplateCallback)();
typedef void(* ConfigureFormCallback)(AsyncWebServerRequest *request, Form &form);
typedef void(* ReconfigurePluginCallback)();
typedef void(* PrepareDeepSleepCallback)(uint32_t timeout, RFMode mode);
#if AT_MODE_SUPPORTED
typedef bool(* AtModeCommandHandlerCallback)(Stream &serial, const String &command, int8_t argc, char **argv);
#endif


// pretty nasty but saves a lot RAM

#if AT_MODE_SUPPORTED
#define PLUGIN_HAS_ATMODE_COMMAND_HANDLER(...)          __VA_ARGS__
#else
#define PLUGIN_HAS_ATMODE_COMMAND_HANDLER(...)
#endif

#define PROGMEM_PLUGIN_CONFIG_DECL(pluginName)          extern const PluginConfig_t __shared_progmem_plugin_config_##pluginName PROGMEM;
#define PGM_PLUGIN_CONFIG_P                             const PluginConfig_t *
#define SPGM_PLUGIN_CONFIG_P(pluginName)                &__shared_progmem_plugin_config_##pluginName

// PROGMEM_PLUGIN_CONFIG_DEF(NAME, ...) also declares a shared string which can be declared with
// PROGMEM_STRING_DECL(plugin_config_name_<NAME>) ... and used with SPGM(plugin_config_name_<NAME>)

#define PROGMEM_PLUGIN_CONFIG_DEF(pluginName, setupPriority, allowSafeMode, autoSetupAfterDeepSleep, rtcMemoryId, \
        setupPlugin, statusTemplate, configureForm, reconfigurePlugin, reconfigurePluginDependencies, prepareDeepSleep, atModeCommandHandler) \
    extern const char _shared_progmem_string_plugin_config_name_##pluginName[] PROGMEM; \
    const char _shared_progmem_string_plugin_config_name_##pluginName[] PROGMEM = { _STRINGIFY(pluginName) }; \
    PROGMEM_PLUGIN_CONFIG_DECL(pluginName); \
    const PluginConfig_t __shared_progmem_plugin_config_##pluginName PROGMEM = { \
        _shared_progmem_string_plugin_config_name_##pluginName, \
        setupPriority, \
        allowSafeMode, \
        autoSetupAfterDeepSleep, \
        rtcMemoryId, \
        setupPlugin, \
        statusTemplate, \
        configureForm, \
        reconfigurePlugin, \
        reconfigurePluginDependencies, \
        prepareDeepSleep, \
        PLUGIN_HAS_ATMODE_COMMAND_HANDLER(atModeCommandHandler,) \
    };

// Example

//register_plugin(SPGM_PLUGIN_CONFIG_P(NAME));
//
//PROGMEM_PLUGIN_CONFIG_DEF(NAME)
//
//PROGMEM_PLUGIN_CONFIG_DEF(
///* pluginName               */ NAME,
///* setupPriority            */ 15,
///* allowSafeMode            */ false,
///* autoSetupWakeUp          */ false,
///* rtcMemoryId              */ 0,
///* setupPlugin              */ nullptr,
///* statusTemplate           */ nullptr,
///* configureForm            */ nullptr,
///* reconfigurePlugin        */ nullptr,
///* reconfigure Dependencies */ "name,another",
///* prepareDeepSleep         */ nullptr,
///* atModeCommandHandler     */ nullptr
//);


typedef struct {
    PGM_P pluginName;
    int8_t setupPriority;
    bool allowSafeMode;
    bool autoSetupAfterDeepSleep;
    uint8_t rtcMemoryId;
    SetupPluginCallback setupPlugin;
    StatusTemplateCallback statusTemplate;
    ConfigureFormCallback configureForm;
    ReconfigurePluginCallback reconfigurePlugin;
    PGM_P reconfigurePluginDependencies;                // call all reconfigurePlugin callbacks of the plugins listed in here after the actual reconfigurePlugin callback
    PrepareDeepSleepCallback prepareDeepSleep;
#if AT_MODE_SUPPORTED
    AtModeCommandHandlerCallback atModeCommandHandler;
#endif
} PluginConfig_t;

class PluginConfiguration {
public:
    PluginConfiguration();
    PluginConfiguration(PGM_PLUGIN_CONFIG_P configPtr);

    bool pluginNameEquals(const String &string) const;
    bool pluginNameEquals(const __FlashStringHelper *string) const;
    String getPluginName() const;
    PGM_P getPluginNamePSTR() const;

    int8_t getSetupPriority() const;
    bool isAllowSafeMode() const;
    bool isAutoSetupAfterDeepSleep() const;
    uint8_t getRtcMemoryId() const;

    SetupPluginCallback getSetupPlugin() const;
    void callSetupPlugin();
    StatusTemplateCallback getStatusTemplate() const;
    ConfigureFormCallback getConfigureForm() const;
    ReconfigurePluginCallback getReconfigurePlugin() const;
    void callReconfigurePlugin();
    bool isDependency(PGM_P pluginName) const;
    String getReconfigurePluginDependecies() const;
    PGM_P getReconfigurePluginDependeciesPSTR() const;
    PrepareDeepSleepCallback getPrepareDeepSleep() const;
    void callPrepareDeepSleep(uint32_t time, RFMode mode);
#if AT_MODE_SUPPORTED
    AtModeCommandHandlerCallback getAtModeCommandHandler() const;
    bool callAtModeCommandHandler(Stream &serial, const String &command, int8_t argc, char **argv);
#endif

private:
    PGM_PLUGIN_CONFIG_P config;
};

typedef std::vector<PluginConfiguration> PluginsVector;

extern PluginsVector plugins;

// register all available plug-ins
void register_all_plugins();

// dump list of plug-ins and some details
void dump_plugin_list(Print &output);

// register plug in
void register_plugin(PGM_PLUGIN_CONFIG_P config);

// prepare plug-ins, must be called once before setup_plugins
void prepare_plugins();

// setup all plug-ins
void setup_plugins(PluginSetupMode_t mode);

// get plug in configuration by name
PluginConfiguration *get_plugin_by_name(PGM_P name);

// get plug in configuration by name
PluginConfiguration *get_plugin_by_name(const String &name);

PluginConfiguration *get_plugin_by_rtc_memory_id(uint8_t id);

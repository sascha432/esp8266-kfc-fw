/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>

#ifndef DEBUG_PLUGINS
#define DEBUG_PLUGINS               1
#endif

#include <push_pack.h>

#define PLUGIN_MAX_PRIORITY         0
#define PLUGIN_DEFAULT_PRIORITY     10
#define PLUGIN_MIN_PRIORITY         15

#define ATMODE_QUERY_COMMAND        -1

typedef enum {
    PLUGIN_SETUP_DEFAULT = 0,                   // normal boot
    PLUGIN_SETUP_SAFE_MODE,                     // safe mode active
    PLUGIN_SETUP_AUTO_WAKE_UP,                  // wake up from deep sleep
    PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP,          // called after a delay to initialize services that have been skipped during wake up
} PluginSetupMode_t;

// RTC memory structure
#define PLUGIN_RTC_MEM_SIZE             512
#define PLUGIN_RTC_MEM_BLK_SIZE         4
#define PLUGIN_RTC_MEM_DATA_BLK_SIZE    __plugin_calculate_header_block_size()
#define PLUGIN_RTC_MEM_DATA_SIZE        (PLUGIN_RTC_MEM_DATA_BLK_SIZE * PLUGIN_RTC_MEM_BLK_SIZE)
#define PLUGIN_RTC_MEM_ADDRESS          ((PLUGIN_RTC_MEM_SIZE - PLUGIN_RTC_MEM_DATA_SIZE) / PLUGIN_RTC_MEM_BLK_SIZE)
// internal structure
#define PLUGIN_RTC_MEM_OFFSET           (PLUGIN_RTC_MEM_ADDRESS * PLUGIN_RTC_MEM_BLK_SIZE)
#define PLUGIN_RTC_MEM_MAX_ID           32
#define PLUGIN_RTC_MEM_MAX_SIZE         64

typedef struct __attribute__packed__ {
    uint16_t length;
    uint16_t crc;
} rtc_memory_header;

typedef struct __attribute__packed__ {
    uint8_t mem_id;
    uint8_t length;
} rtc_memory_record_header;

constexpr int __plugin_calculate_header_block_size() {
    return ((sizeof(rtc_memory_header) & (PLUGIN_RTC_MEM_BLK_SIZE - 1) ? ((sizeof(rtc_memory_header) + 4) & ~(PLUGIN_RTC_MEM_BLK_SIZE - 1)) : sizeof(rtc_memory_header)) / PLUGIN_RTC_MEM_BLK_SIZE);
}


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


#if AT_MODE_SUPPORTED
#define PLUGIN_HAS_ATMODE_COMMAND_HANDLER(...) __VA_ARGS__
#else
#define PLUGIN_HAS_ATMODE_COMMAND_HANDLER(...)
#endif

// pretty nasty but saves a lot RAM

#define PROGMEM_PLUGIN_CONFIG_DECL(pluginName)      extern const PluginProgmemConfig_t __shared_progmem_plugin_config_##pluginName PROGMEM;
#define PROGMEM_PLUGIN_CONFIG_PGM(pluginName)       __shared_progmem_plugin_config_##pluginName

#define PROGMEM_PLUGIN_CONFIG_DEF(pluginName, setupPriority, allowSafeMode, autoSetupDeepSleep, rtcMemoryId, setupPlugin, statusTemplate, configureForm, reconfigurePlugin, prepareDeepSleep, atModeCommandHandler) \
    const char _shared_progmem_string___plugin_config_name_##pluginName[] PROGMEM = { CONFIG_STRINGIFY(pluginName) }; \
    const PluginProgmemConfig_t __shared_progmem_plugin_config_##pluginName PROGMEM = { \
        SPGM(__plugin_config_name_##pluginName), \
        setupPriority, \
        allowSafeMode, \
        autoSetupDeepSleep, \
        rtcMemoryId, \
        setupPlugin, \
        statusTemplate, \
        configureForm, \
        reconfigurePlugin, \
        prepareDeepSleep, \
        PLUGIN_HAS_ATMODE_COMMAND_HANDLER(atModeCommandHandler,) \
    };


typedef struct {
    PGM_P pluginName;
    uint8_t setupPriority;
    bool allowSafeMode;
    bool autoSetupDeepSleep;
    uint8_t rtcMemoryId;
    SetupPluginCallback setupPlugin;
    StatusTemplateCallback statusTemplate;
    ConfigureFormCallback configureForm;
    ReconfigurePluginCallback reconfigurePlugin;
    PrepareDeepSleepCallback prepareDeepSleep;
#if AT_MODE_SUPPORTED
    AtModeCommandHandlerCallback atModeCommandHandler;
#endif
} PluginProgmemConfig_t;

// TODO move to PROGMEM

typedef struct __attribute__packed__ {

    //const PluginProgmemConfig_t *progmemPtr;

    // unique name of the plugin (currently not checked if unique)
    PGM_P pluginName;

    // function to setup the plugin during boot
    SetupPluginCallback setupPlugin;
    // return HTML code for the status page. the template variable is %<PLUGIN NAME>_STATUS%
    StatusTemplateCallback statusTemplate;
    // function to create a form for the configuration. request is either null or contains the POST data from the http request. the filename for the web interface is /<plugin name>.html
    ConfigureFormCallback configureForm;
    // this function gets called when the configuration has changed and the plugin needs to get reconfigured
    ReconfigurePluginCallback reconfigurePlugin;
#if AT_MODE_SUPPORTED
    // handler for AT commands, returns true if the command was handled. an empty command.length == 0 can display the help/usage. argc == ATMODE_COMMAND_QUERY means "AT+CMD?" was sent. The command itself is "CMD" and cannot have any arguments passed
    AtModeCommandHandlerCallback atModeCommandHandler;
#endif
    // prepare device for deep sleep
    PrepareDeepSleepCallback prepareDeepSleep;

    // unique id of the data stored in the RTC memory
    // 0 = not used, 1 = reset detector, 2 = WiFi deep sleep quick connect, 3 NTP client, ... PLUGIN_RTC_MEM_MAX_ID
    uint8_t rtcMemoryId;
    // the order of setup functions being called
    uint8_t setupPriority: 4;
    // initialize plugin in safe mode
    uint8_t allowSafeMode: 1;
    // automatically initialize plugin after wake up
    uint8_t autoSetupDeepSleep: 1;
} Plugin_t;

typedef std::vector<Plugin_t> PluginsVector;

extern PluginsVector plugins;

// register all available plugins
void register_all_plugins();

void dump_plugin_list(Print &output);

// initialize plugin structure and set name
void init_plugin(PGM_P name, Plugin_t &plugin, bool allowSafeMode, bool autoSetupDeepSleep, uint8_t priority);

// register plugin
void register_plugin(Plugin_t &plugin);

// setup all plugins
void setup_plugins(PluginSetupMode_t mode);

// get plugin configuration by name
Plugin_t *get_plugin_by_name(PGM_P name);

// get plugin configuration by name
Plugin_t *get_plugin_by_name(const String &name);

#if DEBUG
void plugin_debug_dump_rtc_memory(Print &output);
#endif

// read RTC memory
bool plugin_read_rtc_memory(uint8_t id, void *, uint8_t maxSize);

// write RTC memory
bool plugin_write_rtc_memory(uint8_t id, void *, uint8_t maxSize);

#include <pop_pack.h>

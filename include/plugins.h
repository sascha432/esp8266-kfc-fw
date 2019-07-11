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
#define PLUGIN_DEFAULT_PRIORITY     127
#define PLUGIN_MIN_PRIORITY         255

#define ATMODE_QUERY_COMMAND        -1

typedef enum {
    PLUGIN_SETUP_DEFAULT = 0,                   // normal boot
    PLUGIN_SETUP_SAFE_MODE,                     // safe mode active
    PLUGIN_SETUP_AUTO_WAKE_UP,                  // wake up from deep sleep
    PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP,          // called after a delay to initialize services that have been skipped during wake up
} PluginSetupMode_t;

typedef struct __attribute__packed__ {
    uint16_t length;
    uint16_t crc;
} rtc_memory_header;

typedef struct __attribute__packed__ {
    uint8_t mem_id;
    uint8_t length;
} rtc_memory_entry_header;

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
#define PLUGIN_HAS_ATMODE_COMMAND_HANDLER(...) __VA_ARGS__
#else
#define PLUGIN_HAS_ATMODE_COMMAND_HANDLER(...)
#endif

#define PROGMEM_PLUGIN_CONFIG_DECL(pluginName)          extern const PluginConfig_t __shared_progmem_plugin_config_##pluginName PROGMEM;
#define PGM_PLUGIN_CONFIG_P                             const PluginConfig_t *
#define SPGM_PLUGIN_CONFIG_P(pluginName)                &__shared_progmem_plugin_config_##pluginName

#define PROGMEM_PLUGIN_CONFIG_DEF(pluginName, setupPriority, allowSafeMode, autoSetupAfterDeepSleep, rtcMemoryId, setupPlugin, statusTemplate, configureForm, reconfigurePlugin, prepareDeepSleep, atModeCommandHandler) \
    PROGMEM_PLUGIN_CONFIG_DECL(pluginName); \
    const char _shared_progmem_string_plugin_config_name__##pluginName[] PROGMEM = { _STRINGIFY(pluginName) }; \
    const PluginConfig_t __shared_progmem_plugin_config_##pluginName PROGMEM = { \
        _shared_progmem_string_plugin_config_name__##pluginName, \
        setupPriority, \
        allowSafeMode, \
        autoSetupAfterDeepSleep, \
        rtcMemoryId, \
        setupPlugin, \
        statusTemplate, \
        configureForm, \
        reconfigurePlugin, \
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
///* prepareDeepSleep         */ nullptr,
///* atModeCommandHandler     */ nullptr
//);


typedef struct {
    PGM_P pluginName;
    uint8_t setupPriority;
    bool allowSafeMode;
    bool autoSetupAfterDeepSleep;
    uint8_t rtcMemoryId;
    SetupPluginCallback setupPlugin;
    StatusTemplateCallback statusTemplate;
    ConfigureFormCallback configureForm;
    ReconfigurePluginCallback reconfigurePlugin;
    PrepareDeepSleepCallback prepareDeepSleep;
#if AT_MODE_SUPPORTED
    AtModeCommandHandlerCallback atModeCommandHandler;
#endif
} PluginConfig_t;

class Plugin_t {
public:
    PGM_PLUGIN_CONFIG_P config;

    bool pluginNameEquals(const String &string) const {
        auto name = reinterpret_cast<PGM_P>(pgm_read_ptr(&config->pluginName));
        return strcmp_P(string.c_str(), name) == 0;
    }
    bool pluginNameEquals(const __FlashStringHelper *string) const {
        auto namePtr = reinterpret_cast<PGM_P>(pgm_read_ptr(&config->pluginName));
        auto stringPtr = reinterpret_cast<PGM_P>(string);
        uint8_t ch;
        do {
            ch = pgm_read_byte(namePtr);
            if (ch != pgm_read_byte(stringPtr)) {
                return false;
            }
            namePtr++;
            stringPtr++;
        } while(ch);
        return true;
    }
    String getPluginName() const {
        PGM_P name = reinterpret_cast<PGM_P>(pgm_read_ptr(&config->pluginName));
        return FPSTR(name);
    }
    PGM_P getPluginName_PGM_P() {
        return reinterpret_cast<PGM_P>(pgm_read_ptr(&config->pluginName));
    }

    uint8_t getSetupPriority() const {
        return pgm_read_byte(&config->setupPriority);
    }
    bool isAllowSafeMode() const {
        return pgm_read_byte(&config->allowSafeMode);
    }
    bool isAutoSetupAfterDeepSleep() const {
        return pgm_read_byte(&config->autoSetupAfterDeepSleep);
    }
    uint8_t getRtcMemoryId() const {
        return pgm_read_byte(&config->rtcMemoryId);
    }
    SetupPluginCallback getSetupPlugin() const {
        return reinterpret_cast<SetupPluginCallback>(pgm_read_ptr(&config->setupPlugin));
    }
    void callSetupPlugin() {
        auto callback = getSetupPlugin();
        if (callback) {
            callback();
        }
    }
    StatusTemplateCallback getStatusTemplate() const {
        return reinterpret_cast<StatusTemplateCallback>(pgm_read_ptr(&config->statusTemplate));
    }
    ConfigureFormCallback getConfigureForm() const {
        return reinterpret_cast<ConfigureFormCallback>(pgm_read_ptr(&config->configureForm));
    }
    ReconfigurePluginCallback getReconfigurePlugin() const {
        return reinterpret_cast<ReconfigurePluginCallback>(pgm_read_ptr(&config->reconfigurePlugin));
    }
    void callReconfigurePlugin() {
        auto callback = getReconfigurePlugin();
        if (callback) {
            callback();
        }
    }
    PrepareDeepSleepCallback getPrepareDeepSleep() const {
        return reinterpret_cast<PrepareDeepSleepCallback>(pgm_read_ptr(&config->prepareDeepSleep));
    }
    void callPrepareDeepSleep(uint32_t time, RFMode mode) {
        auto callback = getPrepareDeepSleep();
        if (callback) {
            callback(time, mode);
        }
    }
#if AT_MODE_SUPPORTED
    AtModeCommandHandlerCallback getAtModeCommandHandler() const {
        return reinterpret_cast<AtModeCommandHandlerCallback>(pgm_read_ptr(&config->atModeCommandHandler));
    }
    bool callAtModeCommandHandler(Stream &serial, const String &command, int8_t argc, char **argv) {
        auto callback = getAtModeCommandHandler();
        if (callback) {
            return callback(serial, command, argc, argv);
        }
        return false;
    }
#endif
};

typedef std::vector<Plugin_t> PluginsVector;

extern PluginsVector plugins;

// register all available plugins
void register_all_plugins();

// dump list of plugins and some details
void dump_plugin_list(Print &output);

// register plugin
void register_plugin(PGM_PLUGIN_CONFIG_P config);

// setup all plugins
void setup_plugins(PluginSetupMode_t mode);

// get plugin configuration by name
Plugin_t *get_plugin_by_name(PGM_P name);

// get plugin configuration by name
Plugin_t *get_plugin_by_name(const String &name);

// RTC Memory manager

// RTC memory structure
#define PLUGIN_RTC_MEM_SIZE             512
#define PLUGIN_RTC_MEM_BLK_SIZE         4
#define PLUGIN_RTC_MEM_DATA_BLK_SIZE    ((sizeof(rtc_memory_header) & (PLUGIN_RTC_MEM_BLK_SIZE - 1) ? ((sizeof(rtc_memory_header) + 4) & ~(PLUGIN_RTC_MEM_BLK_SIZE - 1)) : (sizeof(rtc_memory_header))) / PLUGIN_RTC_MEM_BLK_SIZE)
#define PLUGIN_RTC_MEM_DATA_SIZE        (PLUGIN_RTC_MEM_DATA_BLK_SIZE * PLUGIN_RTC_MEM_BLK_SIZE)
#define PLUGIN_RTC_MEM_ADDRESS          ((PLUGIN_RTC_MEM_SIZE - PLUGIN_RTC_MEM_DATA_SIZE) / PLUGIN_RTC_MEM_BLK_SIZE)
// internal structure
#define PLUGIN_RTC_MEM_OFFSET           (PLUGIN_RTC_MEM_ADDRESS * PLUGIN_RTC_MEM_BLK_SIZE)
#define PLUGIN_RTC_MEM_MAX_ID           32
#define PLUGIN_RTC_MEM_MAX_SIZE         64

constexpr bool __plugin_is_rtc_memory_header_aligned() {
    return (sizeof(rtc_memory_header) & (PLUGIN_RTC_MEM_BLK_SIZE - 1)) == 0;
}

#if DEBUG
void plugin_debug_dump_rtc_memory(Print &output);
#endif

// read RTC memory
bool plugin_read_rtc_memory(uint8_t id, void *, uint8_t maxSize);

// write RTC memory
bool plugin_write_rtc_memory(uint8_t id, void *, uint8_t maxSize);

// clear RTC memory by overwriting the header only
bool plugin_clear_rtc_memory();

#include <pop_pack.h>

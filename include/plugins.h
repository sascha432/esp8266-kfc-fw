/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>

#define PLUGIN_MAX_PRIORITY         0
#define PLUGIN_DEFAULT_PRIORITY     127
#define PLUGIN_MIN_PRIORITY         255

#define ATMODE_QUERY_COMMAND        -1

typedef enum {
    PLUGIN_SETUP_DEFAULT = 0,                   // normal boot
    PLUGIN_SETUP_SAFE_MODE,                     // safe mode active
    PLUGIN_SETUP_AUTO_WAKE_UP,                  // wake up from deep sleep
    PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP,          // called after a detlay to initialize services that have been skipped during wake up
} PluginSetupMode_t;

class AsyncWebServerRequest;
class Form;

typedef void(* SetupPluginCallback)();
typedef const String(* StatusTemplateCallback)();
typedef void(* ConfigureFormCallback)(AsyncWebServerRequest *request, Form &form);
typedef void(* ReconfigurePluginCallback)();
typedef void(* PrepareDeepSleepCallback)(uint32_t timeout, RFMode mode);
typedef void *(* GetPluginConfigurationCallback)(uint16_t &length);
#if AT_MODE_SUPPORTED
typedef bool(* AtModeCommandHandlerCallback)(Stream &serial, const String &command, int8_t argc, char **argv);
#endif

// TODO add functionality that offers using RTC memory for storing additional data for wake-up from deep sleep like
// last DHCP configuration, which is currently stored in the EEPROM, but some data needs to be written to frequently
// to update the EEPROM every time

struct Plugin_t {
    // unique name of the plugin (currently not checked if unique)
    String pluginName;
    // the order of setup functions being called
    uint8_t setupPriority;
    // function to setup the plugin during boot
    SetupPluginCallback setupPlugin;
    // return html code for the status page. the template variable is %<PLUGIN NAME>_STATUS%
    StatusTemplateCallback statusTemplate;
    // function to create a form for the configuration. request is either null or contains the POST data from the http request. the filename for the web interface is /<plugin name>.html
    ConfigureFormCallback configureForm;
    // this function gets called when the configuration has changed and the plugin needs to get reconfigured
    ReconfigurePluginCallback reconfigurePlugin;
    // return pointer to the configuration
    GetPluginConfigurationCallback getPluginConfiguration;
#if AT_MODE_SUPPORTED
    // handler for AT commands, returns true if the command was handled. an empty command.length == 0 can display the help/usage. argc == ATMODE_COMMAND_QUERY means "AT+CMD?" was sent. The command itself is "CMD" and cannot have any arguments passed
    AtModeCommandHandlerCallback atModeCommandHandler;
#endif
    // prepare device for deep sleep
    PrepareDeepSleepCallback prepareDeepSleep;
    // initialize plugin in safe mode
    uint8_t allowSafeMode: 1;
    // automatically initialize plugin after wake up
    uint8_t autoSetupDeepSleep: 1;
};

typedef std::vector<Plugin_t> PluginsVector;

extern PluginsVector plugins;

// initialize plugin structure and set name
void init_plugin(const String &name, Plugin_t &plugin, bool allowSafeMode, bool autoSetupDeepSleep, uint8_t priority);

// register plugin
void register_plugin(Plugin_t &plugin);

// setup all plugins
void setup_plugins(PluginSetupMode_t mode);

// get plugin configuration by name
Plugin_t *get_plugin_by_name(const String &name);

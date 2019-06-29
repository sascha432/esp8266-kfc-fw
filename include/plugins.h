/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>

#define PLUGIN_MAX_PRIORITY         0
#define PLUGIN_DEFAULT_PRIORITY     127
#define PLUGIN_MIN_PRIORITY         255

class AsyncWebServerRequest;
class Form;

typedef void(* SetupPluginCallback)(bool isSafeMode);
typedef const String(* StatusTemplateCallback)();
typedef void(* ConfigureFormCallback)(AsyncWebServerRequest *request, Form &form);
typedef void(* ReconfigurePluginCallback)();
typedef void *(* GetPluginConfigurationCallback)(uint16_t &length);
#if AT_MODE_SUPPORTED
typedef bool(* AtModeCommandHandlerCallback)(Stream &serial, const String &command, uint8_t argc, char **argv);
#endif

struct Plugin_t {
    // unique name of the plugin (currently not checked if unique)
    String pluginName;
    // the order of setup functions being called
    uint8_t setupPriority;
    // function to setup the plugin during boot
    SetupPluginCallback setupPlugin;
    // return html code for the status page. the template variable is %<PLUGINNAME>_STATUS%
    StatusTemplateCallback statusTemplate;
    // function to create a form for the configuration. request is either null or contains the POST data from the http request
    ConfigureFormCallback configureForm;
    // this function gets called when the configuration has changed and the plugin needs to get reconfigured
    ReconfigurePluginCallback reconfigurePlugin;
    // return pointer to the configuration
    GetPluginConfigurationCallback getPluginConfiguration;
#if AT_MODE_SUPPORTED
    // handler for AT commands, returns true if the command was handled. the command "?" can display the usage
    AtModeCommandHandlerCallback atModeCommandHandler;
#endif
};

typedef std::vector<Plugin_t> PluginsVector;

extern PluginsVector plugins;

// initialize plugin structure and set name
void init_plugin(const String &name, Plugin_t &plugin, uint8_t priority = PLUGIN_DEFAULT_PRIORITY);

// register plugin
bool register_plugin(Plugin_t &plugin);

// setup all plugins
void setup_plugins(bool isSafeMode);

// get plugin configuration by name
Plugin_t *get_plugin_by_name(const String &name);

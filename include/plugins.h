/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include <Form.h>

typedef void(* SetupPluginCallback)();
typedef const String(* StatusTemplateCallback)();
typedef Form *(* ConfigureFormCallback)(FormData *request);
typedef void(* ReconfigurePluginCallback)();
typedef void *(* GetPluginConfigurationCallback)(uint16_t &length);

struct Plugin_t {
    // unique name of the plugin (currently not checked if unique)
    String pluginName;
    // function to setup the plugin during boot
    SetupPluginCallback setupPlugin;
    // function to return html code for the status page. the template variable is %<PLUGINNAME>_STATUS%
    StatusTemplateCallback statusTemplate;
    // function to create a form for the configuration
    ConfigureFormCallback configureForm;
    // this function gets called when the configuration has changed and the plugin needs to get reconfigured
    ReconfigurePluginCallback reconfigurePlugin;
    // return structure of the configuration
    GetPluginConfigurationCallback getPluginConfiguration;
};

typedef std::vector<Plugin_t> PluginsVector;

extern PluginsVector plugins;

// initialize plugin structure and set name
void init_plugin(const String &name, Plugin_t &plugin);

// register plugin
bool register_plugin(Plugin_t &plugin);

// setup all plugins
void setup_plugins();

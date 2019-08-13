/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "PluginComponent.h"

#ifndef DEBUG_PLUGINS
#define DEBUG_PLUGINS               0
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

typedef std::vector<PluginComponent *> PluginsVector;

extern PluginsVector plugins;

// dump list of plug-ins and some details
void dump_plugin_list(Print &output);

// register plug in
void register_plugin(PluginComponent *plugin);

// prepare plug-ins, must be called once before setup_plugins
void prepare_plugins();

// setup all plug-ins
void setup_plugins(PluginComponent::PluginSetupMode_t mode);

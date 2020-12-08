/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "PluginComponent.h"

#ifndef DEBUG_PLUGINS
#define DEBUG_PLUGINS                                 0
#endif

#define PLUGIN_PRIO_RESET_DETECTOR                    -127
#define PLUGIN_PRIO_CONFIG                            -126
#define PLUGIN_PRIO_MDNS                              -90
#define PLUGIN_PRIO_SYSLOG                            -80
#define PLUGIN_PRIO_NTP                               -70
#define PLUGIN_PRIO_HTTP                              -60
#define PLUGIN_MAX_PRIORITY                           0           // highest prio, -127 to -1 is reserved for the system
#define PLUGIN_DEFAULT_PRIORITY                       64
#define PLUGIN_MIN_PRIORITY                           127         // lowest

#if ENABLE_DEEP_SLEEP
#ifndef PLUGIN_DEEP_SLEEP_DELAYED_START_TIME
#define PLUGIN_DEEP_SLEEP_DELAYED_START_TIME          60000
#endif
#endif

typedef std::vector<PluginComponent *> PluginsVector;

extern PluginsVector plugins;

#ifndef ENABLE_BOOT_LOG
#define ENABLE_BOOT_LOG                                 0
#endif

#if ENABLE_BOOT_LOG
void bootlog_printf(PGM_P fmt, ...);
#define BOOTLOG_PRINTF(fmt, ...)                        bootlog_printf(PSTR(fmt), ##__VA_ARGS__)
#else
#define BOOTLOG_PRINTF(fmt, ...)
#endif

// dump list of plug-ins and some details
void dump_plugin_list(Print &output);

// register plug in
#if DEBUG_PLUGINS
#define REGISTER_PLUGIN(plugin, name)                   register_plugin(plugin, name)
void register_plugin(PluginComponent *plugin, const char *name);
#else
#define REGISTER_PLUGIN(plugin, name)                   register_plugin(plugin)
void register_plugin(PluginComponent *plugin);
#endif

// prepare plug-ins, must be called once before setup_plugins
void prepare_plugins();

// setup all plug-ins
void setup_plugins(PluginComponent::SetupModeType mode);

// reset delayed startup time
// millis() > timeout starts the procedure
void set_delayed_startup_time(uint32_t timeout);

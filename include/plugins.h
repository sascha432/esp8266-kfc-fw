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

#if ENABLE_DEEP_SLEEP
#ifndef PLUGIN_DEEP_SLEEP_DELAYED_START_TIME
#define PLUGIN_DEEP_SLEEP_DELAYED_START_TIME          7500
#endif
#endif

namespace PluginComponents {

    class Register {
    public:
        Register() :
            _delayedStartupTime(PLUGIN_DEEP_SLEEP_DELAYED_START_TIME),
            _enableWebUIMenu(false)
        {
        }

        static Register *getInstance();
        static void add(PluginComponent *plugin);
        static void setDelayedStartupTime(uint32_t delayedStartupTime);
        static PluginsVector &getPlugins();

    public:
        void prepare();
        void setup(SetupModeType mode, DependenciesPtr dependencies = nullptr);
        void dumpList(Print &output);

    protected:
        void _add(PluginComponent *plugin);
        void _setDelayedStartupTime(uint32_t delayedStartupTime);
        PluginsVector &_getPlugins();

    protected:
        PluginsVector _plugins;
        uint32_t _delayedStartupTime;
        bool _enableWebUIMenu;
    };

    extern void PluginComponentInitRegisterEx();

    inline __attribute__((__always_inline__))
    void Register::add(PluginComponent *plugin)
    {
        getInstance()->_add(plugin);
    }

    inline __attribute__((__always_inline__))
    PluginsVector &Register::getPlugins()
    {
        return getInstance()->_getPlugins();
    }

    inline __attribute__((__always_inline__))
    void Register::setDelayedStartupTime(uint32_t delayedStartupTime)
    {
        getInstance()->_setDelayedStartupTime(delayedStartupTime);
    }

    inline __attribute__((__always_inline__))
    void Register::_setDelayedStartupTime(uint32_t delayedStartupTime)
    {
        _delayedStartupTime = delayedStartupTime;
    }

    inline __attribute__((__always_inline__))
    PluginsVector &Register::_getPlugins()
    {
        return _plugins;
    }

}

// dump list of plug-ins and some details
// void dump_plugin_list(Print &output);

// register plug in
#if DEBUG_PLUGINS
#define REGISTER_PLUGIN(plugin, name)                   PluginComponents::Register::add(plugin, name)
void register_plugin(PluginComponent *plugin, const char *name);
#else
#define REGISTER_PLUGIN(plugin, name)                   PluginComponents::Register::add(plugin)
#endif

//#define plugins
//PluginComponents::Register::getPlugins()

// prepare plug-ins, must be called once before setup_plugins
// void prepare_plugins();

// setup all plug-ins
// void setup_plugins(PluginComponent::SetupModeType mode, PluginComponents::DependenciesPtr dependencies = nullptr);

// reset delayed startup time
// millis() > timeout starts the procedure
// void set_delayed_startup_time(uint32_t timeout);


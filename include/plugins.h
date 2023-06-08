/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <vector>
#include "PluginComponent.h"

#ifndef DEBUG_PLUGINS
#    define DEBUG_PLUGINS (ESP32 || defined(DEBUG_ALL))
#endif

#if ENABLE_DEEP_SLEEP
#    ifndef PLUGIN_DEEP_SLEEP_DELAYED_START_TIME
#        define PLUGIN_DEEP_SLEEP_DELAYED_START_TIME 7500
#    endif
#else
#    ifndef PLUGIN_DEEP_SLEEP_DELAYED_START_TIME
#        define PLUGIN_DEEP_SLEEP_DELAYED_START_TIME 0
#    endif
#endif

namespace PluginComponents {

    class Register {
    public:
        Register() :
            _delayedStartupTime(PLUGIN_DEEP_SLEEP_DELAYED_START_TIME)
        {
        }

        static Register *getInstance();
        static void add(PluginComponent *plugin, const char *name = nullptr);
        static void setDelayedStartupTime(uint32_t delayedStartupTime);
        static PluginsVector &getPlugins();

    public:
        void sort();
        void setup(SetupModeType mode, DependenciesPtr dependencies = nullptr);
        void dumpList(Print &output);

    protected:
        static void _add(PluginComponent *plugin, const char *name = nullptr);
        void _setDelayedStartupTime(uint32_t delayedStartupTime);
        PluginsVector &_getPlugins();

    protected:
        uint32_t _delayedStartupTime;
    };

    extern PluginsVector &_plugins;

    inline __attribute__((__always_inline__))
    void Register::add(PluginComponent *plugin, const char *name)
    {
        _add(plugin, name);
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

// register plug in
#if DEBUG_PLUGINS
#define REGISTER_PLUGIN(plugin, name)                   PluginComponents::Register::add(plugin, name)
#else
#define REGISTER_PLUGIN(plugin, name)                   PluginComponents::Register::add(plugin)
#endif

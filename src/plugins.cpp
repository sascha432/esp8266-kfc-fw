/**
  Author: sascha_lammers@gmx.de
*/

#include "plugins.h"

PluginsVector plugins;

void init_plugin(const String &name, Plugin_t &plugin) {
    memset(&plugin, 0, sizeof(plugin));
    plugin.pluginName = name;
}

bool register_plugin(Plugin_t &plugin) {
    plugins.push_back(plugin);
    return true;
}

void setup_plugins() {
    for(const auto plugin : plugins) {
        plugin.setupPlugin();
    }
}

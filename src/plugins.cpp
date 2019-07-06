/**
  Author: sascha_lammers@gmx.de
*/

#include <Form.h>
#include <ESPAsyncWebServer.h>
#include "templates.h"
#include "plugins.h"

PluginsVector plugins;

void init_plugin(const String &name, Plugin_t &plugin, uint8_t priority) {
    memset(&plugin, 0, sizeof(plugin));
    plugin.pluginName = name;
    plugin.setupPriority = priority;
}

void register_plugin(Plugin_t &plugin) {
    debug_printf_P(PSTR("register_plugin %s priority %d\n"), plugin.pluginName.c_str(), plugin.setupPriority);
    plugins.push_back(plugin);
}

void setup_plugins(bool isSafeMode) {

#if MDNS_SUPPORT
    void add_plugin_mdns_plugin();
    add_plugin_mdns_plugin();
#endif
#if WEBSERVER_SUPPORT
    void add_plugin_web_server();
    add_plugin_web_server();
#endif
#if SYSLOG
    void add_plugin_syslog();
    add_plugin_syslog();
#endif
#if NTP_CLIENT
    void add_plugin_ntp_client();
    add_plugin_ntp_client();
#endif
#if MQTT_SUPPORT
    void add_plugin_mqtt();
    add_plugin_mqtt();
#endif
#if PING_MONITOR
    void add_plugin_ping_monitor();
    add_plugin_ping_monitor();
#endif
#if HTTP2SERIAL
    void add_plugin_http2serial();
    add_plugin_http2serial();
#endif
#if SERIAL2TCP
    void add_plugin_serial2tcp();
    add_plugin_serial2tcp();
#endif
#if HOME_ASSISTANT_INTEGRATION
    void add_plugin_homeassistant();
    add_plugin_homeassistant();
#endif
#if HUE_EMULATION
    void add_plugin_hue();
    add_plugin_hue();
#endif
#if IOT_ATOMIC_SUN_V1
    void add_plugin_atomic_sun_v1();
    add_plugin_atomic_sun_v1();
#endif
#if IOT_ATOMIC_SUN_V2
    void add_plugin_atomic_sun_v2();
    add_plugin_atomic_sun_v2();
#endif
#if FILE_MANAGER
    void add_plugin_file_manager();
    add_plugin_file_manager();
#endif
#if I2CSCANNER_PLUGIN
    void add_plugin_i2cscanner_plugin();
    add_plugin_i2cscanner_plugin();
#endif
#if ESP8266_AT_MODE_SUPPORT
    void add_plugin_esp8266_at_mode();
    add_plugin_esp8266_at_mode();
#endif

    debug_printf_P(PSTR("setup_plugins(%d) counter %d\n"), isSafeMode, plugins.size());

    std::sort(plugins.begin(), plugins.end(), [](const Plugin_t &a, const Plugin_t &b) {
        return b.setupPriority >= a.setupPriority;
    });
    for(const auto plugin : plugins) {
        debug_printf_P(PSTR("setup_plugins(%d) %s priority %d\n"), isSafeMode, plugin.pluginName.c_str(), plugin.setupPriority);
        if (plugin.setupPlugin) {
            plugin.setupPlugin(isSafeMode);
        }
    }
}

Plugin_t *get_plugin_by_name(const String &name) {
    for(auto it = plugins.begin(); it != plugins.end(); ++it) {
        auto plugin = &*it;
        if (plugin->pluginName.equals(name)) {
            debug_printf_P(PSTR("get_plugin_by_name(%s) = %p\n"), name.c_str(), plugin);
            return plugin;
        }
    }
    debug_printf_P(PSTR("get_plugin_by_name(%s) = nullptr\n"), name.c_str());
    return nullptr;
}

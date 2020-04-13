/**
  Author: sascha_lammers@gmx.de
*/

#include "plugins.h"
#include "plugins_menu.h"
#include <Form.h>
#include <algorithm>
#include "RTCMemoryManager.h"
#include "progmem_data.h"
#include "misc.h"
#ifndef DISABLE_EVENT_SCHEDULER
#include <EventScheduler.h>
#endif

#define PLUGIN_RTC_MEM_MAX_ID       255

#if DEBUG_PLUGINS
#include "debug_helper_enable.h"
#else
#include "debug_helper_disable.h"
#endif

PluginsVector plugins;
BootstrapMenu bootstrapMenu;
NavMenu_t navMenu;

// we need to store the plugins somewhere until the vector is initialized otherwise it will discard already registered plugins
static PluginsVector *pluginsPtr = nullptr;

#if DEBUG_PLUGINS
void register_plugin(PluginComponent *plugin, const char *name)
{
    Serial.begin(KFC_SERIAL_RATE);
    Serial.printf_P(PSTR("register_plugin(%p): name=%s\n"), plugin, name);
#else
void register_plugin(PluginComponent *plugin)
{
#endif
    if (plugins.size()) {
        _debug_printf(PSTR("Registering plugins completed already, skipping %s\n"), plugin->getName());
        return;
    }
    _debug_printf_P(PSTR("register_plugin %s priority %d\n"), plugin->getName(), plugin->getSetupPriority());
    if (!pluginsPtr) {
        pluginsPtr = new PluginsVector();
        pluginsPtr->reserve(16);
    }
    pluginsPtr->push_back(plugin);
}

void dump_plugin_list(Print &output)
{
#if DEBUG

    #define BOOL_STR(value) (value ? SPGM(yes) : SPGM(no))

    //                    123456789   12345   123456789   123456789012   1234567890    123456   123456  12345  123456789012
    PGM_P header = PSTR("+-----------+------+-----------+--------------+------------+--------+--------+-------+------------+\n");
    PGM_P titles = PSTR("| Name      | Prio | Safe Mode | Auto Wake-Up | RTC Mem Id | Status | ATMode | WebUI | Setup Time +\n");
    PGM_P format = PSTR("| %-9.9s | % 4d | %-9.9s | %-12.12s | % 10u | %-6.6s | %-6.6s | %-5.5s | %10u |\n");

    output.print(FPSTR(header));
    output.printf_P(titles);
    output.print(FPSTR(header));
    for(const auto plugin : plugins) {
        _debug_printf_P(PSTR("plugin=%p\n"), plugin);
        output.printf_P(format,
            plugin->getName(),
            plugin->getSetupPriority(),
            BOOL_STR(plugin->allowSafeMode()),
            BOOL_STR(plugin->autoSetupAfterDeepSleep()),
            plugin->getRtcMemoryId(),
            BOOL_STR(plugin->hasStatus()),
            BOOL_STR(plugin->hasAtMode()),
            BOOL_STR(plugin->hasWebUI()),
            plugin->getSetupTime()
        );
    }
    output.print(FPSTR(header));
#endif

}

void prepare_plugins()
{
    // copy & sort plugins and free temporary data
    plugins.resize(pluginsPtr->size());
    std::partial_sort_copy(pluginsPtr->begin(), pluginsPtr->end(), plugins.begin(), plugins.end(), [](const PluginComponent *a, const PluginComponent *b) {
         return b->getSetupPriority() >= a->getSetupPriority();
    });
    free(pluginsPtr);

    _debug_printf_P(PSTR("counter=%d\n"), plugins.size());

#if DEBUG
    // check for duplicate RTC memory ids
    uint8_t i = 0;
    for(const auto plugin : plugins) {
#if DEBUG_PLUGINS
        _debug_printf_P(PSTR("name=%s plugin=%p prio=%d\n"), plugin->getName(), plugin, plugin->getSetupPriority());
#endif

        if (plugin->getRtcMemoryId() > PLUGIN_RTC_MEM_MAX_ID) {
            _debug_printf_P(PSTR("WARNING! Plugin %s is using an invalid id %d\n"), plugin->getName(), plugin->getRtcMemoryId());
        }
        if (plugin->getRtcMemoryId()) {
            uint8_t j = 0;
            for(const auto plugin2 : plugins) {
                if (i != j && plugin2->getRtcMemoryId() && plugin->getRtcMemoryId() == plugin2->getRtcMemoryId()) {
                    _debug_printf_P(PSTR("WARNING! Plugin %s and %s use the same id (%d) to identify the RTC memory data\n"), plugin->getName(), plugin2->getName(), plugin->getRtcMemoryId());
                }
                j++;
            }
        }
        i++;
    }
#endif

}

static void create_menu()
{
    navMenu.home = bootstrapMenu.addMenu(F("Home"));
    bootstrapMenu.getItem(navMenu.home)->setUri(F("index.html"));

    // since "home" has an URI, this menu is hidden
    bootstrapMenu.addSubMenu(F("Home"), F("index.html"), navMenu.home);
    bootstrapMenu.addSubMenu(F("Status"), F("status.html"), navMenu.home);
    bootstrapMenu.addSubMenu(F("Manage WiFi"), F("wifi.html"), navMenu.home);
    bootstrapMenu.addSubMenu(F("Configure Network"), F("network.html"), navMenu.home);
    bootstrapMenu.addSubMenu(F("Change Password"), F("password.html"), navMenu.home);
    bootstrapMenu.addSubMenu(F("Reboot Device"), F("reboot.html"), navMenu.home);
    bootstrapMenu.addSubMenu(F("About"), F("about.html"), navMenu.home);

    navMenu.status = bootstrapMenu.addMenu(F("Status"));
    bootstrapMenu.getItem(navMenu.status)->setUri(F("status.html"));

    navMenu.config = bootstrapMenu.addMenu(F("Configuration"));
    bootstrapMenu.addSubMenu(F("WiFi"), F("wifi.html"), navMenu.config);
    bootstrapMenu.addSubMenu(F("Network"), F("network.html"), navMenu.config);
    bootstrapMenu.addSubMenu(F("Remote Access"), F("remote.html"), navMenu.config);

    navMenu.device = bootstrapMenu.addMenu(F("Device"));

    navMenu.admin = bootstrapMenu.addMenu(F("Admin"));
    bootstrapMenu.addSubMenu(F("Change Password"), F("password.html"), navMenu.admin);
    bootstrapMenu.addSubMenu(F("Reboot Device"), F("reboot.html"), navMenu.admin);
    bootstrapMenu.addSubMenu(F("Restore Factory Defaults"), F("factory.html"), navMenu.admin);
    bootstrapMenu.addSubMenu(F("Export Settings"), F("export_settings"), navMenu.admin);
    bootstrapMenu.addSubMenu(F("Update Firmware"), F("update_fw.html"), navMenu.admin);

    navMenu.util = bootstrapMenu.addMenu(F("Utilities"));
    bootstrapMenu.addSubMenu(F("Speed Test"), F("speed_test.html"), navMenu.util);
}

static bool enableWebUIMenu = false;

void setup_plugins(PluginComponent::PluginSetupMode_t mode)
{
    _debug_printf_P(PSTR("mode=%d counter=%d\n"), mode, plugins.size());

    if (mode != PluginComponent::PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP) {
        create_menu();
    }

    for(const auto plugin : plugins) {
        bool runSetup =
            (mode == PluginComponent::PLUGIN_SETUP_DEFAULT) ||
            (mode == PluginComponent::PLUGIN_SETUP_SAFE_MODE && plugin->allowSafeMode()) ||
            (mode == PluginComponent::PLUGIN_SETUP_AUTO_WAKE_UP && plugin->autoSetupAfterDeepSleep()) ||
            (mode == PluginComponent::PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP && !plugin->autoSetupAfterDeepSleep());
        _debug_printf_P(PSTR("name=%s prio=%d setup=%d\n"), plugin->getName(), plugin->getSetupPriority(), runSetup);
        if (runSetup) {
            plugin->setSetupTime();
            plugin->setup(mode);

            if (plugin->hasWebUI()) {
                enableWebUIMenu = true;
            }
            switch(plugin->getMenuType()) {
                case PluginComponent::MenuTypeEnum_t::CUSTOM:
                    plugin->createMenu();
                    break;
                case PluginComponent::MenuTypeEnum_t::AUTO:
                    if (plugin->getConfigureForm()) {
                        String uri = FPSTR(plugin->getConfigureForm());
                        uri += F(".html");
                        bootstrapMenu.addSubMenu(plugin->getFriendlyName(), uri, navMenu.config);
                    }
                    break;
                case PluginComponent::MenuTypeEnum_t::NONE:
                default:
                    break;
            }
        }
    }

    if (enableWebUIMenu) {
        auto url = F("webui.html");
        if (!bootstrapMenu.isValid(bootstrapMenu.findMenuByURI(url, navMenu.device))) {
            auto webUi = F("Web UI");
            bootstrapMenu.addSubMenu(webUi, url, navMenu.device);
            bootstrapMenu.addSubMenu(webUi, url, navMenu.home, bootstrapMenu.getId(bootstrapMenu.findMenuByURI(F("status.html"), navMenu.home)));
        }
    }

#ifndef DISABLE_EVENT_SCHEDULER
    if (mode == PluginComponent::PLUGIN_SETUP_AUTO_WAKE_UP) {
        Scheduler.addTimer(PLUGIN_DEEP_SLEEP_DELAYED_START_TIME, false, [](EventScheduler::TimerPtr timer) {
            setup_plugins(PluginComponent::PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP);
        });
    }
#endif
}

/**
  Author: sascha_lammers@gmx.de
*/

#include "plugins.h"
#include "plugins_menu.h"
#include <Form.h>
#include <algorithm>
#include "kfc_fw_config.h"
#include "RTCMemoryManager.h"
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

using KFCConfigurationClasses::System;

PluginsVector plugins;
BootstrapMenu bootstrapMenu;
NavMenu_t navMenu;

// we need to store the plugins somewhere until the vector is initialized otherwise it will discard already registered plugins
static PluginsVector *pluginsPtr = nullptr;

#if DEBUG_PLUGINS
void register_plugin(PluginComponent *plugin, const char *name)
{
    KFC_SAFE_MODE_SERIAL_PORT.begin(KFC_SERIAL_RATE);
    ::printf(PSTR("register_plugin(%p): name=%s\r\n"), plugin, name);
#else
void register_plugin(PluginComponent *plugin)
{
#endif
    if (plugins.size()) {
        __LDBG_printf("Registering plugins completed already, skipping %s", plugin->getName_P());
        return;
    }
    __LDBG_printf("register_plugin %s priority %d", plugin->getName_P(), plugin->getOptions().priority);
    if (!pluginsPtr) {
        pluginsPtr = __LDBG_new(PluginsVector);
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
        __LDBG_printf("plugin=%p", plugin);
        auto options = plugin->getOptions();
        output.printf_P(format,
            plugin->getName_P(),
            options.priority,
            BOOL_STR(plugin->allowSafeMode()),
#if ENABLE_DEEP_SLEEP
            BOOL_STR(plugin->autoSetupAfterDeepSleep()),
#else
            SPGM(n_a, "n/a"),
#endif
            options.memory_id,
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
         return static_cast<int>(b->getOptions().priority) >= static_cast<int>(a->getOptions().priority);
    });
    free(pluginsPtr);

    __LDBG_printf("counter=%d", plugins.size());
}

static void create_menu()
{
    //TODO move to PROGMEM

    navMenu.home = bootstrapMenu.addMenu(FSPGM(Home));
    bootstrapMenu.getItem(navMenu.home)->setUri(FSPGM(index_html));

    // since "home" has an URI, this menu is hidden
    bootstrapMenu.addSubMenu(FSPGM(Home), FSPGM(index_html), navMenu.home);
    bootstrapMenu.addSubMenu(FSPGM(Status), FSPGM(status_html), navMenu.home);
    bootstrapMenu.addSubMenu(F("Manage WiFi"), FSPGM(wifi_html), navMenu.home);
    bootstrapMenu.addSubMenu(F("Configure Network"), FSPGM(network_html), navMenu.home);
    bootstrapMenu.addSubMenu(FSPGM(Change_Password), FSPGM(password_html), navMenu.home);
    bootstrapMenu.addSubMenu(FSPGM(Reboot_Device), FSPGM(reboot_html), navMenu.home);
    bootstrapMenu.addSubMenu(F("About"), F("about.html"), navMenu.home);

    navMenu.status = bootstrapMenu.addMenu(FSPGM(Status));
    bootstrapMenu.getItem(navMenu.status)->setUri(FSPGM(status_html));

    navMenu.config = bootstrapMenu.addMenu(FSPGM(Configuration));
    bootstrapMenu.addSubMenu(FSPGM(WiFi), FSPGM(wifi_html), navMenu.config);
    bootstrapMenu.addSubMenu(FSPGM(Network), FSPGM(network_html), navMenu.config);
    bootstrapMenu.addSubMenu(FSPGM(Device), FSPGM(device_html), navMenu.config);
    bootstrapMenu.addSubMenu(F("Remote Access"), FSPGM(remote_html), navMenu.config);

    navMenu.device = bootstrapMenu.addMenu(FSPGM(Device));

    navMenu.admin = bootstrapMenu.addMenu(FSPGM(Admin));
    bootstrapMenu.addSubMenu(FSPGM(Change_Password), FSPGM(password_html), navMenu.admin);
    bootstrapMenu.addSubMenu(FSPGM(Reboot_Device), FSPGM(reboot_html), navMenu.admin);
    bootstrapMenu.addSubMenu(F("Restore Factory Defaults"), FSPGM(factory_html), navMenu.admin);
    bootstrapMenu.addSubMenu(F("Export Settings"), F("export_settings"), navMenu.admin);
    bootstrapMenu.addSubMenu(F("Update Firmware"), FSPGM(update_fw_html), navMenu.admin);

    navMenu.util = bootstrapMenu.addMenu(F("Utilities"));
    bootstrapMenu.addSubMenu(F("Speed Test"), F("speed_test.html"), navMenu.util);
}

static bool enableWebUIMenu = false;

void setup_plugins(PluginComponent::SetupModeType mode)
{
    __LDBG_printf("mode=%d counter=%d", mode, plugins.size());

    if (mode != PluginComponent::SetupModeType::DELAYED_AUTO_WAKE_UP) {
        create_menu();
    }

    auto blacklist = System::Firmware::getPluginBlacklist();

    PluginComponent::createDependencies();

    for(const auto plugin : plugins) {
        if (stringlist_find_P_P(blacklist, plugin->getName_P()) != -1) {
            __DBG_printf("plugin=%s blacklist=%s", plugin->getName_P(), blacklist);
            continue;
        }
        bool runSetup = (
            (mode == PluginComponent::SetupModeType::DEFAULT) ||
            (mode == PluginComponent::SetupModeType::SAFE_MODE && plugin->allowSafeMode())
#if ENABLE_DEEP_SLEEP
            ||
            (mode == PluginComponent::SetupModeType::AUTO_WAKE_UP && plugin->autoSetupAfterDeepSleep()) ||
            (mode == PluginComponent::SetupModeType::DELAYED_AUTO_WAKE_UP && !plugin->autoSetupAfterDeepSleep())
#endif
        );
        __LDBG_printf("name=%s prio=%d setup=%d mode=%u menu=%u add_menu=%u", plugin->getName_P(), plugin->getOptions().priority, runSetup, mode, plugin->getMenuType(), (mode != PluginComponent::SetupModeType::DELAYED_AUTO_WAKE_UP));
        if (runSetup) {
            plugin->setSetupTime();
            plugin->setup(mode);
            PluginComponent::checkDependencies();

            if (plugin->hasWebUI()) {
                enableWebUIMenu = true;
            }
        }
        if (mode != PluginComponent::SetupModeType::DELAYED_AUTO_WAKE_UP) {
            switch(plugin->getMenuType()) {
                case PluginComponent::MenuType::CUSTOM:
                    __LDBG_printf("menu=custom plugin=%s", plugin->getName_P());
                    plugin->createMenu();
                    break;
                case PluginComponent::MenuType::AUTO: {
                        StringVector list;
                        explode_P(plugin->getConfigForms(), ',', list);
                        if (std::find(list.begin(), list.end(), plugin->getName()) == list.end()) {
                            list.emplace_back(plugin->getName());
                        }
                        __LDBG_printf("menu=auto plugin=%s forms=%s", plugin->getName_P(), implode(',', list).c_str());
                        for(const auto &str: list) {
                            if (plugin->canHandleForm(str)) {
                                __LDBG_printf("menu=auto form=%s can_handle=true", str.c_str());
                                bootstrapMenu.addSubMenu(plugin->getFriendlyName(), str + FSPGM(_html), navMenu.config);
                            }
                        }
                    } break;
                case PluginComponent::MenuType::NONE:
                default:
                    __LDBG_printf("menu=none plugin=%s", plugin->getName_P());
                    break;
            }
        }
    }

    if (enableWebUIMenu && System::Flags::getConfig().is_webui_enabled) {
        auto url = F("webui.html");
        if (!bootstrapMenu.isValid(bootstrapMenu.findMenuByURI(url, navMenu.device))) {
            auto webUi = FSPGM(WebUI);
            bootstrapMenu.addSubMenu(webUi, url, navMenu.device);
            bootstrapMenu.addSubMenu(webUi, url, navMenu.home, bootstrapMenu.getId(bootstrapMenu.findMenuByURI(FSPGM(status_html), navMenu.home)));
        }
    }

    PluginComponent::deleteDependencies();

#ifndef DISABLE_EVENT_SCHEDULER
#if ENABLE_DEEP_SLEEP
    if (mode == PluginComponent::PLUGIN_SETUP_AUTO_WAKE_UP) {
        _Scheduler.add(PLUGIN_DEEP_SLEEP_DELAYED_START_TIME, false, [](Event::CallbackTimerPtr timer) {
            setup_plugins(PluginComponent::PLUGIN_SETUP_DELAYED_AUTO_WAKE_UP);
        });
    }
#endif
#endif
}

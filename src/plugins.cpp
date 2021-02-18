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

#if ENABLE_BOOT_LOG

File bootLog;
int8_t bootLogOpenRetries = -10;

#include <PrintString.h>

void bootlog_printf(PGM_P format, ...)
{
    if (config.isSafeMode()) {
        bootLogOpenRetries = 0;
        if (bootLog) {
            bootLog.close();
        }
        return;
    }
    if (bootLogOpenRetries < 0) {
        bootLogOpenRetries = -bootLogOpenRetries;
        if (KFCFS.exists(F("/.pvt/boot.txt"))) {
            KFCFS.remove(F("/.pvt/boot.txt.old"));
            KFCFS.rename(F("/.pvt/boot.txt"), F("/.pvt/boot.txt.old"));
        }
        bootLog = KFCFS.open(F("/.pvt/boot.txt"), fs::FileOpenMode::append);
        bootLog.println(F("--- start"));
    }
    else if (bootLogOpenRetries == 0) {
        return;
    }
    if (!bootLog) {
        bootLog = KFCFS.open(F("/.pvt/boot.txt"), fs::FileOpenMode::append);
        if (!bootLog) {
            delay(10);
            bootLogOpenRetries--;
            return;
        }
    }

    va_list arg;
    va_start(arg, format);
    PrintString str(FPSTR(format), arg);
    bootLog.printf_P(PSTR("%lu: "), millis());
    bootLog.println(str);
    bootLog.flush();
    vprintf(format, arg);
    va_end(arg);
    delay(10);

}

#endif

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
            PSTR("n/a"),
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
    BOOTLOG_PRINTF("prepare_plugins");
    // copy & sort plugins and free temporary data
    plugins.resize(pluginsPtr->size());
    std::partial_sort_copy(pluginsPtr->begin(), pluginsPtr->end(), plugins.begin(), plugins.end(), [](const PluginComponent *a, const PluginComponent *b) {
         return static_cast<int>(b->getOptions().priority) >= static_cast<int>(a->getOptions().priority);
    });
    free(pluginsPtr);

    BOOTLOG_PRINTF("plugins=%u", plugins.size());
    __LDBG_printf("counter=%d", plugins.size());
}

static void create_menu()
{
    //TODO move to PROGMEM

    navMenu.home = bootstrapMenu.addMenu(FSPGM(Home));
    bootstrapMenu.getItem(navMenu.home)->setUri(FSPGM(index_html));

    // since "home" has an URI, this menu is hidden
    bootstrapMenu.addMenuItem(FSPGM(Home), FSPGM(index_html), navMenu.home);
    bootstrapMenu.addMenuItem(FSPGM(Status), FSPGM(status_html), navMenu.home);
    bootstrapMenu.addMenuItem(F("Manage WiFi"), FSPGM(wifi_html), navMenu.home);
    bootstrapMenu.addMenuItem(F("Configure Network"), FSPGM(network_html), navMenu.home);
    bootstrapMenu.addMenuItem(FSPGM(Change_Password), FSPGM(password_html), navMenu.home);
    bootstrapMenu.addMenuItem(FSPGM(Reboot_Device), FSPGM(reboot_html), navMenu.home);
    bootstrapMenu.addMenuItem(F("About"), F("about.html"), navMenu.home);

    navMenu.status = bootstrapMenu.addMenu(FSPGM(Status));
    bootstrapMenu.getItem(navMenu.status)->setUri(FSPGM(status_html));

    navMenu.config = bootstrapMenu.addMenu(FSPGM(Configuration));
    bootstrapMenu.addMenuItem(FSPGM(WiFi), FSPGM(wifi_html), navMenu.config);
    bootstrapMenu.addMenuItem(FSPGM(Network), FSPGM(network_html), navMenu.config);
    bootstrapMenu.addMenuItem(FSPGM(Device), FSPGM(device_html), navMenu.config);
    bootstrapMenu.addMenuItem(F("Remote Access"), FSPGM(remote_html), navMenu.config);

    navMenu.device = bootstrapMenu.addMenu(FSPGM(Device));

    navMenu.admin = bootstrapMenu.addMenu(FSPGM(Admin));
    bootstrapMenu.addMenuItem(FSPGM(Change_Password), FSPGM(password_html), navMenu.admin);
    bootstrapMenu.addMenuItem(FSPGM(Reboot_Device), FSPGM(reboot_html), navMenu.admin);
    bootstrapMenu.addMenuItem(F("Restore Factory Defaults"), FSPGM(factory_html), navMenu.admin);
    bootstrapMenu.addMenuItem(F("Export Settings"), F("export-settings"), navMenu.admin);
    bootstrapMenu.addMenuItem(F("Update Firmware"), FSPGM(update_fw_html), navMenu.admin);

    navMenu.util = bootstrapMenu.addMenu(F("Utilities"));
    bootstrapMenu.addMenuItem(F("Speed Test"), F("speed-test.html"), navMenu.util);
}

static bool enableWebUIMenu = false;
static uint32_t delayedStartupTime;

void set_delayed_startup_time(uint32_t timeout)
{
    delayedStartupTime = timeout;
}

void setup_plugins(PluginComponent::SetupModeType mode)
{
    __LDBG_printf("mode=%d counter=%d", mode, plugins.size());
    BOOTLOG_PRINTF("setup_plugins size=%u mode=%u", plugins.size(), mode);

    if (mode != PluginComponent::SetupModeType::DELAYED_AUTO_WAKE_UP) {
        create_menu();
    }

    auto blacklist = System::Firmware::getPluginBlacklist();
    BOOTLOG_PRINTF("blacklist=%s", __S(blacklist));


    PluginComponent::createDependencies();

    for(const auto plugin : plugins) {
        if (stringlist_find_P_P(blacklist, plugin->getName_P()) != -1) {
            __DBG_printf("plugin=%s blacklist=%s", plugin->getName_P(), blacklist);
            continue;
        }
        if ((mode != PluginComponent::SetupModeType::SAFE_MODE) || (mode == PluginComponent::SetupModeType::SAFE_MODE && plugin->allowSafeMode())) {
            plugin->preSetup(mode);
        }
    }

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
            BOOTLOG_PRINTF("setup plugin=%s", plugin->getName_P());
            plugin->setSetupTime();
// #if ENABLE_DEEP_SLEEP
//             ::printf(PSTR("mode=%u plugin=%s\n"), mode, plugin->getName_P());
// #endif
            plugin->setup(mode);
            BOOTLOG_PRINTF("checking dependencies");
            PluginComponent::checkDependencies();
            BOOTLOG_PRINTF("done");

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
                                bootstrapMenu.addMenuItem(plugin->getFriendlyName(), str + FSPGM(_html), navMenu.config);
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
            bootstrapMenu.addMenuItem(webUi, url, navMenu.device);
            bootstrapMenu.addMenuItem(webUi, url, navMenu.home, bootstrapMenu.getId(bootstrapMenu.findMenuByURI(FSPGM(status_html), navMenu.home)));
        }
    }

    BOOTLOG_PRINTF("deleting dependencies");
    PluginComponent::deleteDependencies();

#ifndef DISABLE_EVENT_SCHEDULER
#if ENABLE_DEEP_SLEEP
    if (mode == PluginComponent::SetupModeType::AUTO_WAKE_UP) {
        delayedStartupTime = PLUGIN_DEEP_SLEEP_DELAYED_START_TIME;
        _Scheduler.add(1000, true, [](Event::CallbackTimerPtr timer) {
            if (millis() > delayedStartupTime) {
                timer->disarm();
                setup_plugins(PluginComponent::SetupModeType::DELAYED_AUTO_WAKE_UP);
            }
        });
    }
#endif
#endif
}

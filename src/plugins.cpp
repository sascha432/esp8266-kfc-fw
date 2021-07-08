/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "PluginComponent.h"
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

#include "plugins_menu.h"

using KFCConfigurationClasses::System;

using namespace PluginComponents;


union RegisterExUnitialized {
    uint8_t buffer[sizeof(RegisterEx)];
    RegisterEx _registerEx;
    RegisterExUnitialized() {}
    ~RegisterExUnitialized() {}
};
static RegisterExUnitialized componentRegister __attribute__((section(".noinit")));

// PluginComponentInitRegisterEx() needs to be called from the first class that uses PluginComponents::Register
// global or static data gets zerod during startup in a certain order...  and clears all data of the object while its being used already
// initializing it manually works well

void PluginComponentInitRegisterEx()
{
    ::new(static_cast<void *>(&componentRegister._registerEx)) RegisterEx();
}

Register *Register::getInstance()
{
    return &componentRegister._registerEx;
}

// we need to store the plugins somewhere until the vector is initialized otherwise it will discard already registered plugins
// static PluginComponents::PluginsVector *pluginsPtr = nullptr;

#if DEBUG_PLUGINS
void register_plugin(PluginComponent *plugin, const char *name)
{
    KFC_SAFE_MODE_SERIAL_PORT.begin(KFC_SERIAL_RATE);
    ::printf(PSTR("register_plugin(%p): name=%s\r\n"), plugin, name);
#else
void Register::_add(PluginComponent *plugin)
{
#endif
    // if (_plugins.size()) {
    //     __LDBG_printf("Registering plugins completed already, skipping %s", plugin->getName_P());
    //     return;
    // }
    __LDBG_printf("register_plugin %s priority %d", plugin->getName_P(), plugin->getOptions().priority);
    // if (!pluginsPtr) {
    //     pluginsPtr = new PluginComponents::PluginsVector();
    //     pluginsPtr->reserve(16);
    // }
    // pluginsPtr->push_back(plugin);
    // _plugins.emplace_back(plugin);

//    TestPtr ptr1;

    // auto iterator = std::upper_bound(_plugins.begin(), _plugins.end(), plugin, [](const PluginComponent *a, const PluginComponent *b) {
    //     return static_cast<int>(b->getOptions().priority) >= static_cast<int>(a->getOptions().priority);
    // });
    // _plugins.insert(iterator, plugin);
    _plugins.push_back(plugin);
}

void Register::dumpList(Print &output)
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
    for(const auto plugin: _plugins) {
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

void Register::prepare()
{
    // copy & sort plugins and free temporary data
    std::sort(_plugins.begin(), _plugins.end(), [](const PluginComponent *a, const PluginComponent *b) {
         return static_cast<int>(b->getOptions().priority) >= static_cast<int>(a->getOptions().priority);
    });
    // _plugins.resize(pluginsPtr->size());
    // std::partial_sort_copy(pluginsPtr->begin(), pluginsPtr->end(), _plugins.begin(), _plugins.end(), [](const PluginComponent *a, const PluginComponent *b) {
    //      return static_cast<int>(b->getOptions().priority) >= static_cast<int>(a->getOptions().priority);
    // });
    // free(pluginsPtr);
//
    // __LDBG_printf("counter=%d", _plugins.size());

    __LDBG_printf("counter=%d", _plugins.size());
}

void RegisterEx::_createMenu()
{
    //TODO move to PROGMEM

    _navMenu.home = _bootstrapMenu.addMenu(FSPGM(Home));
    _bootstrapMenu.getItem(_navMenu.home)->setUri(FSPGM(index_html));

    // since "home" has an URI, this menu is hidden
    _bootstrapMenu.addMenuItem(FSPGM(Home), FSPGM(index_html), _navMenu.home);
    _bootstrapMenu.addMenuItem(FSPGM(Status), FSPGM(status_html), _navMenu.home);
    _bootstrapMenu.addMenuItem(F("Manage WiFi"), FSPGM(wifi_html), _navMenu.home);
    _bootstrapMenu.addMenuItem(F("Configure Network"), FSPGM(network_html), _navMenu.home);
    _bootstrapMenu.addMenuItem(FSPGM(Change_Password), FSPGM(password_html), _navMenu.home);
    _bootstrapMenu.addMenuItem(FSPGM(Reboot_Device), FSPGM(reboot_html), _navMenu.home);
    _bootstrapMenu.addMenuItem(F("About"), F("about.html"), _navMenu.home);

    _navMenu.status = _bootstrapMenu.addMenu(FSPGM(Status));
    _bootstrapMenu.getItem(_navMenu.status)->setUri(FSPGM(status_html));

    _navMenu.config = _bootstrapMenu.addMenu(FSPGM(Configuration));
    _bootstrapMenu.addMenuItem(FSPGM(WiFi), FSPGM(wifi_html), _navMenu.config);
    _bootstrapMenu.addMenuItem(FSPGM(Network), FSPGM(network_html), _navMenu.config);
    _bootstrapMenu.addMenuItem(FSPGM(Device), FSPGM(device_html), _navMenu.config);
    _bootstrapMenu.addMenuItem(F("Remote Access"), FSPGM(remote_html), _navMenu.config);

    _navMenu.device = _bootstrapMenu.addMenu(FSPGM(Device));

    _navMenu.admin = _bootstrapMenu.addMenu(FSPGM(Admin));
    _bootstrapMenu.addMenuItem(FSPGM(Change_Password), FSPGM(password_html), _navMenu.admin);
    _bootstrapMenu.addMenuItem(FSPGM(Reboot_Device), FSPGM(reboot_html), _navMenu.admin);
    _bootstrapMenu.addMenuItem(F("Restore Factory Defaults"), FSPGM(factory_html), _navMenu.admin);
    _bootstrapMenu.addMenuItem(F("Export Settings"), F("export-settings"), _navMenu.admin);
    _bootstrapMenu.addMenuItem(F("Update Firmware"), FSPGM(update_fw_html), _navMenu.admin);

    _navMenu.util = _bootstrapMenu.addMenu(F("Utilities"));
    _bootstrapMenu.addMenuItem(F("Speed Test"), F("speed-test.html"), _navMenu.util);
}

void Register::setup(SetupModeType mode, DependenciesPtr dependencies)
{
    // the object is passed into the next setup_plugins called and destroyed when all are done
    if (!dependencies.get()) {
        dependencies.reset(new PluginComponents::Dependencies());
    }
    auto &_bootstrapMenu = RegisterEx::getBootstrapMenu();
    auto &_navMenu = RegisterEx::getNavMenu();

    __LDBG_printf("mode=%d counter=%d", mode, _plugins.size());

    if (mode != SetupModeType::DELAYED_AUTO_WAKE_UP) {
        RegisterEx::getInstance()._createMenu();
    }

    auto blacklist = System::Firmware::getPluginBlacklist();

    for(const auto plugin : _plugins) {
        if (stringlist_find_P_P(blacklist, plugin->getName_P()) != -1) {
            __DBG_printf("plugin=%s blacklist=%s", plugin->getName_P(), blacklist);
            continue;
        }
        if ((mode != SetupModeType::SAFE_MODE) || (mode == SetupModeType::SAFE_MODE && plugin->allowSafeMode())) {
            plugin->preSetup(mode);
        }
    }

    for(const auto plugin : _plugins) {
        if (stringlist_find_P_P(blacklist, plugin->getName_P()) != -1) {
            __DBG_printf("plugin=%s blacklist=%s", plugin->getName_P(), blacklist);
            continue;
        }
        bool runSetup = (
            (mode == SetupModeType::DEFAULT) ||
            (mode == SetupModeType::SAFE_MODE && plugin->allowSafeMode())
#if ENABLE_DEEP_SLEEP
            ||
            (mode == SetupModeType::AUTO_WAKE_UP && plugin->autoSetupAfterDeepSleep()) ||
            (mode == SetupModeType::DELAYED_AUTO_WAKE_UP && !plugin->autoSetupAfterDeepSleep())
#endif
        );
        __LDBG_printf("name=%s prio=%d setup=%d mode=%u menu=%u add_menu=%u", plugin->getName_P(), plugin->getOptions().priority, runSetup, mode, plugin->getMenuType(), (mode != SetupModeType::DELAYED_AUTO_WAKE_UP));
        if (runSetup) {
            plugin->setSetupTime();
            __LDBG_printf("SETUP plugin=%s mode=%u plugin=%p", plugin->getName_P(), mode, plugin);
            plugin->setup(mode, dependencies);
            dependencies->check();

            if (plugin->hasWebUI()) {
                _enableWebUIMenu = true;
            }
        }
        if (mode != SetupModeType::DELAYED_AUTO_WAKE_UP) {
            switch (plugin->getMenuType()) {
            case MenuType::CUSTOM:
                __LDBG_printf("menu=custom plugin=%s", plugin->getName_P());
                plugin->createMenu();
                break;
            case MenuType::AUTO: {
                StringVector list;
                explode_P(plugin->getConfigForms(), ',', list);
                if (std::find(list.begin(), list.end(), plugin->getName()) == list.end()) {
                    list.emplace_back(plugin->getName());
                }
                __LDBG_printf("menu=auto plugin=%s forms=%s", plugin->getName_P(), implode(',', list).c_str());
                for (const auto &str : list) {
                    if (plugin->canHandleForm(str)) {
                        __LDBG_printf("menu=auto form=%s can_handle=true", str.c_str());
                        _bootstrapMenu.addMenuItem(plugin->getFriendlyName(), str + FSPGM(_html), _navMenu.config);
                    }
                }
            } break;
            case MenuType::NONE:
            default:
                __LDBG_printf("menu=none plugin=%s", plugin->getName_P());
                break;
            }
        }
    }

    if (_enableWebUIMenu && System::Flags::getConfig().is_webui_enabled) {
        auto url = F("webui.html");
        if (!_bootstrapMenu.isValid(_bootstrapMenu.findMenuByURI(url, _navMenu.device))) {
            auto webUi = FSPGM(WebUI);
            _bootstrapMenu.addMenuItem(webUi, url, _navMenu.device, _navMenu.device/*insert at the top*/);
            _bootstrapMenu.addMenuItem(webUi, url, _navMenu.home, _bootstrapMenu.getId(_bootstrapMenu.findMenuByURI(FSPGM(status_html), _navMenu.home)));
        }
    }

#if ENABLE_DEEP_SLEEP
    if (mode == SetupModeType::AUTO_WAKE_UP) {
        if (!dependencies->empty()) {
            __DBG_printf("unresolved dependencies=%u", dependencies->size());
            _delayedStartupTime = PLUGIN_DEEP_SLEEP_DELAYED_START_TIME;
            _Scheduler.add(1000, true, [dependencies, this](Event::CallbackTimerPtr timer) {
                if (millis() > _delayedStartupTime) {
                    __LDBG_printf("DELAYED_AUTO_WAKE_UP");
                    timer->disarm();
                    setup(SetupModeType::DELAYED_AUTO_WAKE_UP, dependencies);
                }
            });
        }
    }
#endif
}

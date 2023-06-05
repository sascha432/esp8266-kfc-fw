/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <stl_ext/memory>
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

#define PLUGIN_RTC_MEM_MAX_ID 255

#if DEBUG_PLUGINS
#    include "debug_helper_enable.h"
#else
#    include "debug_helper_disable.h"
#endif

using KFCConfigurationClasses::System;

using namespace PluginComponents;

Register *Register::getInstance()
{
    return &_pluginRegister;
}

#if DEBUG_PLUGINS

void Register::_add(PluginComponent *plugin, const char *name)
{
    #if ESP8266
        KFC_SAFE_MODE_SERIAL_PORT.begin(KFC_SERIAL_RATE);
    #endif
    ::printf(PSTR("register_plugin(%p): name=%s\r\n"), plugin, name);
#else
void Register::_add(PluginComponent *plugin)
{
#endif
    __LDBG_printf("register_plugin %s priority %d", plugin->getName_P(), plugin->getOptions().priority);
    //// insert sorted
    // auto iterator = std::upper_bound(_plugins.begin(), _plugins.end(), plugin, [](const PluginComponent *a, const PluginComponent *b) {
    //     return static_cast<int>(b->getOptions().priority) >= static_cast<int>(a->getOptions().priority);
    // });
    // _plugins.insert(iterator, plugin);

    // push_back + sort is faster
    _plugins.push_back(plugin);
}

void Register::dumpList(Print &output)
{
    #define BOOL_STR(value) (value ? SPGM(yes) : SPGM(no))

    //                     123456789012345   12345  123456789   123456789012   1234567890   123456   123456   12345   1234567890
    PGM_P header = PSTR("+-----------------+------+-----------+--------------+------------+--------+--------+-------+------------+\n");
    PGM_P titles = PSTR("| Name            | Prio | Safe Mode | Auto Wake-Up | RTC Mem Id | Status | ATMode | WebUI | Setup Time +\n");
    PGM_P format = PSTR("| %-15.15s | % 4d | %-9.9s | %-12.12s | % 10u | %-6.6s | %-6.6s | %-5.5s | %10u |\n");

    output.print(FPSTR(header));
    output.printf_P(titles);
    output.print(FPSTR(header));
    for(const auto plugin: _plugins) {
        __LDBG_printf("plugin=%p", plugin);
        auto options = plugin->getOptions();
        output.printf_P(format,
            plugin->getName_P(),
            static_cast<int8_t>(options.priority),
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
    #if WEBSERVER_KFC_OTA
        _bootstrapMenu.addMenuItem(F("Update Firmware"), FSPGM(update_fw_html), _navMenu.admin);
    #endif
    #if ENABLE_ARDUINO_OTA && !ENABLE_ARDUINO_OTA_AUTOSTART
        _bootstrapMenu.addMenuItem(F("Enable ArduinoOTA"), F("start-arduino-ota"), _navMenu.admin);
    #endif

    _navMenu.util = _bootstrapMenu.addMenu(F("Utilities"));
    #if WEBSERVER_SPEED_TEST
        _bootstrapMenu.addMenuItem(F("Speed Test"), F("speed-test.html"), _navMenu.util);
    #endif
}

void Register::sort()
{
    std::sort(_plugins.begin(), _plugins.end(), [](const PluginComponent *a, const PluginComponent *b) {
        return static_cast<int>(b->getOptions().priority) >= static_cast<int>(a->getOptions().priority);
    });
}

void Register::setup(SetupModeType mode, DependenciesPtr dependencies)
{
    __LDBG_printf("mode=%u dependencies=%p", mode, dependencies.get());
    // the object is passed into the next setup_plugins called and destroyed when all are done
    if (!dependencies.get()) {
        dependencies.reset(new PluginComponents::Dependencies());
    }
    __LDBG_printf("main menu");
    auto &_bootstrapMenu = RegisterEx::getBootstrapMenu();
    __LDBG_printf("nav menu");
    auto &_navMenu = RegisterEx::getNavMenu();

    __LDBG_printf("mode=%d counter=%d", mode, _plugins.size());

    if (mode != SetupModeType::DELAYED_AUTO_WAKE_UP) {
        __LDBG_printf("create menu");
        RegisterEx::getInstance()._createMenu();
    }

    __LDBG_printf("get blacklist");
    auto blacklist = System::Firmware::getPluginBlacklist();

    for(const auto plugin : _plugins) {
        if (stringlist_find_P_P(blacklist, plugin->getName_P()) != -1) {
            __DBG_printf("plugin=%s blacklist=%s", plugin->getName_P(), blacklist);
            continue;
        }
        if ((mode != SetupModeType::SAFE_MODE) || (mode == SetupModeType::SAFE_MODE && plugin->allowSafeMode())) {
            __LDBG_printf("presetup plugin=%s", plugin->getName_P());
            plugin->preSetup(mode);
            __LDBG_printf("presetup plugin=%s done", plugin->getName_P());
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
            __LDBG_printf("setup plugin=%s mode=%u plugin=%p", plugin->getName_P(), mode, plugin);
            plugin->setup(mode, dependencies);
            __LDBG_printf("end_setup plugin=%s", plugin->getName_P());
            dependencies->check();
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

    if (System::Flags::getConfig().is_webui_enabled) {
        auto url = F("webui.html");
        if (!_bootstrapMenu.isValid(_bootstrapMenu.findMenuByURI(url, _navMenu.device))) {
            __LDBG_printf("adding webui");
            auto webUi = FSPGM(WebUI);
            _bootstrapMenu.addMenuItem(webUi, url, _navMenu.device, _navMenu.device/*insert at the top*/);

            auto home = _bootstrapMenu.addMenuItem(webUi, url, _navMenu.home, _bootstrapMenu.getId(_bootstrapMenu.findMenuByURI(FSPGM(status_html), _navMenu.home)));
            (void)home;

            #if WEATHER_STATION_HAVE_BMP_SCREENSHOT
                // auto home = _bootstrapMenu.addMenuItem(webUi, url, _navMenu.home, _bootstrapMenu.getId(_bootstrapMenu.findMenuByURI(FSPGM(status_html), _navMenu.home)));
                _bootstrapMenu.addMenuItem(F("Show Screen"), F("display-screen.html"), _navMenu.home, home);
            #endif
        }
    }

    #if ENABLE_DEEP_SLEEP
        if (mode == SetupModeType::AUTO_WAKE_UP) {
            __LDBG_printf("auto wakeup");
            if (!dependencies->empty()) {
                __LDBG_printf("unresolved dependencies=%u", dependencies->size());
                for(auto dep: dependencies->getVector()) {
                    __LDBG_printf("name=%s,src=%s", dep._name, dep._source->getName());
                }

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
    __LDBG_printf("exiting plugin setup");
}

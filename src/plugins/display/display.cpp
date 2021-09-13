/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <kfc_fw_config.h>
#include <PrintHtmlEntitiesString.h>
#include <LoopFunctions.h>
#include <WiFiCallbacks.h>
#include <EventScheduler.h>
#include <KFCForms.h>
#include <Buffer.h>
#include "display.h"
#include "logger.h"
#include "web_server.h"
#include "plugins.h"
#include "plugins_menu.h"
#include "Utility/ProgMemHelper.h"
#include <stl_ext/algorithm.h>

#if DEBUG_DISPLAY_PLUGIN
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::System;
using Plugins = KFCConfigurationClasses::PluginsType;

// ------------------------------------------------------------------------
// class PingMonitorPlugin

class DisplayPlugin : public PluginComponent {
public:
    DisplayPlugin();

    virtual void setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies) override;
    virtual void reconfigure(const String &source) override;
    virtual void shutdown() override;
    virtual void getStatus(Print &output) override;
    // virtual void createMenu() override;

    virtual void createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request) override;

    #if AT_MODE_SUPPORTED
        virtual ATModeCommandHelpArrayPtr atModeCommandHelp(size_t &size) const override;
        virtual bool atModeHandler(AtModeArgs &args) override;
    #endif

private:
    Display::DisplayType _tft;
};

static DisplayPlugin plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    DisplayPlugin,
    "display",          // name
    "Display Plugin",   // friendly name
    "",                 // web_templates
    "Display",          // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::DISPLAY_PLUGIN,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::NONE),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    false,              // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

DisplayPlugin::DisplayPlugin() :
    PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(DisplayPlugin)),
    _tft(DISPLAY_PLUGIN_WIDTH, DISPLAY_PLUGIN_HEIGHT)
{
    REGISTER_PLUGIN(this, "DisplayPlugin");
}

void DisplayPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    #if DISPLAY_TFT_ESPI
        __LDBG_printf("init %u x %u", _tft.width(), _tft.height());
        _tft.init();

        _tft.setRotation(0);
        _tft.fillScreen(TFT_BLACK);
        _tft.setTextColor(TFT_WHITE, TFT_BLACK);
        _tft.setTextSize(1);
        _tft.setCursor(0, 0);
        _tft.drawString("Booting...",  _tft.width() / 2, _tft.height() / 2);

        #if (TFT_BL >= 0)
            __LDBG_printf("backlight pin %u", TFT_BL);
            pinMode(TFT_BL, OUTPUT);
            digitalWrite(TFT_BL, true);
        #endif

        int color = 0;
        _Scheduler.add(Event::seconds(5), true, [this, color](Event::CallbackTimerPtr timer) mutable {
            _tft.fillScreen(color++ % 2 ? TFT_RED : TFT_GREEN);
        });
    #endif
}

void DisplayPlugin::reconfigure(const String &source)
{
}

void DisplayPlugin::shutdown()
{
    #if DISPLAY_TFT_ESPI && (TFT_BL >= 0)
        #if ESP32
            analogWrite(TFT_BL, -1);
            pinMode(TFT_BL, OUTPUT);
        #endif
        digitalWrite(TFT_BL, false);
    #endif
}

void DisplayPlugin::getStatus(Print &output)
{
    output.printf_P(PSTR("Resolution %u x %u"), _tft.width(), _tft.height());
}

// void DisplayPlugin::createMenu()
// {
// }

void DisplayPlugin::createConfigureForm(FormCallbackType type, const String &formName, FormUI::Form::BaseForm &form, AsyncWebServerRequest *request)
{
    if (!isCreateFormCallbackType(type)) {
        return;
    }

    auto &cfg = Plugins::Display::getWriteableConfig();

    auto &ui = form.createWebUI();
    ui.setStyle(FormUI::WebUI::StyleType::ACCORDION);
    ui.setTitle(F("Display Configuration"));
    ui.setContainerId(F("display_settings"));

    auto &mainGroup = form.addCardGroup(F("dspcfg"));

    form.addObjectGetterSetter(F("bll"), FormGetterSetter(cfg, backlight_level));
    form.addFormUI(F("Backlight"));
    cfg.addRangeValidatorFor_backlight_level(form);

    mainGroup.end();

    form.finalize();
}

#if AT_MODE_SUPPORTED

#include "at_mode.h"

PROGMEM_AT_MODE_HELP_COMMAND_DEF_PPPN(BL, "BL", "<level>", "Set backlight level");

ATModeCommandHelpArrayPtr DisplayPlugin::atModeCommandHelp(size_t &size) const
{
    static ATModeCommandHelpArray tmp PROGMEM = {
        PROGMEM_AT_MODE_HELP_COMMAND(BL)
    };
    size = sizeof(tmp) / sizeof(tmp[0]);
    return tmp;
}

bool DisplayPlugin::atModeHandler(AtModeArgs &args)
{
    if (args.isCommand(PROGMEM_AT_MODE_HELP_COMMAND(BL))) {
        if (args.requireArgs(1, 1)) {
            auto level = args.toIntMinMax<uint8_t>(0, 0, Plugins::Display::DisplayConfig::Config_t::kMaxValueFor_backlight_level);
            if  (level == 0) {
                #if ESP32
                    analogWrite(TFT_BL, -1);
                    pinMode(TFT_BL, OUTPUT);
                #endif
                digitalWrite(TFT_BL, false);
            }
            else if (level == Plugins::Display::DisplayConfig::Config_t::kMaxValueFor_backlight_level) {
                #if ESP32
                    analogWrite(TFT_BL, -1);
                    pinMode(TFT_BL, OUTPUT);
                #endif
                digitalWrite(TFT_BL, true);
            }
            else {
                analogWrite(TFT_BL, level);
            }
            args.print(F("backlight level %u"), level);
        }
        return true;
    }
    return false;
}

#endif

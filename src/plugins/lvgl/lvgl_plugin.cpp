/**
 * Author: sascha_lammers@gmx.de
 */

#include "lvgl_plugin.h"

#if IOT_LVGL_SUPPORT

#include <LoopFunctions.h>
#include <PrintHtmlEntitiesString.h>
#include "lvgl_test_screen.h"
#include "devices/wt32_sc01/wt32_sc01.h"
#include "logger.h"

using Plugins = KFCConfigurationClasses::PluginsType;
using Display = KFCConfigurationClasses::Plugins::DisplayConfigNS::Display;
using DisplayConfig = Display::Config_t;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    LVGLPlugin,
    "lvgl",             // name
    "LVGL Display",     // friendly name
    "",                 // web_templates
    "lvgl",             // config_forms
    "",                 // reconfigure_dependencies
    PluginComponent::PriorityType::DISPLAY_PLUGIN,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    false,              // has_web_ui
    false,              // has_web_templates
    false,              // has_at_mode
    0                   // __reserved
);

static LVGLPlugin plugin;

// LVGL is not re-entrant, the handler runs from the main loop
static void displayLoop()
{
    WT32_SC01::loop();
}

uint8_t LVGLPlugin::_brightness = 100; // percent
uint8_t LVGLPlugin::_backlight = 255; // 0-255

LVGLPlugin::LVGLPlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(LVGLPlugin))
{
    REGISTER_PLUGIN(this, "LVGLPlugin");
}

uint8_t LVGLPlugin::getBrightness()
{
    return _brightness;
}

void LVGLPlugin::_applyBrightness()
{
    auto percent = Display::getConfig().backlight_level;
    if (percent > DisplayConfig::kMaxValueFor_backlight_level) {
        percent = DisplayConfig::kMaxValueFor_backlight_level;
    }
    _brightness = static_cast<uint8_t>(percent);
    _backlight = static_cast<uint8_t>((percent * 255 + 50) / 100);
    WT32_SC01::setBacklight(_backlight);
    __LDBG_printf("brightness=%u%% backlight=%u/255", _brightness, _backlight);
}

bool LVGLPlugin::_initialize()
{
    if (WT32_SC01::display()) { // already initialized
        return true;
    }
    if (!WT32_SC01::begin()) {
        auto error = WT32_SC01::lastError();
        Logger_error(error ? PrintString(F("LVGL: initializing the display failed: %s"), error) : PrintString(F("LVGL: initializing the display failed")));
        return false;
    }
    Logger_notice(F("LVGL display initialized, %ux%u"), IOT_WT32_SC01_TFT_WIDTH, IOT_WT32_SC01_TFT_HEIGHT);
    return true;
}

void LVGLPlugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    // the backlight level is applied by WT32_SC01::begin() as well, so that a failed
    // display init does not leave it at the LVGL default (127/255)
    _applyBrightness();
    if (!_initialize()) {
        return;
    }
    LVGLTestScreen::create();
    LOOP_FUNCTION_ADD(displayLoop);
}

void LVGLPlugin::reconfigure(const String &source)
{
    __LDBG_printf("source=%s", source.c_str());
    _applyBrightness();
}

void LVGLPlugin::shutdown()
{
    LoopFunctions::remove(displayLoop);
    LVGLTestScreen::clear();
}

void LVGLPlugin::getStatus(Print &output)
{
    if (!WT32_SC01::display()) {
        output.print(F("Display not initialized"));
        if (auto error = WT32_SC01::lastError()) {
            output.print(F(": "));
            output.print(error);
        }
        output.print(HTML_S(br));
        output.printf_P(PSTR("Brightness: %u%% (%u/255)" HTML_S(br)), static_cast<unsigned>(_brightness), static_cast<unsigned>(WT32_SC01::getBacklight()));
        return;
    }

    output.printf_P(PSTR("Display: LGFX_WT32_SC01, ST7796S, %ux%u, initialized" HTML_S(br)), static_cast<unsigned>(IOT_WT32_SC01_TFT_WIDTH), static_cast<unsigned>(IOT_WT32_SC01_TFT_HEIGHT));

    output.printf_P(PSTR("Touch: FT6336U, I2C address 0x%02x, %u points, %u event(s)"),
        static_cast<unsigned>(IOT_WT32_SC01_TOUCH_I2C_ADDRESS), static_cast<unsigned>(IOT_WT32_SC01_TOUCH_POINTS), static_cast<unsigned>(LVGLTestScreen::getPressCount()));
    int32_t x;
    int32_t y;
    if (LVGLTestScreen::getLastPoint(x, y)) {
        output.printf_P(PSTR(", last %d,%d"), static_cast<int>(x), static_cast<int>(y));
    }
    output.print(HTML_S(br));

    output.printf_P(PSTR("Brightness: %u%% (%u/255)" HTML_S(br)), static_cast<unsigned>(_brightness), static_cast<unsigned>(WT32_SC01::getBacklight()));
}

#endif

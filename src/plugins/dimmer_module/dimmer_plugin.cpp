/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_plugin.h"
#include <plugins.h>
#include <plugins_menu.h>
#include "../include/templates.h"

#if DEBUG_IOT_DIMMER_MODULE
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace Dimmer;

Plugin Dimmer::dimmer_plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Plugin,
    IOT_DIMMER_PLUGIN_NAME,
    IOT_DIMMER_PLUGIN_FRIENDLY_NAME,
    "",                 // web_templates
    "general," \
    "channels," \
    "buttons," \
    "advanced",         // forms
    "mqtt",             // reconfigure_dependencies
    PluginComponent::PriorityType::DIMMER_MODULE,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::CUSTOM),
    false,              // allow_safe_mode
    false,              // setup_after_deep_sleep
    true,               // has_get_status
    true,               // has_config_forms
    true,               // has_web_ui
    false,              // has_web_templates
    true,               // has_at_mode
    0                   // __reserved
);

Plugin::Plugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(Plugin))
{
    REGISTER_PLUGIN(this, "Dimmer::Plugin");
}

void Plugin::setup(SetupModeType mode, const PluginComponents::DependenciesPtr &dependencies)
{
    setupWebServer();
    Module::setup();
    #if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
        dependencies->dependsOn(F("sensor"), [this](const PluginComponent *plugin, DependencyResponseType response) {
            if (response != DependencyResponseType::SUCCESS) {
                return;
            }
            __LDBG_printf("sensor=%p loaded", plugin);
            this->_setDimmingLevels();
        }, this);
    #endif
}

void Plugin::createWebUI(WebUINS::Root &webUI)
{
    webUI.addRow(WebUINS::Group(F(IOT_DIMMER_TITLE), IOT_DIMMER_GROUP_SWITCH));

    // global sliders, metrics and switches

    #if IOT_DIMMER_HAS_RGB
        auto slider = WebUINS::Slider(F("d-br"), F(IOT_DIMMER_BRIGHTNESS_TITLE), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS));
        slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.min_brightness / 100));
        slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.max_brightness / 100));
        webUI.addRow(slider);

        webUI.addRow(WebUINS::RGBSlider(F("d_rgb"), F("Color"));

        webUI.addRow(WebUINS::ColorTemperatureSlider(F("d-ct"), F("Color Temperature"));

        #if IOT_DIMMER_HAS_RGBW
            auto slider = WebUINS::Slider(F("d_wbr"), F(IOT_DIMMER_WBRIGHTNESS_TITLE), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS));
            slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.min_brightness / 100));
            slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.max_brightness / 100));
            webUI.addRow(slider);
        #endif
    #elif IOT_DIMMER_HAS_COLOR_TEMP

        auto slider = WebUINS::Slider(F("d-br"), F("Brightness"), 0, IOT_DIMMER_MODULE_CHANNELS * IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MODULE_CHANNELS * IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.min_brightness / 100));
        slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MODULE_CHANNELS * IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.max_brightness / 100));
        webUI.addRow(slider);

        auto colorSlider = WebUINS::ColorTemperatureSlider(F("d-ct"), F("Color Temperature"));
        colorSlider.append(WebUINS::NamedInt32(F("min"), ColorTemperature::kColorMin));
        colorSlider.append(WebUINS::NamedInt32(F("max"), ColorTemperature::kColorMax));
        webUI.addRow(colorSlider);
    #else

        // no global sliders

    #endif

    // add sensors below global sliders

    auto sensor = getMetricsSensor();
    if (sensor) {
        sensor->_createWebUI(webUI);
    }

    // add lock button

    #if IOT_DIMMER_GROUP_LOCK
        webUI.appendToLastRow(WebUINS::Switch(F("d-lck"), F("Lock Channels"), true, WebUINS::NamePositionType::TOP, 4));
    #endif

    // single channels

    #if IOT_DIMMER_SINGLE_CHANNELS
        #if IOT_DIMMER_HAS_RGB || IOT_DIMMER_HAS_COLOR_TEMP
            // add group switch if not already in the first group title
            webUI.addRow(WebUINS::Group(F(IOT_DIMMER_CHANNELS_TITLE), !IOT_DIMMER_GROUP_SWITCH));
        #endif

        #if IOT_DIMMER_HAVE_CHANNEL_ORDER
            const auto channelOrder = std::array<int8_t, IOT_DIMMER_MODULE_CHANNELS>({IOT_DIMMER_CHANNEL_ORDER});
        #endif
        for (uint8_t number = 1; number <= IOT_DIMMER_MODULE_CHANNELS; number++) {
            #if IOT_DIMMER_HAVE_CHANNEL_ORDER
                auto idx = channelOrder[number - 1];
            #else
                auto idx = number - 1;
            #endif

            String id = PrintString(F("d-ch%u"), idx);
            String title = PrintString(F("Channel %u"), number);
            auto slider = WebUINS::Slider(id, title, 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);

            #if IOT_DIMMER_MODULE_CHANNELS == 1
                // show min/max brightness range
                slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.min_brightness / 100));
                slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.max_brightness / 100));
            #else
                // internally scaled
            #endif
            webUI.addRow(slider);
        }
    #endif
}

void Plugin::createMenu()
{
    auto root = bootstrapMenu.getMenuItem(navMenu.config);

    auto subMenu = root.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(F("General"), F("dimmer-fw?type=read-config&redirect=dimmer/general.html"));
    subMenu.addMenuItem(F("Channel Configuration"), F("dimmer/channels.html"));
    #if IOT_DIMMER_MODULE_HAS_BUTTONS
        subMenu.addMenuItem(F("Button Configuration"), F("dimmer/buttons.html"));
    #endif
    subMenu.addMenuItem(F("Advanced Firmware Configuration"), F("dimmer-fw?type=read-config&redirect=dimmer/advanced.html"));
    subMenu.addMenuItem(F("Cubic Interpolation"), F("dimmer-ci.html"));
}

#if IOT_DIMMER_MODULE_INTERFACE_UART
#    define IOT_DIMMER_INTERFACE "Serial Port"
#else
#    define IOT_DIMMER_INTERFACE "I2C"
#endif

void Plugin::getStatus(Print &out)
{
    out.printf_P(PSTR("%u Channel MOSFET Dimmer "), IOT_DIMMER_MODULE_CHANNELS);
    if (_isEnabled()) {
        out.print(F("enabled on " IOT_DIMMER_INTERFACE));
        Module::getStatus(out);
    }
    else {
        out.print(F("disabled"));
    }
}

ChannelsArray &Plugin::getChannels()
{
    return _channels;
}

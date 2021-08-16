/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_plugin.h"
#include <plugins.h>
#include <plugins_menu.h>
#include "../include/templates.h"

using namespace Dimmer;

Plugin Dimmer::dimmer_plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    Plugin,
    IOT_DIMMER_PLUGIN_NAME,
    IOT_DIMMER_PLUGIN_FRIENDLY_NAME,
    "",                 // web_templates
    "general,channels,buttons,advanced", // forms
    "mqtt",        // reconfigure_dependencies
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
    // auto row = &webUI.addRow();
    // row->addGroup(F(IOT_DIMMER_TITLE), IOT_DIMMER_GROUP_SWITCH);

    webUI.addRow(WebUINS::Group(F(IOT_DIMMER_TITLE), IOT_DIMMER_GROUP_SWITCH));

    #if IOT_DIMMER_HAS_RGB
        auto slider = WebUINS::Slider(F("d_br"), F(IOT_DIMMER_BRIGHTNESS_TITLE), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS));
        slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100));
        slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100));
        webUI.addRow(slider);

        // row = &webUI.addRow();
        // auto obj = &row->addSlider(F("d_br"), F(IOT_DIMMER_BRIGHTNESS_TITLE), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        // obj->add(JJ(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
        // obj->add(JJ(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);

        webUI.addRow(WebUINS::RGBSlider(F("d_rgb"), F("Color"));
        // row = &webUI.addRow();
        // row->addRGBSlider(F("d_rgb"), F("Color"));

        webUI.addRow(WebUINS::ColorTemperatureSlider(F("d_ct"), F("Color Temperature"));
        // row = &webUI.addRow();
        // row->addColorTemperatureSlider(F("d_ct"), F("Color Temperature"));

        #if IOT_DIMMER_HAS_RGBW
            auto slider = WebUINS::Slider(F("d_wbr"), F(IOT_DIMMER_WBRIGHTNESS_TITLE), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS));
            slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100));
            slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100));
            webUI.addRow(slider);

            // row = &webUI.addRow();
            // obh = &row->addSlider(F("d_wbr"), F(IOT_DIMMER_WBRIGHTNESS_TITLE), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
            // obj->add(JJ(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
            // obj->add(JJ(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);
        #endif
    #elif IOT_DIMMER_HAS_COLOR_TEMP

        auto slider = WebUINS::Slider(F("d_br"), F("Brightness"), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS));
            slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100));
            slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100));
        webUI.addRow(slider);

        // row = &webUI.addRow();
        // auto obj = &row->addSlider(F("d_br"), F("Brightness"), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        // obj->add(JJ(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
        // obj->add(JJ(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);

        webUI.addRow(WebUINS::ColorTemperatureSlider(F("d_ct"), F("Color Temperature"));

        // row = &webUI.addRow();
        // row->addColorTemperatureSlider(F("d_ct"), F("Color Temperature"));
    #else
    #endif

    auto sensor = getMetricsSensor();
    if (sensor) {
        sensor->_createWebUI(webUI);
    }

    #if IOT_DIMMER_GROUP_LOCK
        webUI.addRow(WebUINS::Switch(F("d_lck"), F("Lock Channels"), false, WebUINS::NamePositionType::TOP));
        // row->addSwitch(F("d_lck"), F("Lock Channels"), false, WebUINS::NamePositionType::TOP);
    #endif


    #if IOT_DIMMER_SINGLE_CHANNELS
        #if IOT_DIMMER_HAS_RGB || IOT_DIMMER_HAS_COLOR_TEMP
            // add group switch if not already in the first group title
            // row->addGroup(F(IOT_DIMMER_CHANNELS_TITLE), !IOT_DIMMER_GROUP_SWITCH);
            webUI.addRow(WebUINS::Group(F(IOT_DIMMER_CHANNELS_TITLE), !IOT_DIMMER_GROUP_SWITCH));
        #endif

        #if IOT_DIMMER_HAVE_CHANNEL_ORDER
            const auto channelOrder = std::array<uint8_t, IOT_DIMMER_MODULE_CHANNELS>({IOT_DIMMER_CHANNEL_ORDER});
        #endif
        for (uint8_t number = 1; number <= _channels.size(); number++) {
            #if IOT_DIMMER_HAVE_CHANNEL_ORDER
                auto idx = channelOrder[number - 1];
            #else
                auto idx = number - 1;
            #endif

            auto slider = WebUINS::Slider(PrintString(F("d_chan%u"), idx),  PrintString(F("Channel %u"), number), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
            slider.append(WebUINS::NamedInt32(J(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.min_brightness / 100));
            slider.append(WebUINS::NamedInt32(J(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config._base.max_brightness / 100));
            webUI.addRow(slider);

            // row = &webUI.addRow();
            // auto &obj = row->addSlider(PrintString(F("d_chan%u"), idx), PrintString(F("Channel %u"), number), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
            // obj.add(JJ(range_min), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
            // obj.add(JJ(range_max), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);
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
    #define IOT_DIMMER_INTERFACE "Serial Port"
#else
    #define IOT_DIMMER_INTERFACE "I2C"
#endif

void Plugin::getStatus(Print &out)
{
    out.printf_P(PSTR("%u Channel MOSFET Dimmer "), _channels.size());
    if (_isEnabled()) {
        out.print(F("enabled on " IOT_DIMMER_INTERFACE));
        Module::getStatus(out);
    }
    else {
        out.print(F("disabled"));
    }
}

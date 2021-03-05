
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
    "mqtt,http",        // reconfigure_dependencies
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

void Plugin::setup(SetupModeType mode)
{
    setupWebServer();
    _begin();
#if IOT_SENSOR_HLW80xx_ADJUST_CURRENT
    dependsOn(F("sensor"), [this](const PluginComponent *plugin) {
        __LDBG_printf("sensor=%p loaded", plugin);
        this->_setDimmingLevels();
    });
#endif
}

void Plugin::reconfigure(const String &source)
{
    _readConfig(_config);
    if (source == FSPGM(http)) {
        setupWebServer();
    }
    else {
        _end();
        _begin();
    }
}

void Plugin::shutdown()
{
    _end();
}

void Plugin::createWebUI(WebUIRoot &webUI)
{
    auto row = &webUI.addRow();
    row->addGroup(F(IOT_DIMMER_TITLE), IOT_DIMMER_GROUP_SWITCH);

    #if IOT_DIMMER_HAS_RGB
        row = &webUI.addRow();
        auto obj = &row->addSlider(F("d_br"), F(IOT_DIMMER_BRIGHTNESS_TITLE), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        obj->add(JJ(rmin), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
        obj->add(JJ(rmax), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);

        row = &webUI.addRow();
        row->addRGBSlider(F("d_rgb"), F("Color"));

        row = &webUI.addRow();
        row->addColorTemperatureSlider(F("d_ct"), F("Color Temperature"));

        #if IOT_DIMMER_HAS_RGBW
            row = &webUI.addRow();
            obh = &row->addSlider(F("d_wbr"), F(IOT_DIMMER_WBRIGHTNESS_TITLE), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
            obj->add(JJ(rmin), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
            obj->add(JJ(rmax), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);
        #endif
    #elif IOT_DIMMER_HAS_COLOR_TEMP
        row = &webUI.addRow();
        auto obj = &row->addSlider(F("d_br"), F("Brightness"), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        obj->add(JJ(rmin), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
        obj->add(JJ(rmax), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);

        row = &webUI.addRow();
        row->addColorTemperatureSlider(F("d_ct"), F("Color Temperature"));
    #else
    #endif

    row = &webUI.addRow();
    auto sensor = getMetricsSensor();
    if (sensor) {
        sensor->_createWebUI(webUI, &row);
    }

    #if IOT_DIMMER_GROUP_LOCK
        row->addSwitch(F("d_lck"), F("Lock Channels"), false, WebUIRow::NamePositionType::TOP);
    #endif


    #if IOT_DIMMER_SINGLE_CHANNELS
        #if IOT_DIMMER_HAS_RGB || IOT_DIMMER_HAS_COLOR_TEMP
            // add group switch if not already in the first group title
            row->addGroup(F(IOT_DIMMER_CHANNELS_TITLE), !IOT_DIMMER_GROUP_SWITCH);
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

            row = &webUI.addRow();
            auto &obj = row->addSlider(PrintString(F("d_chan%u"), idx), PrintString(F("Channel %u"), number), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
            obj.add(JJ(rmin), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
            obj.add(JJ(rmax), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);
        }
    #endif


#if DEBUG_ASSETS

    // testing new UI

    row = &webUI.addRow();
    row->addGroup(F("Clock"), false);

    row = &webUI.addRow();
    row->addSlider(FSPGM(brightness), FSPGM(brightness), 0, 1024, true);

    row = &webUI.addRow();
    row->addRGBSlider(F("colorx"), F("Color"));


    row = &webUI.addRow();
    auto height = F("15rem");
    row->addButtonGroup(F("btn_colon"), F("Colon"), F("Solid,Blink slowly,Blink fast")).add(JJ(height), height);
    row->addButtonGroup(F("btn_animation"), F("Animation"), F("Solid,Rainbow,Flashing,Fading")).add(JJ(height), height);
    row->addSensor(FSPGM(light_sensor), F("Ambient Light Sensor"), F("<img src=\"http://192.168.0.100/images/light.svg\" width=\"80\" height=\"80\" style=\"margin-top:-20px;margin-bottom:1rem\">"), WebUIComponent::SensorRenderType::COLUMN).add(JJ(height), height);

    row = &webUI.addRow();
    row->addGroup(F("Various items"), false);

    row = &webUI.addRow();
    row->addListbox(F("listboxtest"), F("My Listbox"), F("{\"1\":\"item1\",\"2\":\"item2\",\"3\":\"item3\",\"4\":\"item4\"}"));

    row = &webUI.addRow();
    row->addGroup(F("Atomic Sun"), false);

    row = &webUI.addRow();
    row->addSlider(F("dimmer_brightness"), F("Atomic Sun Brightness"), 0, 8192);

    row = &webUI.addRow();
    row->addColorTemperatureSlider(F("dimmer_color"), F("Atomic Sun Color"));

    row = &webUI.addRow();
    auto sensor = getMetricsSensor();
    if (sensor) {
        sensor->_createWebUI(webUI, &row);
    }
    row->addSwitch(F("dimmer_lock"), F("Lock Channels"), false, true);

    row->addGroup(F("Channels"), false);

    for(uint8_t j = 0; j < 4; j++) {
        row = &webUI.addRow();
        row->addSlider(PrintString(F("d_chan%u"), 0), PrintString(F("Channel %u"), j + 1), 0, 8192);
    }

#endif
}

void Plugin::createMenu()
{
    auto root = bootstrapMenu.getMenuItem(navMenu.config);

    auto subMenu = root.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(F("General"), F("dimmer/general.html"));
    subMenu.addMenuItem(F("Channel Configuration"), F("dimmer/channels.html"));
#if IOT_DIMMER_MODULE_HAS_BUTTONS
    subMenu.addMenuItem(F("Button Configuration"), F("dimmer/buttons.html"));
#endif
    subMenu.addMenuItem(F("Advanced Firmware Configuration"), F("dimmer/advanced.html"));
}

void Plugin::getStatus(Print &out)
{
    out.printf_P(PSTR("%u Channel MOSFET Dimmer "), _channels.size());
    if (_isEnabled()) {
        out.print(F("enabled on "));
#if IOT_DIMMER_MODULE_INTERFACE_UART
        out.print(F("Serial Port"));
#else
        out.print(F("I2C"));
#endif
        _printStatus(out);
    }
    else {
        out.print(F("disabled"));
    }
}

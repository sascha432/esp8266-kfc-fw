/**
 * Author: sascha_lammers@gmx.de
 */

#include <plugins.h>
#include <plugins_menu.h>
#include "../include/templates.h"
#include "dimmer_module.h"
#include "dimmer_plugin.h"
#include "firmware_protocol.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

Driver_DimmerModule::Driver_DimmerModule() :
    MQTTComponent(ComponentType::SENSOR)
{
}

void DimmerModulePlugin::getStatus(Print &out)
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

void Driver_DimmerModule::_begin()
{
    __LDBG_println();
    Dimmer_Base::_begin();
    _beginMqtt();
    _beginButtons();
    _Scheduler.add(900, false, [this](Event::CallbackTimerPtr) {
        _getChannels();
    });
}

void Driver_DimmerModule::_end()
{
    __LDBG_println();
    _endButtons();
    _endMqtt();
    Dimmer_Base::_end();
}

#if IOT_DIMMER_MODULE_HAS_BUTTONS == 0

// if IOT_DIMMER_MODULE_HAS_BUTTONS == 1 defined in dimmer_buttons.cpp

void Driver_DimmerModule::_beginButtons() {}
void Driver_DimmerModule::_endButtons() {}

#endif

void Driver_DimmerModule::_beginMqtt()
{
    auto client = MQTTClient::getClient();
    if (client) {
        for (uint8_t i = 0; i < _channels.size(); i++) {
            _channels[i].setup(this, i);
            client->registerComponent(&_channels[i]);
        }
        client->registerComponent(this);
    }
}

void Driver_DimmerModule::_endMqtt()
{
    _debug_println();
    auto client = MQTTClient::getClient();
    if (client) {
        client->unregisterComponent(this);
        for(uint8_t i = 0; i < _channels.size(); i++) {
            client->unregisterComponent(&_channels[i]);
        }
    }
}

#if !IOT_DIMMER_MODULE_INTERFACE_UART
void Driver_DimmerModule::onConnect(MQTTClient *client)
{
    _fetchMetrics();
}
#endif

void Driver_DimmerModule::_printStatus(Print &out)
{
    out.print(F(", Fading enabled" HTML_S(br)));
    for(uint8_t i = 0; i < _channels.size(); i++) {
        out.printf_P(PSTR("Channel %u: "), i);
        if (_channels[i].getOnState()) {
            out.printf_P(PSTR("on - %.1f%%" HTML_S(br)), _channels[i].getLevel() / (IOT_DIMMER_MODULE_MAX_BRIGHTNESS * 100.0));
        } else {
            out.print(F("off" HTML_S(br)));
        }
    }
    Dimmer_Base::_printStatus(out);
}


bool Driver_DimmerModule::on(uint8_t channel, float transition)
{
    return _channels[channel].on(transition);
}

bool Driver_DimmerModule::off(uint8_t channel, float transition)
{
    return _channels[channel].off(&_config, transition);
}

bool Driver_DimmerModule::isAnyOn() const
{
    for(uint8_t i = 0; i < getChannelCount(); i++) {
        if (_channels[i].getOnState()) {
            return true;
        }
    }
    return false;
}

// get brightness values from dimmer
void Driver_DimmerModule::_getChannels()
{
    if (_wire.lock()) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_CHANNELS);
        _wire.write(_channels.size() << 4);
        int16_t level;
        const int len = _channels.size() * sizeof(level);
        if (_wire.endTransmission() == 0 && _wire.requestFrom(DIMMER_I2C_ADDRESS, len) == len) {
            for(uint8_t i = 0; i < _channels.size(); i++) {
                _wire.read(level);
                setChannel(i, level);
            }
#if DEBUG_IOT_DIMMER_MODULE
            String str;
            for(uint8_t i = 0; i < _channels.size(); i++) {
                str += String(_channels[i].getLevel());
                str += ' ';
            }
            __LDBG_printf("%s", str.c_str());
#endif
        }
        _wire.unlock();
    }
}

int16_t Driver_DimmerModule::getChannel(uint8_t channel) const
{
    return _channels[channel].getLevel();
}

bool Driver_DimmerModule::getChannelState(uint8_t channel) const
{
    return _channels[channel].getOnState();
}

void Driver_DimmerModule::setChannel(uint8_t channel, int16_t level, float transition)
{
    _channels[channel].setLevel(level, transition);
}

uint8_t Driver_DimmerModule::getChannelCount() const
{
    return _channels.size();
}

void Driver_DimmerModule::_onReceive(size_t length)
{
    // __LDBG_printf("length=%u type=%02x", length, _wire.peek());
    if (_wire.peek() == DIMMER_EVENT_FADING_COMPLETE) {
        _wire.read();
        length--;

        dimmer_fading_complete_event_t event;
        while(length >= sizeof(event)) {
            length -= _wire.read(event);
            if (event.channel < _channels.size()) {
                if (_channels[event.channel].getLevel() != event.level) {  // update level if out of sync
                    _channels[event.channel].setLevel(event.level);
                }
            }
        }

        // update MQTT
        _forceMetricsUpdate(5);
    }
    else {
        Dimmer_Base::_onReceive(length);
    }
}



DimmerModulePlugin dimmer_plugin;

PROGMEM_DEFINE_PLUGIN_OPTIONS(
    DimmerModulePlugin,
    "dimmer",           // name
    "Dimmer",           // friendly name
    "",                 // web_templates
    "general,buttons,advanced", // forms
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

DimmerModulePlugin::DimmerModulePlugin() : PluginComponent(PROGMEM_GET_PLUGIN_OPTIONS(DimmerModulePlugin))
{
    REGISTER_PLUGIN(this, "DimmerModulePlugin");
}


void DimmerModulePlugin::setup(SetupModeType mode)
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

void DimmerModulePlugin::reconfigure(const String &source)
{
    _readConfig(_config);
    if (source == FSPGM(http)) {
        setupWebServer();
    }
    else if (source == FSPGM(mqtt)) {
        _endMqtt();
        _beginMqtt();
    }
    else {
        _endMqtt();
        _endButtons();
        _beginMqtt();
        _beginButtons();
    }
}

void DimmerModulePlugin::shutdown()
{
    _end();
}

void DimmerModulePlugin::createWebUI(WebUIRoot &webUI)
{
    auto row = &webUI.addRow();
    row->addGroup(F("Dimmer"), true);

    for (uint8_t i = 0; i < _channels.size(); i++) {
        row = &webUI.addRow();
        String name = PrintString(F("d_chan%u"), i);
        auto &obj = row->addSlider(name, name, 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        obj.add(JJ(rmin), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.min_brightness / 100);
        obj.add(JJ(rmax), IOT_DIMMER_MODULE_MAX_BRIGHTNESS * _config.max_brightness / 100);
    }

#if DEBUG_ASSETS == 0

    row = &webUI.addRow();
    auto sensor = getMetricsSensor();
    if (sensor) {
        sensor->_createWebUI(webUI, &row);
    }
#else

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

void DimmerModulePlugin::createMenu()
{
    auto root = bootstrapMenu.getMenuItem(navMenu.config);

    auto subMenu = root.addSubMenu(getFriendlyName());
    subMenu.addMenuItem(F("General"), F("dimmer/general.html"));
    subMenu.addMenuItem(F("Button Configuration"), F("dimmer/buttons.html"));
    subMenu.addMenuItem(F("Advanced Firmware Configuration"), F("dimmer/advanced.html"));
}

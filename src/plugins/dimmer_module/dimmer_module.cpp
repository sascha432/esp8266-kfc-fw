/**
 * Author: sascha_lammers@gmx.de
 */

#include "../include/templates.h"
#include <plugins.h>
#include "dimmer_module.h"
#include "dimmer_plugin.h"
#include "firmware_protocol.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;

Driver_DimmerModule::Driver_DimmerModule() : MQTTComponent(ComponentTypeEnum_t::SENSOR)
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
    _getChannels();
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
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        for (uint8_t i = 0; i < _channels.size(); i++) {
            _channels[i].setup(this, i);
            mqttClient->registerComponent(&_channels[i]);
        }
        mqttClient->registerComponent(this);
    }
}

void Driver_DimmerModule::_endMqtt()
{
    _debug_println();
    auto mqttClient = MQTTClient::getClient();
    if (mqttClient) {
        mqttClient->unregisterComponent(this);
        for(uint8_t i = 0; i < _channels.size(); i++) {
            mqttClient->unregisterComponent(&_channels[i]);
        }
    }
}

void Driver_DimmerModule::onConnect(MQTTClient *client)
{
#if !IOT_DIMMER_MODULE_INTERFACE_UART
    _fetchMetrics();
#endif
}

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


bool Driver_DimmerModule::on(uint8_t channel)
{
    return _channels[channel].on();
}

bool Driver_DimmerModule::off(uint8_t channel)
{
    return _channels[channel].off();
}

// get brightness values from dimmer
void Driver_DimmerModule::_getChannels()
{
    __LDBG_println();

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
                _channels[i].setLevel(level);
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

void Driver_DimmerModule::setChannel(uint8_t channel, int16_t level, float time)
{
    if (time == -1) {
        time = getFadeTime(_channels[channel].getLevel(), level);
    }


    _channels[channel].setLevel(level);
    _fade(channel, level, time);
    writeEEPROM();
    _channels[channel].publishState();
}

void Driver_DimmerModule::_onReceive(size_t length)
{
    // __LDBG_printf("length=%u type=%02x", length, _wire.peek());
    if (_wire.peek() == DIMMER_FADING_COMPLETE) {
        _wire.read();
        length--;

        dimmer_fading_complete_event_t event;
        while(length >= sizeof(event)) {
            length -= _wire.read(event);
            if (event.channel < _channels.size()) {
                if (_channels[event.channel].getLevel() != event.level) {  // update level if out of sync
                    _channels[event.channel].setLevel(event.level);
                    _channels[event.channel].publishState();
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
    "dimmer_cfg",       // config_forms
    "mqtt,http",        // reconfigure_dependencies
    PluginComponent::PriorityType::DIMMER_MODULE,
    PluginComponent::RTCMemoryId::NONE,
    static_cast<uint8_t>(PluginComponent::MenuType::AUTO),
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
    dependsOn(F("sensor"), [this](const PluginComponent *plugin) {
        __LDBG_printf("sensor=%p loaded", plugin);
        this->_setDimmingLevels();
    });
}

void DimmerModulePlugin::reconfigure(const String &source)
{
    if (String_equals(source, SPGM(http))) {
        setupWebServer();
    }
    else if (String_equals(source, SPGM(mqtt))) {
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
    row->setExtraClass(JJ(title));
    row->addGroup(F("Dimmer"), true);

    for (uint8_t i = 0; i < _channels.size(); i++) {
        row = &webUI.addRow();
        row->addSlider(PrintString(F("dimmer_channel%u"), i), PrintString(F("dimmer_channel%u"), i), 0, IOT_DIMMER_MODULE_MAX_BRIGHTNESS, true);
    }

    row = &webUI.addRow();
    auto sensor = getMetricsSensor();
    if (sensor) {
        sensor->_createWebUI(webUI, &row);
    }
    // row->addBadgeSensor(F("dimmer_vcc"), F("Dimmer VCC"), 'V');
    // row->addBadgeSensor(F("dimmer_frequency"), F("Dimmer Frequency"), FSPGM(Hz));
    // row->addBadgeSensor(F("dimmer_int_temp"), F("Dimmer ATmega"), FSPGM(_degreeC));
    // row->addBadgeSensor(F("dimmer_ntc_temp"), F("Dimmer NTC"), FSPGM(_degreeC));
}


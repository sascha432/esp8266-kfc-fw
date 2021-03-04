/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_module.h"
#include "firmware_protocol.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using KFCConfigurationClasses::Plugins;
using namespace Dimmer;

Module::Module() :
    MQTTComponent(ComponentType::SENSOR)
{
}

void Module::_begin()
{
    __LDBG_println();
    Base::_begin();
    Buttons::_begin();
    _beginMqtt();
    _Scheduler.add(900, false, [this](Event::CallbackTimerPtr) {
        _getChannels();
    });
}

void Module::_end()
{
    _endMqtt();
    Buttons::_end();
    Base::_end();
}

void Module::_beginMqtt()
{
    auto client = MQTT::Client::getClient();
    if (client) {
        for (uint8_t i = 0; i < _channels.size(); i++) {
            _channels[i].setup(this, i);
            client->registerComponent(&_channels[i]);
        }
        client->registerComponent(this);
    }
}

void Module::_endMqtt()
{
    _debug_println();
    auto client = MQTT::Client::getClient();
    if (client) {
        client->unregisterComponent(this);
        for(uint8_t i = 0; i < _channels.size(); i++) {
            client->unregisterComponent(&_channels[i]);
        }
    }
}

#if !IOT_DIMMER_MODULE_INTERFACE_UART
void Driver_DimmerModule::onConnect()
{
    _fetchMetrics();
}
#endif

void Module::_printStatus(Print &out)
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
    Base::_printStatus(out);
}


bool Module::on(uint8_t channel, float transition)
{
    return _channels[channel].on(transition);
}

bool Module::off(uint8_t channel, float transition)
{
    return _channels[channel].off(&_config, transition);
}

bool Module::isAnyOn() const
{
    for(uint8_t i = 0; i < getChannelCount(); i++) {
        if (_channels[i].getOnState()) {
            return true;
        }
    }
    return false;
}

// get brightness values from dimmer
void Module::_getChannels()
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

int16_t Module::getChannel(uint8_t channel) const
{
    return _channels[channel].getLevel();
}

bool Module::getChannelState(uint8_t channel) const
{
    return _channels[channel].getOnState();
}

void Module::setChannel(uint8_t channel, int16_t level, float transition)
{
    _channels[channel].setLevel(level, transition);
}

uint8_t Module::getChannelCount() const
{
    return _channels.size();
}

void Module::_onReceive(size_t length)
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
        Base::_onReceive(length);
    }
}

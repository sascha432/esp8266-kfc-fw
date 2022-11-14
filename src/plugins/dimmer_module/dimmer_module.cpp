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

using Plugins = KFCConfigurationClasses::PluginsType;
using namespace Dimmer;

void Module::setup()
{
    __LDBG_println();
    Base::begin();
    Buttons::begin();
    _beginMqtt();
    _Scheduler.add(Event::milliseconds(900), false, [this](Event::CallbackTimerPtr) {
        _getChannels();
        #if IOT_DIMMER_HAS_COLOR_TEMP
            _color._channelsToBrightness();
        #endif
    });
}

void Module::shutdown()
{
    _endMqtt();
    Buttons::end();
    Base::end();
}

void Module::_beginMqtt()
{
    for (uint8_t i = 0; i < _channels.size(); i++) {
        _channels[i].setup(this, i);
        MQTT::Client::registerComponent(&_channels[i]);
    }
    #if IOT_DIMMER_HAS_COLOR_TEMP
        MQTT::Client::registerComponent(&_color);
    #endif
    MQTT::Client::registerComponent(this);
}

void Module::_endMqtt()
{
    MQTT::Client::unregisterComponent(this);
    #if IOT_DIMMER_HAS_COLOR_TEMP
        MQTT::Client::unregisterComponent(&_color);
    #endif
    for(uint8_t i = 0; i < _channels.size(); i++) {
        MQTT::Client::unregisterComponent(&_channels[i]);
    }
}

void Module::getStatus(Print &out)
{
    out.print(F(", Fading enabled"));
    auto &cfg = _getConfig();
    if (cfg) {
        auto &cfg = _config._firmwareConfig;
        if (cfg.bits.cubic_interpolation) {
            out.print(F(", Cubic Interpolation enabled"));
        }
    }
    out.print(F(HTML_S(br)));

    for(uint8_t i = 0; i < _channels.size(); i++) {
        out.printf_P(PSTR("Channel %u: "), i);
        if (_channels[i].getOnState()) {
            out.printf_P(PSTR("on - %.1f%% "), (_channels[i].getLevel() * 100) / static_cast<float>(IOT_DIMMER_MODULE_MAX_BRIGHTNESS));
        }
        else {
            out.print(F("off "));
        }
    }
    out.print(F(HTML_S(br)));

    Base::getStatus(out);
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
    TwoWire::Lock lock(_wire);
    if (lock) {
        _wire.beginTransmission(DIMMER_I2C_ADDRESS);
        _wire.write(DIMMER_REGISTER_COMMAND);
        _wire.write(DIMMER_COMMAND_READ_CHANNELS);
        _wire.write(_channels.size() << 4); // read 0 - (_channels.size() - 1)
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
    }
}

void Module::_onReceive(size_t length)
{
    __LDBG_printf("length=%u type=%02x", length, _wire.peek());
    if (_wire.peek() == DIMMER_EVENT_FADING_COMPLETE) {
        _wire.read();
        length--;

        dimmer_fading_complete_event_t event;
        while(length >= sizeof(event)) {
            length -= _wire.read(event);
            if (event.channel < _channels.size()) {
                auto level = _calcLevelReverse(event.level, event.channel);
                auto curLevel = _channels[event.channel].getLevel();
                if (curLevel != level) {  // update level if out of sync
                    __LDBG_printf("resync cur=%u lvl=%u", curLevel, level);
                    auto publish = (event.level == _calcLevel(curLevel, event.channel)); // check if the error comes from up or downsampling and do not publish in those cases
                    _channels[event.channel].setLevel(level, NAN, publish);
                    #if IOT_DIMMER_HAS_COLOR_TEMP
                        if (publish) { // update color temperature if the brightness has changed
                            _color._brightnessToChannels();
                            _color._publish();
                        }
                    #endif
                }
            }
            else {
                __LDBG_printf("invalid channel=%u", event.channel);
            }
        }

        #if IOT_SENSOR_HAVE_HLW8012
            // update MQTT
            _forceMetricsUpdate(5);
        #endif
    }
    else {
        Base::_onReceive(length);
    }
}

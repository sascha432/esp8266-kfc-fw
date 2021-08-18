/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_HAS_COLOR_TEMP

#include "dimmer_colortemp.h"
#include "dimmer_base.h"
#include "dimmer_channel.h"
#include "dimmer_module.h"
#include "dimmer_buttons.h"
#include <WebUISocket.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace Dimmer {

    ColorTemperature::ColorTemperature(Base *base) :
        MQTTComponent(ComponentType::LIGHT),
        _base(*base)
    {
    }

    MQTT::AutoDiscovery::EntityPtr ColorTemperature::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
    {
        auto discovery = new MQTT::AutoDiscovery::Entity();
        switch(num) {
            case 0:
                if (discovery->createJsonSchema(this, FSPGM(main), format)) {
                    discovery->addStateTopic(_createTopics(TopicType::MAIN_STATE));
                    discovery->addCommandTopic(_createTopics(TopicType::MAIN_SET));
                    discovery->addBrightnessScale(_base.getChannelCount() * Channel::getMaxLevel());
                    discovery->addParameter(F("brightness"), true);
                    discovery->addParameter(F("white"), true);
                }
                // discovery->addPayloadOnOff();
                // discovery->addBrightnessStateTopic(_data.brightness.state);
                // discovery->addBrightnessCommandTopic(_data.brightness.set);
                // discovery->addBrightnessScale(MAX_LEVEL_ALL_CHANNELS);
                // discovery->addColorTempStateTopic(_data.color.state);
                // discovery->addColorTempCommandTopic(_data.color.set);
                break;
            case 1:
                if (discovery->create(this, FSPGM(lock_channels, "lock_channels"), format)) {
                    discovery->addStateTopic(_createTopics(TopicType::LOCK_STATE));
                    discovery->addCommandTopic(_createTopics(TopicType::LOCK_SET));
                }
                break;
        }
        return discovery;
    }

    uint8_t ColorTemperature::getAutoDiscoveryCount() const
    {
        return 2;
    }

    void ColorTemperature::onConnect()
    {
        subscribe(_createTopics(TopicType::MAIN_SET));
        subscribe(_createTopics(TopicType::LOCK_SET));
    }

    void ColorTemperature::onMessage(const char *topic, const char *payload, size_t len)
    {
        __DBG_printf("topic=%s payload=%s", topic, payload);

        if (strcmp_end_P(topic, SPGM(_set)) == 0) {
            __LDBG_printf("set main");
            auto stream = HeapStream(payload, len);
            auto reader = MQTT::Json::Reader(&stream);
            if (reader.parse()) {
                #if DEBUG_IOT_DIMMER_MODULE
                    reader.dump(DEBUG_OUTPUT);
                #endif
                // onJsonMessage(client, reader, index);
            }

        }
        else if (strcmp_end_P(topic, PSTR("/lock/set")) == 0) {
            _setLockChannels(atoi(payload));
        }
    }

    void ColorTemperature::getValues(WebUINS::Events &array)
    {
        __LDBG_printf("getValues");
        array.append(WebUINS::Values(F("d-lck"), _channelLock, true));
        array.append(WebUINS::Values(F("d-br"), _brightness, true));
        array.append(WebUINS::Values(F("d-ct"), static_cast<uint32_t>(_color), true));
    }

    void ColorTemperature::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
    {
        __LDBG_printf("id=%s value=%s has_value=%u state=%d has_state=%u", id.c_str(), value.c_str(), hasValue, state, hasState);
        if (hasValue) {
            if (id == F("d-lck")) {
                _setLockChannels(value.toInt());
                _publish();
            }
            else if (id == F("d-ct")) {
                _color = value.toFloat();
                _brightnessToChannels();
                _publish();
            }
            else if (id == F("d-br")) {
                _brightness = value.toInt();
                _brightnessToChannels();
                _publish();
            }
        }
    }

    // convert channels to brightness and color
    void ColorTemperature::_channelsToBrightness()
    {
        auto &_channels = reinterpret_cast<Module &>(_base)._channels;
        // calculate color and brightness values from single channels
        int32_t sum = _channels.getSum();
        if (sum) {
            _color = ((_channels[_channel_ww1].getLevel() + _channels[_channel_ww2].getLevel()) * kColorRange) / static_cast<float>(sum) + kColorMin;
            _brightness = sum;
        }
        else {
            _brightness = 0;
            _color = kColorMin + (kColorRange / 2); // set to center
        }
        _calcRatios();
        __LDBG_printf("ww=%u,%u cw=%u,%u = brightness=%u color=%.0f ratio=%f,%f",
            _channels[_channel_ww1].getLevel(),
            _channels[_channel_ww2].getLevel(),
            _channels[_channel_cw1].getLevel(),
            _channels[_channel_cw2].getLevel(),
            _brightness,
            _color,
            _ratio[0],
            _ratio[1]
        );
    }

    // convert brightness and color to channels
    void ColorTemperature::_brightnessToChannels()
    {
        auto &_channels = reinterpret_cast<Module &>(_base)._channels;
        // calculate single channels from brightness and color
        float color = (_color - kColorMin) / kColorRange;
        uint16_t ww = _brightness * color;
        uint16_t cw = _brightness * (1.0f - color);
        _channels[_channel_ww2].setLevel(ww / _ratio[0]);
        _channels[_channel_ww1].setLevel(ww - _channels[_channel_ww2].getLevel());
        _channels[_channel_cw2].setLevel(cw / _ratio[1]);
        _channels[_channel_cw1].setLevel(cw - _channels[_channel_cw2].getLevel());
        __LDBG_printf("brightness=%u color=%.0f(%f) = ww=%u,%u cw=%u,%u, ratio=%f,%f",
            _brightness,
            _color,
            color,
            _channels[_channel_ww1].getLevel(),
            _channels[_channel_ww2].getLevel(),
            _channels[_channel_cw1].getLevel(),
            _channels[_channel_cw2].getLevel(),
            _ratio[0],
            _ratio[1]
        );
    }

    void ColorTemperature::_publish()
    {
        __LDBG_printf("brightness=%u brightness_pub=%d color=%f color_pub=%f channel_lock=%u ch_lck_pub=%u", _brightness, _brightnessPublished, _color, _colorPublished, _channelLock, _channelLockPublished);
        if (_brightness != _brightnessPublished || _color != _colorPublished || _channelLock != _channelLockPublished) {
            _publishMQTT();
            _publishWebUI();
            _brightness = _brightnessPublished;
            _color = _colorPublished;
            _channelLock = _channelLockPublished;
        }
    }

    void ColorTemperature::_publishMQTT()
    {
        if (isConnected()) {
            using namespace MQTT::Json;
            publish(_createTopics(TopicType::MAIN_STATE), true, UnnamedObject(
                State(_brightness != 0),
                Brightness(_brightness),
                MQTT::Json::ColorTemperature(_color),
                Transition(_base._getConfig()._base._fadetime())).toString()
            );
        }
    }

    void ColorTemperature::_publishWebUI()
    {
        if (WebUISocket::hasAuthenticatedClients()) {
            __LDBG_printf("brightness=%u color=%.0f locked=%u", _brightness, _color, _channelLock);
            WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(
                WebUINS::Values(F("d-br"), _brightness, true),
                WebUINS::Values(F("d-ct"), static_cast<uint32_t>(_color), true),
                WebUINS::Values(F("d-lck"), _channelLock, true)
            )));
        }
    }

    void ColorTemperature::_setLockChannels(bool value)
    {
        __LDBG_printf("state=%u", value);
        _channelLock = value;
        if (value) {
            _ratio[0] = 2;
            _ratio[1] = 2;
            _brightnessToChannels();
            _publish();
        }
    }

    void ColorTemperature::_calcRatios()
    {
        auto &_channels = reinterpret_cast<Module &>(_base)._channels;
        _ratio[0] = _channels[_channel_ww2].getOnState() ?
            ((_channels[_channel_ww1].getLevel() + _channels[_channel_ww2].getLevel()) / static_cast<float>(_channels[_channel_ww2].getLevel())) : (_channels[_channel_ww1].getOnState() ?
                INFINITY : 2);
        _ratio[1] = _channels[_channel_cw2].getOnState() ?
            ((_channels[_channel_cw1].getLevel() + _channels[_channel_cw2].getLevel()) / static_cast<float>(_channels[_channel_cw2].getLevel())) : (_channels[_channel_cw1].getOnState() ?
                INFINITY : 2);
        __LDBG_printf("ww=%f cw=%f", _ratio[0], _ratio[1]);
    }

    void ColorTemperature::begin()
    {
        _brightness = 0;
        _brightnessPublished = ~0;
        _color = 0;
        _colorPublished = NAN;
        _channelLock = false;
        _channelLockPublished = !_channelLock;
    }

    String ColorTemperature::_createTopics(TopicType type)
    {
        switch(type) {
            case TopicType::MAIN_SET:
                return MQTT::Client::formatTopic(String(FSPGM(main)), FSPGM(_set));
            case TopicType::MAIN_STATE:
                return MQTT::Client::formatTopic(String(FSPGM(main)), FSPGM(_state));
            case TopicType::LOCK_SET:
                return MQTT::Client::formatTopic(String(FSPGM(lock_channels)), F("/lock/set"));
            case TopicType::LOCK_STATE:
                return MQTT::Client::formatTopic(String(FSPGM(lock_channels)), F("/lock/state"));
            default:
                break;
        }
        return String();
    }

}

#endif

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
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

namespace Dimmer {

    ColorTemperature::ColorTemperature(Base *base) :
        MQTTComponent(ComponentType::LIGHT),
        _base(base),
        _color(kColorMin + (kColorRangeFloat / 2.0)),
        _channelLock(true)
    {
    }

    #if IOT_ATOMIC_SUN_V2

        bool ColorTemperature::setChannel(uint8_t channel, int16_t level, float transition)
        {
            auto &_channels = _getBase().getChannels();
            if (_channelLock) {
                if (channel == _channel_ww1 || channel == _channel_ww2) {
                    _channels[_channel_ww1].setLevel(level, transition);
                    _channels[_channel_ww2].setLevel(level, transition);
                }
                else if (channel == _channel_cw1 || channel == _channel_cw2) {
                    _channels[_channel_cw1].setLevel(level, transition);
                    _channels[_channel_cw2].setLevel(level, transition);
                }
            }
            else {
                _channels[channel].setLevel(level, transition);
            }
            return true;
        }

    #endif

    MQTT::AutoDiscovery::EntityPtr ColorTemperature::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
    {
        auto discovery = new MQTT::AutoDiscovery::Entity();
        __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
        auto baseTopic = MQTT::Client::getBaseTopicPrefix();
        switch(num) {
            case 0:
                if (discovery->createJsonSchema(this, FSPGM(main), format)) {
                    discovery->addStateTopic(_createTopics(TopicType::MAIN_STATE));
                    discovery->addCommandTopic(_createTopics(TopicType::MAIN_SET));
                    discovery->addName(F("Dimmer Main"));
                    discovery->addObjectId(baseTopic + F("main"));
                    discovery->addBrightnessScale(IOT_DIMMER_MODULE_CHANNELS * IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
                    discovery->addParameter(F("brightness"), true);
                    discovery->addParameter(FSPGM(mqtt_color_mode), true);
                    discovery->addSupportedColorModes(F("[\"color_temp\"]"));

                    discovery->addParameter(FSPGM(mqtt_min_mireds), static_cast<uint16_t>(kColorMin / kColorMultiplier));
                    discovery->addParameter(FSPGM(mqtt_max_mireds), static_cast<uint16_t>(kColorMax / kColorMultiplier));
                }
                break;
            case 1:
                if (discovery->create(this, FSPGM(lock_channels, "lock_channels"), format)) {
                    discovery->addStateTopic(_createTopics(TopicType::LOCK_STATE));
                    discovery->addCommandTopic(_createTopics(TopicType::LOCK_SET));
                    discovery->addName(F("Lock Channels"));
                    discovery->addObjectId(baseTopic + F("lock_channels"));
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
        __LDBG_printf("topic=%s payload=%s", topic, payload);

        if (strcmp_end_P(topic, PSTR("/lock/set")) == 0) {
            _setLockChannels(MQTT::Client::toBool(payload));
        }
        else if (strcmp_end_P(topic, PSTR("/set")) == 0) {
            __LDBG_printf("set main");
            auto stream = HeapStream(payload, len);
            auto reader = MQTT::Json::Reader(&stream);
            if (reader.parse()) {
                #if DEBUG_IOT_DIMMER_MODULE
                    reader.dump(DEBUG_OUTPUT);
                    DEBUG_OUTPUT.flush();
                #endif
                onJsonMessage(reader);
            }
        }
    }

    void ColorTemperature::onJsonMessage(const MQTT::Json::Reader &json)
    {
        __LDBG_printf("main state=%d brightness=%d color=%d", json.state, json.brightness, json.color_temp);
        if (json.state != -1) {
            if (json.state && !_brightness) {
                _getBase()._setOnOffState(true);
            }
            else if (!json.state && _brightness) {
                _getBase()._setOnOffState(false);
            }
        }
        if (json.brightness != -1) {
            if (json.brightness == 0 && _brightness) {
                _getBase()._setOnOffState(false);
            }
            else if (json.brightness) {
                _brightness = json.brightness;
                _brightnessToChannels();
                _publish();
            }
        }
        else if (json.color_temp != -1) {
            _color = json.color_temp * kColorMultiplier;
            _brightnessToChannels();
            _publish();
        }
    }

    void ColorTemperature::getValues(WebUINS::Events &array)
    {
        __LDBG_printf("getValues");
        array.append(WebUINS::Values(F("d-lck"), static_cast<uint32_t>(_channelLock), true));
        array.append(WebUINS::Values(F("d-br"), _brightness, true));
        array.append(WebUINS::Values(F("d-ct"), static_cast<uint32_t>(_color), true));
    }

    void ColorTemperature::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
    {
        __LDBG_printf("id=%s val=%s has_val=%u state=%d has_state=%u", __S(id), __S(value), hasValue, state, hasState);
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

    void ColorTemperature::_publishMQTT()
    {
        if (isConnected()) {
            _Timer(_mqttTimer).throttle(333, [this](Event::CallbackTimerPtr) {
                using namespace MQTT::Json;
                publish(_createTopics(TopicType::MAIN_STATE), true, UnnamedObject(
                    State(_brightness != 0),
                    Brightness(_brightness),
                    MQTT::Json::NamedString(F("color_mode"), F("color_temp")),
                    MQTT::Json::ColorTemperature(_color / kColorMultiplier),
                    Transition(_getBase()._getConfig()._base._getFadeTime())).toString()
                );

                publish(_createTopics(TopicType::LOCK_STATE), true, MQTT::Client::toBoolOnOff(_channelLock));

                auto &_channels = _getBase().getChannels();
                for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
                    _channels[i]._publishMQTT();
                }
            });
        }
    }

    void ColorTemperature::_publishWebUI()
    {
        if (WebUISocket::hasAuthenticatedClients()) {
            __LDBG_printf("clients=%u brightness=%d color=%.0f locked=%u", WebUISocket::hasAuthenticatedClients(), _brightness, _color, _channelLock);
            _Timer(_webuiTimer).throttle(333, [this](Event::CallbackTimerPtr) {
                auto &_channels = _getBase().getChannels();
                WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(
                    WebUINS::Values(F("d-br"), _brightness, true),
                    WebUINS::Values(F("d-ct"), static_cast<uint32_t>(_color), true),
                    WebUINS::Values(F("d-lck"), static_cast<uint32_t>(_channelLock), true),
                    WebUINS::Values(PrintString(F("d-ch%u"), 0), _channels[0].getLevel(), true),
                    WebUINS::Values(PrintString(F("d-ch%u"), 1), _channels[1].getLevel(), true),
                    WebUINS::Values(PrintString(F("d-ch%u"), 2), _channels[2].getLevel(), true),
                    WebUINS::Values(PrintString(F("d-ch%u"), 3), _channels[3].getLevel(), true),
                    WebUINS::Values(F("group-switch-0"), static_cast<uint32_t>(_channels.getSum()), true)
                )));
            });
        }
    }

    // convert channels to brightness and color
    void ColorTemperature::_channelsToBrightness()
    {
        auto &_channels = _getBase().getChannels();
        // calculate color and brightness values from single channels
        int32_t sum = _channels.getSum();
        if (sum) {
            _color = (((_channels[_channel_ww1].getLevel() + _channels[_channel_ww2].getLevel()) * kColorRange) / static_cast<float>(sum)) + kColorMin;
            _brightness = sum;
        }
        else {
            _brightness = 0;
            _color = kColorMin + (kColorRange / 2); // set to center
        }
        _calcRatios();
        __LDBG_printf("ww=%d,%d cw=%d,%d = brightness=%d sum=%d color=%.0f ratio=%f,%f,%f,%f",
            _channels[_channel_ww1].getLevel(),
            _channels[_channel_ww2].getLevel(),
            _channels[_channel_cw1].getLevel(),
            _channels[_channel_cw2].getLevel(),
            _brightness,
            sum,
            _color,
            _ratio[0],
            _ratio[1],
            _ratio[2],
            _ratio[3]
        );
    }

    // convert brightness and color to channels
    void ColorTemperature::_brightnessToChannels()
    {
        auto &_channels = _getBase().getChannels();
        // calculate single channels from brightness and color
        auto color = (_color - kColorMin) / kColorRangeFloat;
        #if DEBUG_IOT_DIMMER_MODULE
            if (color < 0.0 || color > 1.0) {
                __DBG_panic("color=%f _color=%f min=%d range=%f", color, _color, kColorMin, kColorRangeFloat);
            }
        #endif
        // adjust brightness per color
        // ww is 0.0, center is 0.5, cw = 1.0
        auto rww = _ratio[0] + _ratio[1];
        auto rcw = _ratio[2] + _ratio[3];
        // scale brightness to match the ratio pairs for ww/cw
        auto ww = rww ? (_brightness * (color / rww)) : 0.0f;
        auto cw = rcw ? (_brightness * ((1 - color) / rcw)) : 0.0f;
        // set each channel
        _channels[_channel_ww1]._set(ww * _ratio[0], NAN, false);
        _channels[_channel_ww2]._set(ww * _ratio[1], NAN, false);
        _channels[_channel_cw1]._set(cw * _ratio[2], NAN, false);
        _channels[_channel_cw2]._set(cw * _ratio[3], NAN, false);
        _base->_wire.writeEEPROM();
        __LDBG_printf("brightness=%d sum=%d color=%.0f(%f) = ww=%u,%u cw=%u,%u, ratio=%f,%f,%f,%f",
            _brightness,
            _channels.getSum(),
            _color,
            color ,
            _channels[_channel_ww1].getLevel(),
            _channels[_channel_ww2].getLevel(),
            _channels[_channel_cw1].getLevel(),
            _channels[_channel_cw2].getLevel(),
            _ratio[0],
            _ratio[1],
            _ratio[2],
            _ratio[3]
        );
    }

    void ColorTemperature::_publish()
    {
        __LDBG_printf("brightness=%d brightness_pub=%d color=%.0f color_pub=%.0f ch_lock=%u ch_lck_pub=%u", _brightness, _brightnessPublished, _color, _colorPublished, _channelLock, _channelLockPublished);
        if (_brightness != _brightnessPublished || _color != _colorPublished || _channelLock != _channelLockPublished) {
            // publish if any value has been changed
            _publishMQTT();
            _publishWebUI();
            _brightnessPublished = _brightness;
            _colorPublished = _color;
            _channelLockPublished = _channelLock;
        }
    }

    void ColorTemperature::_setLockChannels(bool value)
    {
        __LDBG_printf("lock=%u", value);
        _channelLock = value;
        if (value) {
            // if channels are locked, the ratio is 1:1
            auto &_channels = _getBase().getChannels();
            uint16_t ww = (_channels[_channel_ww1].getLevel() + _channels[_channel_ww2].getLevel()) / 2;
            uint16_t cw = (_channels[_channel_cw1].getLevel() + _channels[_channel_cw2].getLevel()) / 2;
            _channels[_channel_ww1].setLevel(ww);
            _channels[_channel_ww2].setLevel(ww);
            _channels[_channel_cw1].setLevel(cw);
            _channels[_channel_cw2].setLevel(cw);
            _channelsToBrightness();
            _publish();
        }
    }

    void ColorTemperature::_calcRatios()
    {
        auto &_channels = _getBase().getChannels();
        float ww1 = _channels[_channel_ww1].getLevel();
        float ww2 = _channels[_channel_ww2].getLevel();
        float cw1 = _channels[_channel_cw1].getLevel();
        float cw2 = _channels[_channel_cw2].getLevel();
        float sum = ww1 + ww2 + cw1 + cw2;
        if (sum == 0) {
            // all off
            std::fill(std::begin(_ratio), std::end(_ratio), 0.0);
        }
        else {
            if (_channelLock) {
                // set ww and cw channels to the same value
                ww1 = (ww1 + ww2) / 2.0;
                ww2 = ww1;
                cw1 = (cw1 + cw2) / 2.0;
                cw2 = cw1;
            }
            // calculate the percentage per channel, the sum is 1.0
            _ratio[0] = ww1 / sum;
            _ratio[1] = ww2 / sum;
            _ratio[2] = cw1 / sum;
            _ratio[3] = cw2 / sum;
        }
        __LDBG_printf("ratio=%f,%f,%f,%f ww=%.0f/%.0f cw=%.0f/%.0f", _ratio[0], _ratio[1], _ratio[2], _ratio[3], ww1, ww2, cw1, cw2);
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

    void ColorTemperature::end()
    {
        _Timer(_mqttTimer).remove();
        _Timer(_webuiTimer).remove();
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
        }
        __builtin_unreachable();
    }

}

#endif

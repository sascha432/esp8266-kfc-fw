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
        _base(base)
    {
    }

    MQTT::AutoDiscovery::EntityPtr ColorTemperature::getAutoDiscovery(MQTT::FormatType format, uint8_t num)
    {
        auto discovery = new MQTT::AutoDiscovery::Entity();
        __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
        switch(num) {
            case 0:
                if (discovery->createJsonSchema(this, FSPGM(main), format)) {
                    discovery->addStateTopic(_createTopics(TopicType::MAIN_STATE));
                    discovery->addCommandTopic(_createTopics(TopicType::MAIN_SET));
                    discovery->addBrightnessScale(IOT_DIMMER_MODULE_CHANNELS * IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
                    discovery->addParameter(F("brightness"), true);
                    discovery->addParameter(F("color_mode"), true);
                    discovery->addSupportedColorModes(F("[\"color_temp\"]"));
                    // discovery->addParameter(F("min_mireds"), static_cast<uint16_t>(kColorMin / kColorMultiplier));
                    // discovery->addParameter(F("max_mireds"), static_cast<uint16_t>(kColorMax / kColorMultiplier));
                }
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

        if (strcmp_end_P(topic, PSTR("/lock/set")) == 0) {
            _setLockChannels(MQTT::Client::toBool(payload));
        }
        else if (strcmp_end_P(topic, PSTR("/set")) == 0) {
            __LDBG_printf("set main");
            auto stream = HeapStream(payload, len);
            auto reader = MQTT::Json::Reader(&stream);
            if (reader.parse()) {
                #if DEBUG_IOT_DIMMER_MODULE || 1
                    reader.dump(DEBUG_OUTPUT);
                #endif
                onJsonMessage(reader);
            }

        }
    }

    void ColorTemperature::onJsonMessage(const MQTT::Json::Reader &json)
    {
        __LDBG_printf("main state=%d brightness=%d color=%d", json.state, json.brightness, json.color_temp);
        auto &channels = _getBase().getChannels();
        if (json.state != -1) {
            if (json.state && !_brightness) {
                for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
                    channels[i].on();
                }
            }
            else if (!json.state && _brightness) {
                for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
                    channels[i].off();
                }
            }
        }
        if (json.brightness != -1) {
            if (json.brightness == 0 && _brightness) {
                for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
                    channels[i].off();
                }
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
        array.append(WebUINS::Values(F("d-lck"), _channelLock, true));
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
                    MQTT::Json::ColorTemperature(_color),
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
                    WebUINS::Values(F("d-lck"), _channelLock, true),
                    WebUINS::Values(PrintString(F("d-ch%u"), 0), _channels[_channel_ww1].getLevel(), true),
                    WebUINS::Values(PrintString(F("d-ch%u"), 1), _channels[_channel_ww2].getLevel(), true),
                    WebUINS::Values(PrintString(F("d-ch%u"), 2), _channels[_channel_cw1].getLevel(), true),
                    WebUINS::Values(PrintString(F("d-ch%u"), 3), _channels[_channel_cw2].getLevel(), true),
                    WebUINS::Values(F("group-switch-0"), _channels.getSum() ? 1 : 0, true)
                )));
                // for(uint8_t i = 0; i < IOT_DIMMER_MODULE_CHANNELS; i++) {
                //     _channels[i]._publishWebUI();
                // }
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
        __LDBG_printf("ww=%d,%d cw=%d,%d = brightness=%d sum=%d color=%.0f ratio=%f,%f",
            _channels[_channel_ww1].getLevel(),
            _channels[_channel_ww2].getLevel(),
            _channels[_channel_cw1].getLevel(),
            _channels[_channel_cw2].getLevel(),
            _brightness,
            sum,
            _color,
            _ratio[0],
            _ratio[1]
        );
    }

    // convert brightness and color to channels
    void ColorTemperature::_brightnessToChannels()
    {
        auto &_channels = _getBase().getChannels();
        // calculate single channels from brightness and color
        auto color = (_color - kColorMin) / kColorRangeFloat;
        #if DEBUG_IOT_DIMMER_MODULE
            if (color < 0 || color > 1.0f || !isnormal(color)) {
                __DBG_panic("color=%f _color=%f min=%d range=%f", color, _color, kColorMin, kColorRangeFloat);
            }
        #endif
        uint16_t ww = _brightness * color;
        uint16_t cw = _brightness * (1.0f - color);
        _channels[_channel_ww2].setLevel(ww / _ratio[0]);
        _channels[_channel_ww1].setLevel(ww - _channels[_channel_ww2].getLevel());
        _channels[_channel_cw2].setLevel(cw / _ratio[1]);
        _channels[_channel_cw1].setLevel(cw - _channels[_channel_cw2].getLevel());
        __LDBG_printf("brightness=%d sum=%d color=%.0f(%f) = ww=%u,%u cw=%u,%u, ratio=%f,%f",
            _brightness,
            _channels.getSum(),
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
            _ratio[0] = 2;
            _ratio[1] = 2;
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
        if (_channelLock) {
            _ratio[0] = 2;
            _ratio[1] = 2;
            return;
        }
        auto &_channels = _getBase().getChannels();
        auto ww1 = _channels[_channel_ww1].getLevel();
        auto ww2 = _channels[_channel_ww2].getLevel();
        auto cw1 = _channels[_channel_cw1].getLevel();
        auto cw2 = _channels[_channel_cw2].getLevel();
        _ratio[0] = (ww2 ? ((ww1 + ww2) / static_cast<float>(ww2)) : 2);
        if (_ratio[0] == 0) {
            _ratio[0] = 2;
        }
        _ratio[1] = (cw2 ? ((cw1 + cw2) / static_cast<float>(cw2)) : 2);
        if (_ratio[1] == 0) {
            _ratio[1] = 2;
        }
        __LDBG_printf("ratio=%f,%f ww=%d/%d cw=%d/%d", _ratio[0], _ratio[1], ww1, ww2, cw1, cw2);
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
        _mqttTimer.remove();
        _webuiTimer.remove();
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

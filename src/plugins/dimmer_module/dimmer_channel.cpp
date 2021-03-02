/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_channel.h"
#include "dimmer_module.h"
#include <WebUISocket.h>
#include <stl_ext/algorithm.h>
#include <stl_ext/utility.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DimmerChannel::DimmerChannel(Driver_DimmerModule *dimmer, uint8_t channel) :
    MQTTComponent(ComponentType::LIGHT),
    _dimmer(dimmer),
    _publishLastTime(0),
#if IOT_DIMMER_MODULE_HAS_BUTTONS
    _delayTimer(nullptr),
#endif
    _storedBrightness(0),
    _brightness(0),
    _channel(channel),
    _publishFlag(0)
{
}



MQTTComponent::MQTTAutoDiscoveryPtr DimmerChannel::nextAutoDiscovery(MQTT::FormatType format, uint8_t num)
{
    if (num > 0) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0:
            discovery->create(this, PrintString(FSPGM(channel__u), _channel), format);
            discovery->addSchemaJson();
            discovery->addStateTopic(_createTopics(TopicType::COMMAND_STATE));
            discovery->addCommandTopic(_createTopics(TopicType::COMMAND_SET));
            discovery->addBrightnessScale(MAX_LEVEL);
            discovery->addParameter(F("brightness"), true);
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t DimmerChannel::getAutoDiscoveryCount() const
{
    return 1;
}

String DimmerChannel::_createTopics(TopicType type, bool full) const
{
#if IOT_DIMMER_MODULE_CHANNELS > 1
    String value = PrintString(FSPGM(channel__u), _channel);
    #define VALUE value,
    #define VALUEP value +
#else
    #define VALUE
    #define VALUEP
#endif
    switch(type) {
        case TopicType::COMMAND_SET:
            if (!full) {
                return VALUEP FSPGM(_set);
            }
            return MQTTClient::formatTopic(VALUE FSPGM(_set));
        case TopicType::COMMAND_STATE:
            if (!full) {
                return VALUEP FSPGM(_state);
            }
            return MQTTClient::formatTopic(VALUE FSPGM(_state));
        default:
            break;
    }
    return String();
}

void DimmerChannel::onConnect(MQTTClient *client)
{
    client->subscribe(this, _createTopics(TopicType::COMMAND_SET));
    _publishLastTime = 0;
    _publishTimer.remove();
    publishState();
}

void DimmerChannel::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    if (strcmp_end_P(topic, _topic.c_str())) {
        return;
    }
    auto stream = HeapStream(payload, len);
    auto reader = MQTT::JsonReader(&stream);
    if (reader.parse()) {
        onMessage(client, reader);
    }
    else {
        __DBG_printf("parsing json failed=%s payload=%s", reader.getLastErrorMessage().c_str(), payload);
    }

}

void DimmerChannel::onMessage(MQTTClient *client, const MQTT::JsonReader &json)
{
    if (json._state != -1) {
        if (json._state && !_brightness) {
            on();
        }
        else if (!json._state && _brightness) {
            off();
        }
    }
    if (json._brightness != -1) {
        if (json._brightness == 0 && _brightness) {
            off();
        }
        else if (json._brightness) {
            _set(json._brightness);
        }
    }
}

bool DimmerChannel::on(float transition)
{
// #if IOT_DIMMER_MODULE_HAS_BUTTONS
//     // returns -1 for abort with false, 1 for abort with true and 0 for continue
//     int result = _offDelayPrecheck(DEFAULT_LEVEL);
//     if (result != 0) {
//         return result != -1;
//     }
// #endif
    if (!_brightness) {
        return _set(_storedBrightness, transition);
    }
    return false;
}

bool DimmerChannel::off(ConfigType *config, float transition, int32_t level)
{
// #if IOT_DIMMER_MODULE_HAS_BUTTONS
//     // returns -1 for abort with false, 1 for abort with true and 0 for continue
//     int result = _offDelayPrecheck(0, config, level);
//     if (result != 0) {
//         return result != -1;
//     }
// #endif
    if (_brightness) {
        return _set(0, transition);
    }
    return false;
}

void DimmerChannel::setLevel(int32_t level, float transition)
{
// #if IOT_DIMMER_MODULE_HAS_BUTTONS
//     _offDelayPrecheck(level, nullptr);
// #endif
    if (level == 0) {
        setStoredBrightness(_brightness);
        _set(0, transition);
    }
    else {
        _set(level, transition);
    }
}

bool DimmerChannel::_set(int32_t level, float transition)
{
    if (level == 0) {
        if (_brightness != 0) {
            auto tmp = _brightness;
            setStoredBrightness(_brightness);
            _brightness = 0;
            _dimmer->_fade(_channel, 0, _dimmer->getTransitionTime(tmp, _brightness, transition));
            _dimmer->_wire.writeEEPROM();
            publishState();
            return true;
        }
    }
    else if (level != _brightness) {
        auto tmp = _brightness;
        _brightness = std::clamp<int32_t>(level, _dimmer->_getConfig().min_brightness * MAX_LEVEL / 100, _dimmer->_getConfig().max_brightness * MAX_LEVEL / 100);
        __DBG_printf("level=%u min=%u max=%u brightness=%u", level, _dimmer->_getConfig().min_brightness * MAX_LEVEL / 100, _dimmer->_getConfig().max_brightness * MAX_LEVEL / 100, _brightness);
        _dimmer->_fade(_channel, _brightness, _dimmer->getTransitionTime(tmp, _brightness, transition));
        _dimmer->_wire.writeEEPROM();
        publishState();
        return true;
    }
    return false;
}

#if IOT_DIMMER_MODULE_HAS_BUTTONS

int DimmerChannel::_offDelayPrecheck(int16_t level, ConfigType *config, int16_t storeLevel)
{
    __LDBG_printf("timer=%p level=%d config=%p store=%d", _delayTimer, level, config, storeLevel);
    if (_delayTimer) {
        // restore brightness
        (*_delayTimer)->_callback(nullptr);
        delete _delayTimer;
        _delayTimer = nullptr;

        if (level == 0) {
            // timer was running, turn off now
            return off(nullptr) ? -1 : 1;
        }
    }
    if (level != 0 || config == nullptr) {
        // continue
        return 0;
    }
    if (_brightness == 0 || config->off_delay == 0) {
        // no delay, continue
        return 0;
    }

    __LDBG_printf("off_delay=%d signal=%d", config->off_delay, config->off_delay_signal);
    _delayTimer = new Event::Timer();

    if (config->off_delay >= 5 && config->off_delay_signal) {
        auto delay = config->off_delay - 4;
        int16_t brightness = storeLevel + (storeLevel * 100 / MAX_LEVEL > 70 ? MAX_LEVEL * -0.3 : MAX_LEVEL * 0.3);
        // flash once to signal confirmation
        _Timer(_delayTimer)->add(Event::milliseconds(1900), true, [this, delay, brightness, storeLevel](Event::CallbackTimerPtr timer) {
            if (timer == nullptr) {
                __LDBG_printf("timer=%p fade_to=%d", timer, storeLevel);
                _dimmer->_fade(_channel, storeLevel, 0.9);
                return;
            }
            auto interval = timer->getShortInterval();
            if (interval == 1900) {
                __LDBG_printf("timer=%p fade_to=%d reschedule=%d", timer, brightness, Event::milliseconds(2100).count());
                // first callback, dim up or down and reschedule
                _dimmer->_fade(_channel, brightness, 0.9);
                timer->updateInterval(Event::milliseconds(2100));
            }
            else if (interval == 2100) {
                __LDBG_printf("timer=%p fade_to=%d reschedule=%d", timer, brightness, Event::seconds(delay).count());
                // second callback, dim back and wait
                _dimmer->_fade(_channel, storeLevel, 0.9);
                timer->updateInterval(Event::seconds(delay));
            }
            else {
                // third callback
                __LDBG_printf("timer=%p store_level=%d off=%u", timer, storeLevel, true);
                off(nullptr, storeLevel);
                // the timer has been deleted at this point
                // make sure to exit
                return;
            }
        }, Event::PriorityType::HIGHEST);
    }
    else {
        _Timer(_delayTimer)->add(Event::seconds(config->off_delay), false, [this, storeLevel](Event::CallbackTimerPtr timer) {
            __LDBG_printf("timer=%p store_level=%d off=%u", timer, storeLevel, true);
            off(nullptr, storeLevel);
        }, Event::PriorityType::HIGHEST);
        // abort command
    }
    return 1;
}


void DimmerChannel::_publishMQTT()
{
    MQTTClient::safePublish(_createTopics(TopicType::COMMAND_STATE), true, PrintString(F("{\"state\":\"%s\",\"brightness\":%u,\"transition\":%.2f}"), MQTTClient::toBoolOnOff(_brightness), _brightness, _dimmer->_getConfig().lp_fadetime));
}

void DimmerChannel::_publishWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
    //     F("{\"type\":\"ue\",\"events\":[{\"id\":\"d_chan%u\",\"value\":%d,\"state\":true},{\"id\":\"group-switch-0\",\"value\":%u,\"state\":true}]}"),

        auto json = PrintString(F("{\"type\":\"ue\",\"events\":[{\"id\":\"d_chan%u\",\"value\":%u,\"state\":true,\"group\":%u}]}"),
            _channel, _brightness, _dimmer->isAnyOn()
        );

        __DBG_printf("%s", json.c_str());

         WsClient::broadcast(WebUISocket::getServerSocket(), WebUISocket::getSender(), json.c_str(), json.length());
        // WebUISocket::broadcast(WebUISocket::getSender(), json);
    }

}

void DimmerChannel::_publish()
{
    __DBG_printf("publish");
    _publishMQTT();
    _publishWebUI();
    _publishLastTime = millis();
}

void DimmerChannel::publishState()
{
    if (_publishTimer) {
        __DBG_printf("timer running");
        return;
    }
    auto diff = get_time_diff(_publishLastTime, millis());
    if (diff < 250 - 10) {
        __DBG_printf("starting timer %u", 250 - diff);
        _Timer(_publishTimer).add(Event::milliseconds(250 - diff), false, [this](Event::CallbackTimerPtr) {
            _publish();
        });
    }
    else {
        _publish();
    }



    // if (publishFlag == 0) { // no updates
    //     return;
    // }
    // else if (publishFlag == kStartTimerFlag) { // start timer

    //     _publishFlag |= kWebUIUpdateFlag|kMQTTUpdateFlag;
    //     if (_publishTimer) { // timer already running
    //         return;
    //     }
    //     // start timer to limit update rate
    //     _mqttCounter = 0;
    //     _Timer(_publishTimer).add(Event::milliseconds(kWebUIMaxUpdateRate), true, [this](Event::CallbackTimerPtr timer) {
    //         auto flag = _publishFlag;
    //         if (flag == 0) { // no updates, end timer
    //             timer->disarm();
    //             return;
    //         }
    //         if (_mqttCounter < kMQTTUpdateRateMultiplier) {
    //             // once the counter reached its max. it is reset by the publishState() method
    //             _mqttCounter++;
    //             // remove MQTT flag
    //             flag &= ~kMQTTUpdateFlag;
    //         }
    //         publishState(nullptr, flag);
    //     });
    //     return;
    // }

    // if (publishFlag & kMQTTUpdateFlag) {
    //     MQTTClient::safePublish(_createTopics(TopicType::COMMAND_STATE), true, PrintString(F("{\"state\":\"%s\",\"brightness\":%u}"), MQTTClient::toBoolOnOff(_brightness), _brightness));
    //     _publishFlag &= ~kMQTTUpdateFlag;
    //     _mqttCounter = 0;
    // }

    // if (publishFlag & kWebUIUpdateFlag) {
    //     _publishFlag &= ~kWebUIUpdateFlag;
    // }
}

#endif

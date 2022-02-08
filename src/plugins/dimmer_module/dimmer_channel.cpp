/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_channel.h"
#include "dimmer_module.h"
#include <WebUISocket.h>
#include <stl_ext/algorithm.h>
#include <stl_ext/utility.h>
#include <../src/plugins/mqtt/mqtt_client.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Dimmer;

Channel::Channel(Module *dimmer, uint8_t channel) :
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

MQTT::AutoDiscovery::EntityPtr Channel::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new AutoDiscovery::Entity();
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(num) {
        case 0:
            if (discovery->createJsonSchema(this, PrintString(FSPGM(channel__u), _channel), format)) {
                discovery->addStateTopic(_createTopics(TopicType::COMMAND_STATE));
                discovery->addCommandTopic(_createTopics(TopicType::COMMAND_SET));
                discovery->addBrightnessScale(getMaxLevel());
                discovery->addParameter(F("brightness"), true);
                String fullname;
                auto name = KFCConfigurationClasses::Plugins::DimmerConfigNS::Dimmer::getChannelName(_channel);
                if (*name) {
                    fullname = KFCConfigurationClasses::System::Device::getName();
                    fullname += ' ';
                    fullname += name;
                }
                else {
                    #if IOT_DIMMER_MODULE_CHANNELS > 1
                        fullname = PrintString(F("Dimmer Channel %u"), _channel + 1);
                    #else
                        fullname = F("Dimmer");
                    #endif
                }
                discovery->addName(fullname);
                discovery->addObjectId(baseTopic + MQTT::Client::filterString(fullname, true));
            }
            break;
    }
    return discovery;
}

uint8_t Channel::getAutoDiscoveryCount() const
{
    return 1;
}

String Channel::_createTopics(TopicType type, bool full) const
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
            return MQTT::Client::formatTopic(VALUE FSPGM(_set));
        case TopicType::COMMAND_STATE:
            if (!full) {
                return VALUEP FSPGM(_state);
            }
            return MQTT::Client::formatTopic(VALUE FSPGM(_state));
        default:
            break;
    }
    return String();
}

void Channel::onConnect()
{
    subscribe(_createTopics(TopicType::COMMAND_SET));
    _publishLastTime = 0;
    _publishTimer.remove();
    publishState();
}

void Channel::onMessage(const char *topic, const char *payload, size_t len)
{
    __LDBG_printf("topic=%s payload=%s", topic, payload);
    if (strcmp_end_P(topic, _topic.c_str())) {
        return;
    }
    auto stream = HeapStream(payload, len);
    auto reader = MQTT::Json::Reader(&stream);
    if (reader.parse()) {
        #if DEBUG_IOT_DIMMER_MODULE || 1
            PrintString str;
            reader.dump(str);
            __DBG_printf("%s", str.c_str());
        #endif
        onJsonMessage(reader);
    }
    else {
        __DBG_printf("parsing json failed=%s payload=%s", reader.getLastErrorMessage().c_str(), payload);
    }
}

void Channel::onJsonMessage(const MQTT::Json::Reader &json)
{
    if (json.state != -1) {
        if (json.state && !_brightness) {
            on();
        }
        else if (!json.state && _brightness) {
            off();
        }
    }
    if (json.brightness != -1) {
        if (json.brightness == 0 && _brightness) {
            off();
        }
        else if (json.brightness) {
            _set(json.brightness);
        }
    }
}

bool Channel::on(float transition)
{
// #if IOT_DIMMER_MODULE_HAS_BUTTONS
//     // returns -1 for abort with false, 1 for abort with true and 0 for continue
//     int result = _offDelayPrecheck(DEFAULT_LEVEL);
//     if (result != 0) {
//         return result != -1;
//     }
// #endif
    if (!_brightness) {
        if (!_storedBrightness)  {
            _storedBrightness = kDefaultLevel;
        }
        return _set(_storedBrightness, transition);
    }
    return false;
}

bool Channel::off(ConfigType *config, float transition, int32_t level)
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

void Channel::setLevel(int32_t level, float transition, bool publish)
{
// #if IOT_DIMMER_MODULE_HAS_BUTTONS
//     _offDelayPrecheck(level, nullptr);
// #endif
    if (level == 0) {
        //setStoredBrightness(_brightness);
        _set(0, transition, publish);
    }
    else {
        _set(level, transition, publish);
    }
    #if IOT_DIMMER_HAS_COLOR_TEMP
        _dimmer->_color._channelsToBrightness();
    #endif
    #if IOT_DIMMER_HAS_RGB
    #endif
}

void Channel::stopFading()
{
    _dimmer->_stopFading(_channel);
}


bool Channel::_set(int32_t level, float transition, bool publish)
{
    __LDBG_printf("set=%d transition=%f", level, transition);
    if (level == 0) {
        if (_brightness != 0) {
            auto tmp = _brightness;
            setStoredBrightness(_brightness);
            _brightness = 0;
            _dimmer->_fade(_channel, 0, _dimmer->getTransitionTime(tmp, _brightness, transition));
            _dimmer->_wire.writeEEPROM();
            if (publish) {
                publishState();
            }
            return true;
        }
    }
    else if (level != _brightness) {
        auto &cfg = _dimmer->_getConfig()._base;
        auto tmp = _brightness;
        _brightness = std::clamp<int32_t>(level, cfg.min_brightness * getMaxLevel() / 100, cfg.max_brightness * getMaxLevel() / 100);
        __LDBG_printf("level=%u min=%u max=%u brightness=%u", level, cfg.min_brightness * getMaxLevel() / 100, cfg.max_brightness * getMaxLevel() / 100, _brightness);
        _dimmer->_fade(_channel, _brightness, _dimmer->getTransitionTime(tmp, _brightness, transition));
        _dimmer->_wire.writeEEPROM();
        if (publish) {
            publishState();
        }
        return true;
    }
    return false;
}

#if IOT_DIMMER_MODULE_HAS_BUTTONS

int Channel::_offDelayPrecheck(int16_t level, ConfigType *configPtr, int16_t storeLevel)
{
    __LDBG_printf("timer=%p level=%d config=%p store=%d", _delayTimer, level, configPtr, storeLevel);
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
    if (level != 0 || configPtr == nullptr) {
        // continue
        return 0;
    }
    auto &config = configPtr->_base;
    if (_brightness == 0 || config.off_delay == 0) {
        // no delay, continue
        return 0;
    }

    __LDBG_printf("off_delay=%d signal=%d", config.off_delay, config.off_delay_signal);
    _delayTimer = new Event::Timer();

    if (config.off_delay >= 5 && config.off_delay_signal) {
        auto delay = config.off_delay - 4;
        int16_t brightness = storeLevel + (storeLevel * 100 / getMaxLevel() > 70 ? getMaxLevel() * -0.3 : getMaxLevel() * 0.3);
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
        _Timer(_delayTimer)->add(Event::seconds(config.off_delay), false, [this, storeLevel](Event::CallbackTimerPtr timer) {
            __LDBG_printf("timer=%p store_level=%d off=%u", timer, storeLevel, true);
            off(nullptr, storeLevel);
        }, Event::PriorityType::HIGHEST);
        // abort command
    }
    return 1;
}

#endif

void Channel::_publishMQTT()
{
    if (isConnected()) {
        using namespace MQTT::Json;
        publish(_createTopics(TopicType::COMMAND_STATE), true, UnnamedObject(State(_brightness != 0), Brightness(_brightness), Transition(_dimmer->_getConfig()._base._fadetime())).toString());
    }
}

void Channel::_publishWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        __LDBG_printf("channel=%u brightness=%u", _channel, _brightness);
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(
            WebUINS::Values(PrintString(F("d-chan%u"), _channel), _brightness, true),
            WebUINS::Values(F("group-switch-0"), _dimmer->isAnyOnInt(), true)
        )));
    }
}

void Channel::_publish()
{
    __LDBG_printf("publish channel=%u", _channel);
    _publishMQTT();
    _publishWebUI();
    _publishLastTime = millis();
}

void Channel::publishState()
{
    if (_publishTimer) {
        // __LDBG_printf("timer running");
        return;
    }
    uint32_t diff = millis() - _publishLastTime;
    if (diff < 250 - 10) {
        __LDBG_printf("starting timer %u", 250 - diff);
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
    //     publish(_createTopics(TopicType::COMMAND_STATE), true, PrintString(F("{\"state\":\"%s\",\"brightness\":%u}"), MQTT::Client::toBoolOnOff(_brightness), _brightness));
    //     _publishFlag &= ~kMQTTUpdateFlag;
    //     _mqttCounter = 0;
    // }

    // if (publishFlag & kWebUIUpdateFlag) {
    //     _publishFlag &= ~kWebUIUpdateFlag;
    // }
}

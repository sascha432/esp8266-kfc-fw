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
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

using namespace Dimmer;

Channel::Channel(Module *dimmer, uint8_t channel) :
    MQTTComponent(ComponentType::LIGHT),
    _dimmer(dimmer),
    #if IOT_DIMMER_MODULE_HAS_BUTTONS
        _delayTimer(nullptr),
    #endif
    _storedBrightness(0),
    _brightness(0),
    _channel(channel)
{
}

MQTT::AutoDiscovery::EntityPtr Channel::getAutoDiscovery(FormatType format, uint8_t num)
{
    auto discovery = new MQTT::AutoDiscovery::Entity();
    __DBG_discovery_printf("num=%u/%u d=%p", num, getAutoDiscoveryCount(), discovery);
    auto baseTopic = MQTT::Client::getBaseTopicPrefix();
    switch(num) {
        case 0:
            if (discovery->createJsonSchema(this, PrintString(FSPGM(channel__u), _channel), format)) {
                discovery->addStateTopic(_createTopics(TopicType::COMMAND_STATE));
                discovery->addCommandTopic(_createTopics(TopicType::COMMAND_SET));
                discovery->addBrightnessScale(IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
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
    }
    __builtin_unreachable();
}

void Channel::onConnect()
{
    subscribe(_createTopics(TopicType::COMMAND_SET));
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
        #if DEBUG_IOT_DIMMER_MODULE
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
    __LDBG_printf("channel=%u state=%d brightness=%d", _channel, json.state, json.brightness);
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
   if (_brightness) {
        return _set(0, transition);
    }
    return false;
}

void Channel::stopFading()
{
    _dimmer->_stopFading(_channel);
}

bool Channel::_set(int32_t level, float transition)
{
    __LDBG_printf("lvl=%d trans=%f ch=%u", level, transition, _channel);
    if (level == 0) {
        // if channel is turned off, store previous brightness
        if (_brightness != 0) {
            auto prevBrightness = _brightness;
            setStoredBrightness(_brightness);
            _brightness = 0;
            _dimmer->_fade(_channel, 0, _dimmer->getTransitionTime(prevBrightness, _brightness, transition));
            _dimmer->_wire.writeEEPROM();
            _dimmer->publishChannelState(_channel);
            return true;
        }
    }
    else if (level != _brightness) {
        // brightness has changed, get min/max. levels and change brightness
        auto &cfg = _dimmer->_getConfig()._base;
        auto prevBrightness = _brightness;
        _brightness = std::clamp<int32_t>(level, cfg.min_brightness * IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 100, cfg.max_brightness * IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 100);
        __LDBG_printf("lvl=%u min=%u max=%u brightness=%u", level, cfg.min_brightness * IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 100, cfg.max_brightness * IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 100, _brightness);
        _dimmer->_fade(_channel, _brightness, _dimmer->getTransitionTime(prevBrightness, _brightness, transition));
        _dimmer->_wire.writeEEPROM();
        _dimmer->publishChannelState(_channel);
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
            int16_t brightness = storeLevel + (storeLevel * 100 / IOT_DIMMER_MODULE_MAX_BRIGHTNESS > 70 ? IOT_DIMMER_MODULE_MAX_BRIGHTNESS * -0.3 : IOT_DIMMER_MODULE_MAX_BRIGHTNESS * 0.3);
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
        publish(_createTopics(TopicType::COMMAND_STATE), true, UnnamedObject(
            State(_brightness != 0),
            Brightness(_brightness),
            Transition(_dimmer->_getConfig()._base._getFadeTime())).toString()
        );
    }
}

void Channel::_publishWebUI()
{
    if (WebUISocket::hasAuthenticatedClients()) {
        __LDBG_printf("channel=%u brightness=%u", _channel, _brightness);
        WebUISocket::broadcast(WebUISocket::getSender(), WebUINS::UpdateEvents(WebUINS::Events(
            WebUINS::Values(PrintString(F("d-ch%u"), _channel), _brightness, true),
            WebUINS::Values(F("group-switch-0"), _dimmer->isAnyOnInt(), true)
        )));
    }
}

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
                #if IOT_ATOMIC_SUN_V2
                    discovery->addName(_dimmer->getChannelName(_channel));
                #else
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
                #endif
                discovery->addObjectId(baseTopic + PrintString(F("dimmer_channel_%u"), _channel + 1));
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
            _dimmer->setChannel(_channel, json.brightness);
        }
    }
}

bool Channel::on(float transition)
{
    // if (!_storedBrightness)  {
    //     _storedBrightness = kDefaultLevel;
    // }
    return _dimmer->setChannel(_channel, _storedBrightness, transition);
}

bool Channel::off(ConfigType *config, float transition)
{
    return _dimmer->setChannel(_channel, 0, transition);
}

void Channel::stopFading()
{
    _dimmer->_stopFading(_channel);
}

bool Channel::_set(int32_t level, float transition)
{
    __LDBG_printf("lvl=%d trans=%f ch=%u", level, transition, _channel);
    bool updateDimmer = false;
    auto prevBrightness = _brightness;
    if (level == 0) {
        // if channel is turned off, store previous brightness
        if (_brightness != 0) {
            setStoredBrightness(_brightness);
        }
        _brightness = 0;
        updateDimmer = true;
    }
    else if (level != _brightness) {
        auto &cfg = _dimmer->_getConfig()._base;
        _brightness = std::clamp<int32_t>(level, cfg.min_brightness * IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 100, cfg.max_brightness * IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 100);
        updateDimmer = true;
        __LDBG_printf("lvl=%u min=%u max=%u brightness=%u", level, cfg.min_brightness * IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 100, cfg.max_brightness * IOT_DIMMER_MODULE_MAX_BRIGHTNESS / 100, _brightness);
    }
    if (updateDimmer && transition != -1) {
        _dimmer->_fade(_channel, _brightness, _dimmer->getTransitionTime(prevBrightness, _brightness, transition));
        _dimmer->_wire.writeEEPROM();
        _dimmer->publishChannelState(_channel);
        return true;
    }
    return false;
}

void Channel::_publishMQTT()
{
    if (isConnected()) {
        using namespace MQTT::Json;
        publish(_createTopics(TopicType::COMMAND_STATE), true, UnnamedObject(
                State(_brightness != 0),
                Brightness(_brightness),
                Transition(_dimmer->_getConfig()._base._getFadeTime())
            ).toString()
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

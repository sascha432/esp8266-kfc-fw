/**
 * Author: sascha_lammers@gmx.de
 */

#include "blinds_ctrl.h"
#include "BlindsChannel.h"
#include "BlindsControl.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

BlindsChannel::BlindsChannel() : MQTTComponent(ComponentTypeEnum_t::SWITCH), _state(UNKNOWN), _channel(), _controller(nullptr)
{
}


MQTTComponent::MQTTAutoDiscoveryPtr BlindsChannel::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
            discovery->create(this, PrintString(FSPGM(channel__u, "channel_%u"), _number), format);
            discovery->addStateTopic(_getTopic(format, _number, TopicType::STATE));
            discovery->addCommandTopic(_getTopic(format, _number, TopicType::SET));
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
            break;
    }
    discovery->finalize();
    return discovery;
}

void BlindsChannel::onConnect(MQTTClient *client)
{
    client->subscribe(this, _getTopic(MQTTTopicType::TOPIC, _number, TopicType::SET), MQTTClient::getDefaultQos());
}

void BlindsChannel::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    auto state = atoi(payload); // payload is NUL terminated
    _debug_printf_P(PSTR("topic=%s, state=%u, controller=%p\n"), topic, state, _controller);
    _controller->setChannel(_number, state == 0 ? OPEN : CLOSED);
}

void BlindsChannel::_publishState(MQTTClient *client, uint8_t qos)
{
    client->publish(_getTopic(MQTTTopicType::TOPIC, _number, TopicType::STATE), qos, 1, _state == OPEN ? String(1) : String(0));
}

void BlindsChannel::setState(StateEnum_t state)
{
    _state = state;
}

BlindsChannel::StateEnum_t BlindsChannel::getState() const
{
    return _state;
}

bool BlindsChannel::isOpen() const
{
    return _state == OPEN;
}

bool BlindsChannel::isClosed() const
{
    return _state == CLOSED;
}

void BlindsChannel::setChannel(const Channel_t &channel)
{
    _channel = channel;
}

BlindsChannel::Channel_t &BlindsChannel::getChannel()
{
    return _channel;
}

void BlindsChannel::setNumber(uint8_t number)
{
    _number = number;
}

void BlindsChannel::setController(BlindsControl *controller)
{
    _controller = controller;
}

const __FlashStringHelper *BlindsChannel::_stateStr(StateEnum_t state)
{
    switch(state) {
        case OPEN:
            return F("Open");
        case CLOSED:
            return F("Closed");
        case STOPPED:
            return F("Stopped");
        default:
            break;
    }
    return FSPGM(n_a);
}

String BlindsChannel::_getTopic(MQTTTopicType topicType,  uint8_t channel, TopicType type) const
{
    const __FlashStringHelper *str;
    switch(type) {
        case TopicType::SET:
            str = FSPGM(_set, "/set");
            break;
        case TopicType::STATE:
        default:
            str = FSPGM(_state, "/state");
            break;
    }
    return MQTTClient::formatTopic(topicType, PrintString(FSPGM(channel__u, "channel_%u"), channel), str);
}

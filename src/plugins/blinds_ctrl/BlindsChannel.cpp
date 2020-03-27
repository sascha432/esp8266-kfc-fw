/**
 * Author: sascha_lammers@gmx.de
 */

#include "blinds_ctrl.h"
#include "BlindsChannel.h"
#include "BlindsControl.h"
#include "progmem_data.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

BlindsChannel::BlindsChannel() : MQTTComponent(SWITCH), _state(UNKNOWN), _channel(), _controller(nullptr)
{
}

void BlindsChannel::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector)
{
    _setTopic = MQTTClient::formatTopic(_number, F("/set"));
    _stateTopic = MQTTClient::formatTopic(_number, F("/state"));

    auto discovery = new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(_stateTopic);
    discovery->addCommandTopic(_setTopic);
    discovery->addPayloadOn(FSPGM(1));
    discovery->addPayloadOff(FSPGM(0));
    discovery->finalize();
    vector.emplace_back(discovery);
}

void BlindsChannel::onConnect(MQTTClient *client) {
    client->subscribe(this, _setTopic, MQTTClient::getDefaultQos());
}

void BlindsChannel::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    auto state = atoi(payload); // payload is NUL terminated
    _debug_printf_P(PSTR("topic=%s, state=%u, controller=%p\n"), topic, state, _controller);
    _controller->setChannel(_number, state == 0 ? OPEN : CLOSED);
}

void BlindsChannel::_publishState(MQTTClient *client, uint8_t qos)
{
    client->publish(_stateTopic, qos, 1, _state == OPEN ? FSPGM(1) : FSPGM(0));
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

void BlindsChannel::setNumber(uint8_t number) {
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
    return F("???");
}

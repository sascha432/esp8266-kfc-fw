/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_BLINDS_CTRL

#include "blinds_ctrl.h"
#include "BlindsChannel.h"
#include "BlindsControl.h"
#include "progmem_data.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

BlindsChannel::BlindsChannel() : MQTTComponent(SWITCH) {
    memset(&_channel, 0, sizeof(_channel));
    _state = UNKNOWN;
}

void BlindsChannel::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector) {

    _setTopic = MQTTClient::formatTopic(_number, F("/set"));
    _stateTopic = MQTTClient::formatTopic(_number, F("/state"));

    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, 0, format);
    discovery->addStateTopic(_stateTopic);
    discovery->addCommandTopic(_setTopic);
    discovery->addPayloadOn(FSPGM(1));
    discovery->addPayloadOff(FSPGM(0));
    discovery->finalize();
    vector.emplace_back(MQTTComponent::MQTTAutoDiscoveryPtr(discovery));
}

uint8_t BlindsChannel::getAutoDiscoveryCount() const {
    return 1;
}

void BlindsChannel::onConnect(MQTTClient *client) {
    client->subscribe(this, _setTopic, MQTTClient::getDefaultQos());
}

void BlindsChannel::onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {
    int state = atoi(payload);
    _debug_printf_P(PSTR("BlindsChannel::onMessage(): topic=%s, state=%u, payload=%*.*s, length=%u\n"), topic, state, len, len, payload, len);
    _controller->setChannel(_number, state == 0 ? OPEN : CLOSED);
}

void BlindsChannel::_publishState(MQTTClient *client, uint8_t qos) {
    client->publish(_stateTopic, qos, 1, _state == OPEN ? FSPGM(1) : FSPGM(0));
}

void BlindsChannel::setState(StateEnum_t state) {
    _state = state;
}

BlindsChannel::StateEnum_t BlindsChannel::getState() const {
    return _state;
}

bool BlindsChannel::isOpen() const {
    return _state == OPEN;
}

bool BlindsChannel::isClosed() const {
    return _state == CLOSED;
}

void BlindsChannel::setChannel(const Channel_t &channel) {
    _channel = channel;
}

BlindsChannel::Channel_t &BlindsChannel::getChannel() {
    return _channel;
}

void BlindsChannel::setNumber(uint8_t number) {
    _number = number;
}

void BlindsChannel::setController(BlindsControl *controller) {
    _controller = controller;
}

PGM_P BlindsChannel::_stateStr(StateEnum_t state) {
    switch(state) {
        case OPEN:
            return PSTR("Open");
        case CLOSED:
            return PSTR("Closed");
        case STOPPED:
            return PSTR("Stopped");
        default:
            return PSTR("???");
    }
}

#endif

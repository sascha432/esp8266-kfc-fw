/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE

#include "dimmer_channel.h"
#include "dimmer_module.h"
#include "progmem_data.h"
#include <PrintString.h>
#include "WebUISocket.h"

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DimmerChannel::DimmerChannel() : MQTTComponent(LIGHT) {
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("DimmerChannel(): component=%p\n"), this);
#endif
    memset(&_data, 0, sizeof(_data));
    _storedBrightness = 0;
}

void DimmerChannel::setup(Driver_DimmerModule *dimmer, uint8_t channel) {
    _dimmer = dimmer;
    _channel = channel;
}

void DimmerChannel::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTAutoDiscoveryVector &vector) {
    if (_data.state.state.length() == 0) {
        _createTopics();
    }
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, _channel, format);
    discovery->addStateTopic(_data.state.state);
    discovery->addCommandTopic(_data.state.set);
    discovery->addPayloadOn(FSPGM(1));
    discovery->addPayloadOff(FSPGM(0));
    discovery->addBrightnessStateTopic(_data.brightness.state);
    discovery->addBrightnessCommandTopic(_data.brightness.set);
    discovery->addBrightnessScale(IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
    discovery->finalize();
    vector.emplace_back(MQTTComponent::MQTTAutoDiscoveryPtr(discovery));
}

uint8_t DimmerChannel::getAutoDiscoveryCount() const {
    return 1;
}

void DimmerChannel::_createTopics() {

#if IOT_DIMMER_MODULE_CHANNELS > 1
    auto num = getNumber();
#else
    uint8_t num = 0xff;
#endif

    _data.state.set = MQTTClient::formatTopic(num, F("/set"));
    _data.state.state = MQTTClient::formatTopic(num, F("/state"));
    _data.brightness.set = MQTTClient::formatTopic(num, F("/brightness/set"));
    _data.brightness.state = MQTTClient::formatTopic(num, F("/brightness/state"));
}

void DimmerChannel::onConnect(MQTTClient *client) {

    uint8_t _qos = MQTTClient::getDefaultQos();

    _createTopics();

    _debug_printf_P(PSTR("DimmerChannel[%u]::subscribe(%s)\n"), _channel, _data.state.set.c_str());
    _debug_printf_P(PSTR("DimmerChannel[%u]::subscribe(%s)\n"), _channel, _data.brightness.set.c_str());

    client->subscribe(this, _data.state.set, _qos);
    client->subscribe(this, _data.brightness.set, _qos);

    publishState(client);
}

void DimmerChannel::onMessage(MQTTClient *client, char *topic, char *payload, size_t len) {
    _debug_printf_P(PSTR("DimmerChannel[%u]::onMessage(%s)\n"), _channel, topic);

    int value = atoi(payload);
    if (_data.state.set.equals(topic)) {

        // on/off only changes brightness if the state is different
        bool result;
        if (value) {
            result = on();
        } else {
            result = off();
        }
        if (!result) {
            publishState(client); // publish state again even if it has not been turned on or off
        }

    } else if (_data.brightness.set.equals(topic)) {

        float fadetime;

        // if brightness changes, also turn dimmer on or off
        if (value > 0) {
            fadetime = _data.state.value ? _dimmer->getFadeTime() : _dimmer->getOnOffFadeTime();
            _data.state.value = true;
            _data.brightness.value = std::min(value, IOT_DIMMER_MODULE_MAX_BRIGHTNESS);
        } else {
            fadetime = _data.state.value ? _dimmer->getOnOffFadeTime() : _dimmer->getFadeTime();
            _data.state.value = false;
            _data.brightness.value = 0;
        }
        _dimmer->_fade(_channel, _data.brightness.value, fadetime);
        publishState(client);
    }
}

bool DimmerChannel::on() {
    if (!_data.state.value) {
        _data.brightness.value = _storedBrightness;
        if (_data.brightness.value == 0) {
            _data.brightness.value = IOT_DIMMER_MODULE_MAX_BRIGHTNESS;
        }
        _data.state.value = true;
        _dimmer->_fade(_channel, _data.brightness.value, _dimmer->getOnOffFadeTime());
        _dimmer->writeEEPROM();

        publishState();
        return true;
    }
    return false;
}

bool DimmerChannel::off() {
    if (_data.state.value) {
        _storedBrightness = _data.brightness.value;
        _data.brightness.value = 0;
        _data.state.value = false;
        _dimmer->_fade(_channel, 0, _dimmer->getOnOffFadeTime());
        _dimmer->writeEEPROM();

        publishState();
        return true;
    }
    return false;
}

void DimmerChannel::publishState(MQTTClient *client) {
    if (!client) {
        client = MQTTClient::getClient();
    }
    if (client) {
        _debug_printf_P(PSTR("DimmerChannel[%u]::publishState(): brightness %d, state %u, client %p\n"), _channel, _data.brightness.value, _data.state.value, client);
        uint8_t _qos = MQTTClient::getDefaultQos();

        client->publish(_data.state.state, _qos, 1, String(_data.state.value));
        client->publish(_data.brightness.state, _qos, 1, String(_data.brightness.value));
    }

    JsonUnnamedObject json(2);
    json.add(JJ(type), JJ(ue));
    auto &events = json.addArray(JJ(events), 1);
    auto &obj = events.addObject(3);
    PrintString id(F("dimmer_channel%u"), _channel);
    obj.add(JJ(id), id);
    obj.add(JJ(value), _data.brightness.value);
    obj.add(JJ(state), _data.state.value);
    WsWebUISocket::broadcast(WsWebUISocket::getSender(), json);
}

#endif


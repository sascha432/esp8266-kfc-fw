/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_DIMMER_MODULE

#include "dimmer_channel.h"
#include "dimmer_module.h"
#include "dimmer_web_socket.h"
#include "progmem_data.h"
#include <PrintString.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DimmerChannel::DimmerChannel() {
    memset(&_data, 0, sizeof(_data));
    _storedBrightness = 0;
}

void DimmerChannel::setup(Driver_DimmerModule *dimmer, uint8_t channel) {
    _dimmer = dimmer;
    _channel = channel;
}

const String DimmerChannel::getName() {
    return F("Dimmer Module");
}
    
PGM_P DimmerChannel::getComponentName() {
    return SPGM(mqtt_component_light);
}

MQTTAutoDiscovery *DimmerChannel::createAutoDiscovery(MQTTAutoDiscovery::Format_t format) {
    if (_data.state.state.length() == 0) {
        _createTopics();
    }
    auto discovery = _debug_new MQTTAutoDiscovery();
    discovery->create(this, format);
    discovery->addParameter(FSPGM(mqtt_state_topic), _data.state.state);
    discovery->addParameter(FSPGM(mqtt_command_topic), _data.state.set);
    discovery->addParameter(FSPGM(mqtt_payload_on), FSPGM(1));
    discovery->addParameter(FSPGM(mqtt_payload_off), FSPGM(0));
    discovery->addParameter(FSPGM(mqtt_brightness_state_topic), _data.brightness.state);
    discovery->addParameter(FSPGM(mqtt_brightness_command_topic), _data.brightness.set);
    discovery->addParameter(FSPGM(mqtt_brightness_scale), String(IOT_DIMMER_MODULE_MAX_BRIGHTNESS));
    discovery->finalize();
    return discovery;
}

void DimmerChannel::_createTopics() {
    
    _data.state.set = MQTTClient::formatTopic(getNumber(), F("/set"));
    _data.state.state = MQTTClient::formatTopic(getNumber(), F("/state"));
    _data.brightness.set = MQTTClient::formatTopic(getNumber(), F("/brightness/set"));
    _data.brightness.state = MQTTClient::formatTopic(getNumber(), F("/brightness/state"));
}

void DimmerChannel::onConnect(MQTTClient *client) {

    uint8_t _qos = MQTTClient::getDefaultQos();

    _createTopics();

#if MQTT_AUTO_DISCOVERY
    if (MQTTAutoDiscovery::isEnabled()) {
        auto discovery = createAutoDiscovery(MQTTAutoDiscovery::FORMAT_JSON);
        _debug_printf_P(PSTR("DimmerChannel[%u]::createAutoDiscovery(): topic %s: %s\n"), _channel, discovery->getTopic().c_str(), discovery->getPayload().c_str());

        client->publish(discovery->getTopic(), _qos, true, discovery->getPayload());
        delete discovery;
    }
#endif

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
        _dimmer->_setChannel(_channel, _data.brightness.value, fadetime);
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
        _dimmer->_setChannel(_channel, _data.brightness.value, _dimmer->getOnOffFadeTime());

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
        _dimmer->_setChannel(_channel, 0, _dimmer->getOnOffFadeTime());

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
        _publishState(client);
    }
    PrintString message(F("+SET %u %d"), _channel, _data.brightness.value);
    WsDimmerClient::broadcast(message);
}

void DimmerChannel::_publishState(MQTTClient *client) {
    _debug_printf_P(PSTR("DimmerChannel[%u]::_publishState(): brightness %d, state %u\n"), _channel, _data.brightness.value, _data.state.value);

    uint8_t _qos = MQTTClient::getDefaultQos(); 

    client->publish(_data.state.state, _qos, 1, String(_data.state.value));
    client->publish(_data.brightness.state, _qos, 1, String(_data.brightness.value));
}

#endif

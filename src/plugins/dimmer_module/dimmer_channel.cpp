/**
 * Author: sascha_lammers@gmx.de
 */

#include "dimmer_channel.h"
#include "dimmer_module.h"
#include <WebUISocket.h>

#if DEBUG_IOT_DIMMER_MODULE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

DimmerChannel::DimmerChannel() : MQTTComponent(ComponentTypeEnum_t::LIGHT), _data(), _storedBrightness(0), _publishFlag(0)
{
#if DEBUG_MQTT_CLIENT
    debug_printf_P(PSTR("DimmerChannel(): component=%p\n"), this);
#endif
}

void DimmerChannel::setup(Driver_DimmerModule *dimmer, uint8_t channel)
{
    _dimmer = dimmer;
    _channel = channel;
}

MQTTComponent::MQTTAutoDiscoveryPtr DimmerChannel::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num > 0) {
        return nullptr;
    }
    auto discovery = __LDBG_new(MQTTAutoDiscovery);
    switch(num) {
        case 0:
            _createTopics();
            discovery->create(this, PrintString(FSPGM(channel__u), _channel), format);
            discovery->addStateTopic(_data.state.state);
            discovery->addCommandTopic(_data.state.set);
            discovery->addPayloadOn(1);
            discovery->addPayloadOff(0);
            discovery->addBrightnessStateTopic(_data.brightness.state);
            discovery->addBrightnessCommandTopic(_data.brightness.set);
            discovery->addBrightnessScale(MAX_LEVEL);
            break;
    }
    discovery->finalize();
    return discovery;
}

uint8_t DimmerChannel::getAutoDiscoveryCount() const
{
    return 1;
}

void DimmerChannel::_createTopics()
{
#if IOT_DIMMER_MODULE_CHANNELS > 1
    String value = PrintString(FSPGM(channel__u), _channel);
    #define VALUE value,
#else
    #define VALUE
#endif

    _data.state.set = MQTTClient::formatTopic(VALUE FSPGM(_set));
    _data.state.state = MQTTClient::formatTopic(VALUE FSPGM(_state));
    _data.brightness.set = MQTTClient::formatTopic(VALUE FSPGM(_brightness_set));
    _data.brightness.state = MQTTClient::formatTopic(VALUE FSPGM(_brightness_state));
}

void DimmerChannel::onConnect(MQTTClient *client)
{
    _createTopics();

    __LDBG_printf("DimmerChannel[%u]::subscribe(%s)", _channel, _data.state.set.c_str());
    __LDBG_printf("DimmerChannel[%u]::subscribe(%s)", _channel, _data.brightness.set.c_str());

    client->subscribe(this, _data.state.set);
    client->subscribe(this, _data.brightness.set);

    publishState(client, kUpdateAllFlag);
}

void DimmerChannel::onMessage(MQTTClient *client, char *topic, char *payload, size_t len)
{
    __LDBG_printf("DimmerChannel[%u]::onMessage(%s)", _channel, topic);

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
            publishState(client, kUpdateAllFlag); // publish state again even if it has not been turned on or off
        }

    } else if (_data.brightness.set.equals(topic)) {

        float fadetime;

        // if brightness changes, also turn dimmer on or off
        if (value > 0) {
            fadetime = _data.state.value ? _dimmer->getFadeTime() : _dimmer->getOnOffFadeTime();
            _data.state.value = true;
            _data.brightness.value = std::min(value, (int)MAX_LEVEL);
            setStoredBrightness(_data.brightness.value);
        } else {
            fadetime = _data.state.value ? _dimmer->getOnOffFadeTime() : _dimmer->getFadeTime();
            _data.state.value = false;
            _data.brightness.value = 0;
        }
        _dimmer->_fade(_channel, _data.brightness.value, fadetime);
        publishState(client, kUpdateAllFlag);
    }
}

bool DimmerChannel::on()
{
    if (!_data.state.value) {
        _data.brightness.value = _storedBrightness;
        if (_data.brightness.value <= MIN_LEVEL) {
            _data.brightness.value = DEFAULT_LEVEL;
        }
        _data.state.value = true;
        _dimmer->_fade(_channel, _data.brightness.value, _dimmer->getOnOffFadeTime());
        _dimmer->writeEEPROM();

        publishState();
        return true;
    }
    return false;
}

bool DimmerChannel::off()
{
    if (_data.state.value) {
        setStoredBrightness(_data.brightness.value);
        _data.brightness.value = 0;
        _data.state.value = false;
        _dimmer->_fade(_channel, 0, _dimmer->getOnOffFadeTime());
        _dimmer->writeEEPROM();

        publishState();
        return true;
    }
    return false;
}

void DimmerChannel::publishState(MQTTClient *client, uint8_t publishFlag)
{
    if (publishFlag == 0) { // no updates
        return;
    }
    else if (publishFlag == kStartTimerFlag) { // start timer

        _publishFlag |= kWebUIUpdateFlag|kMQTTUpdateFlag;
        if (_publishTimer) { // timer already running
            return;
        }
        // start timer to limit update rate
        _mqttCounter = 0;
        _Timer(_publishTimer).add(Event::milliseconds(kWebUIMaxUpdateRate), true, [this](Event::CallbackTimerPtr timer) {
            auto flag = _publishFlag;
            if (flag == 0) { // no updates, end timer
                timer->disarm();
                return;
            }
            if (_mqttCounter < kMQTTUpdateRateMultiplier) {
                // once the counter reached its max. it is reset by the publishState() method
                _mqttCounter++;
                // remove MQTT flag
                flag &= ~kMQTTUpdateFlag;
            }
            publishState(nullptr, flag);
        });
        return;
    }

    if (publishFlag & kMQTTUpdateFlag) {
        if (!client) {
            client = MQTTClient::getClient();
        }
        if (client && client->isConnected()) {

            // __LDBG_printf("channel=%u brightness=%d state=%u client=%p", _channel, _data.brightness.value, _data.state.value, client);

            client->publish(_data.state.state, true, String(_data.state.value));
            client->publish(_data.brightness.state, true, String(_data.brightness.value));
        }
        _publishFlag &= ~kMQTTUpdateFlag;
        _mqttCounter = 0;
    }

    if (publishFlag & kWebUIUpdateFlag) {
        auto webUi = WsWebUISocket::getWsWebUI();
        if (webUi && WsWebUISocket::hasClients(webUi)) {
            // JsonUnnamedObject json(2);
            // json.add(JJ(type), JJ(ue));
            // auto &events = json.addArray(JJ(events), 1);
            // auto &obj = events.addObject(3);
            // PrintString id(F("dimmer_channel%u"), _channel);
            // obj.add(JJ(id), id);
            // obj.add(JJ(value), _data.brightness.value);
            // obj.add(JJ(state), _data.state.value);

            static constexpr size_t bufSize = 88; // max json length ~81 byte
            auto buf = new uint8_t[bufSize];
            snprintf_P((char *)buf, bufSize - 1, PSTR("{\"type\":\"ue\",\"events\":[{\"id\":\"dimmer_channel%u\",\"value\":%d,\"state\":%s}]}"),
                _channel, _data.brightness.value, _data.state.value ? PSTR("true") : PSTR("false")
            );
            buf[bufSize - 1] = 0;
            WsWebUISocket::broadcast(WsWebUISocket::getSender(), buf, strlen((const char *)buf));
        }
        _publishFlag &= ~kWebUIUpdateFlag;
    }

}

bool DimmerChannel::getOnState() const
{
    return _data.state.value;
}

int16_t DimmerChannel::getLevel() const
{
    return _data.brightness.value;
}

void DimmerChannel::setLevel(int16_t level)
{
    _data.brightness.value = level;
    _data.state.value = (level != 0);
    setStoredBrightness(level);
}

void DimmerChannel::setStoredBrightness(uint16_t store)
{
    if (store > MIN_LEVEL) {
        _storedBrightness = store;
    }
}

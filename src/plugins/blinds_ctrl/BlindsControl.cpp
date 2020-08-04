/**
 * Author: sascha_lammers@gmx.de
 */

#include "WebUISocket.h"
#include "BlindsControl.h"
#include "blinds_ctrl.h"
#include "../src/plugins/mqtt/mqtt_client.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern int8_t operator *(const BlindsControl::ChannelType type);

int8_t operator *(const BlindsControl::ChannelType type) {
    return static_cast<int8_t>(type);
}

const std::array<uint8_t, BlindsControl::kChannelCount * 2> BlindsControl::_pins = { IOT_BLINDS_CTRL_M1_PIN, IOT_BLINDS_CTRL_M2_PIN, IOT_BLINDS_CTRL_M3_PIN, IOT_BLINDS_CTRL_M4_PIN };

PROGMEM_STRING_DEF(iot_blinds_control_state_file, "/.pvt/blinds_ctrl.state");

BlindsControl::BlindsControl() : MQTTComponent(ComponentTypeEnum_t::SENSOR), _activeChannel(ChannelType::NONE)
{
}

void BlindsControl::_setup()
{
    __LDBG_println();
    _readConfig();

    digitalWrite(IOT_BLINDS_CTRL_M1_PIN, LOW);
    digitalWrite(IOT_BLINDS_CTRL_M2_PIN, LOW);
    digitalWrite(IOT_BLINDS_CTRL_M3_PIN, LOW);
    digitalWrite(IOT_BLINDS_CTRL_M4_PIN, LOW);

    pinMode(IOT_BLINDS_CTRL_M1_PIN, OUTPUT);
    pinMode(IOT_BLINDS_CTRL_M2_PIN, OUTPUT);
    pinMode(IOT_BLINDS_CTRL_M3_PIN, OUTPUT);
    pinMode(IOT_BLINDS_CTRL_M4_PIN, OUTPUT);
    pinMode(IOT_BLINDS_CTRL_RSSEL_PIN, OUTPUT);

    analogWriteFreq(IOT_BLINDS_CTRL_PWM_FREQ);

#if IOT_BLINDS_CTRL_RPM_PIN
    attachScheduledInterrupt(digitalPinToInterrupt(IOT_BLINDS_CTRL_RPM_PIN), BlindsControl::rpmIntCallback, RISING);
    pinMode(IOT_BLINDS_CTRL_RPM_PIN, INPUT);
#endif
}

MQTTComponent::MQTTAutoDiscoveryPtr BlindsControl::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    if (num < kChannelCount) {
        ChannelType channel = (ChannelType)(num - 3);
        discovery->create(this, PrintString(FSPGM(channel__u, "channel_%u"), channel), format);
        discovery->addStateTopic(_getTopic(channel, TopicType::STATE));
        discovery->addCommandTopic(_getTopic(channel, TopicType::SET));
        discovery->addPayloadOn(1);
        discovery->addPayloadOff(0);
    }
    else {
        discovery->create(MQTTComponent::ComponentType::SENSOR, FSPGM(state), format);
        discovery->addStateTopic(_getTopic(ChannelType::NONE, TopicType::METRICS));
    }
    discovery->finalize();
    return discovery;
}

void BlindsControl::getValues(JsonArray &array)
{
    JsonUnnamedObject *obj;

    for(const auto channel: _states.channels()) {

        String prefix = PrintString(FSPGM(channel__u), channel);

        obj = &array.addObject(3);
        obj->add(JJ(id), prefix + F("_state"));
        obj->add(JJ(value), _getStateStr(channel));
        obj->add(JJ(state), true);

        obj = &array.addObject(3);
        obj->add(JJ(id), prefix + F("_set"));
        obj->add(JJ(value), _states[channel].isOpen() ? 1 : 0);
        obj->add(JJ(state), true);

    }

}

void BlindsControl::onConnect(MQTTClient *client)
{
    __LDBG_printf("client=%p", client);
    _publishState(client);
    for(const auto channel: _states.channels()) {
        client->subscribe(this, _getTopic(channel, TopicType::SET));
    }
}

void BlindsControl::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    __LDBG_printf("id=%s hasValue=%u value=%s hasState=%u state=%u", id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        if (String_endsWith(id, PSTR("_set"))) {
            size_t channel = atoi(id.c_str() + id.indexOf('_') + 1);
            if (channel < _states.size()) {
                _states[channel] = value.toInt() ? StateType::CLOSED : StateType::OPEN;
            }
        }
    }
}

void BlindsControl::setChannel(ChannelType channel, StateType state)
{
    __LDBG_printf("channel=%u state=%u", channel, state);
    // perform action in _loopMethod
    _action.set(state, channel);
}

void BlindsControl::_loopMethod()
{
    if (_action.isSet()) {
        auto channel = _action.getChannel();
        if (_motorTimeout.isActive()) { // abort if running, otherwise the action is queued
            if (_activeChannel == channel) {
                _action.clear();
                _stop();
                _states[channel] = StateType::STOPPED;
                _publishState();
            }
        }
        else {
            auto state = _action.getState();
            _action.clear();
            _stop();
            _activeChannel = channel;
            auto &_channel = _config.channels[*channel];
            _setMotorSpeed(channel, _channel.pwm_value, state == StateType::OPEN);
            _states[channel].setToggleOpenClosed(state);
            _motorTimeout.set(state == StateType::CLOSED ? _channel.open_time : _channel.close_time);
            _publishState();
            _clearAdc();
        }
    }
    if (_motorTimeout.isActive()) {
        _updateAdc();
        auto &_channel = _config.channels[*_activeChannel];
        auto currentLimit = _channel.current_limit;
        if (_adcIntegral > currentLimit && millis() % 2 == _currentLimitCounter % 2) { // assuming this loop runs at least once per millisecond. measured ~250-270µs, should be save...
            if (++_currentLimitCounter > _channel.current_limit_time) {
                __LDBG_printf("current limit");
                _stop();
                _publishState();
                return;
            }
        }
        else if (_adcIntegral < currentLimit * 0.8) {   // reset current limit counter if it drops below 80%
            _currentLimitCounter = 0;
        }

#if IOT_BLINDS_CTRL_RPM_PIN
        if (_hasStalled()) {
            __LDBG_printf("stalled");
            _stop();
            return;
        }
#endif

        if (_motorTimeout.reached()) {
            __LDBG_printf("timeout");
            _stop();
            _publishState();
        }
    }
}


bool BlindsControl::isBusy(ChannelType channel) const
{
    return (_motorTimeout.isActive() && _activeChannel == channel);
}

bool BlindsControl::isBusyOrError(ChannelType channel) const
{
    auto state = _states[channel];
    return (!state.isOpen() && !state.isClosed()) || isBusy(channel);
}

BlindsControl::NameType BlindsControl::_getStateStr(ChannelType channel) const
{
    if (isBusy(channel)) {
        return FSPGM(Busy);
    }
    return _states[channel]._getFPStr();
}

void BlindsControl::_clearAdc()
{
    auto channel = *_activeChannel;
    if (_config.swap_channels) {
        channel++;
        __LDBG_printf("channels swapped");
    }
    channel %= kChannelCount;

    __LDBG_printf("rssel=%u", channel == 0 ? HIGH : LOW);

    // select shunt
    digitalWrite(IOT_BLINDS_CTRL_RSSEL_PIN, channel == 0 ? HIGH : LOW);
    delay(IOT_BLINDS_CTRL_RSSEL_WAIT);

    _adcIntegral = 0;
    _currentLimitCounter = 0;
    _currentTimer.start();

#if IOT_BLINDS_CTRL_RPM_PIN
    _rpmReset();
#endif
}

void BlindsControl::_updateAdc()
{
    uint16_t count = (_currentTimer.getTime() / IOT_BLINDS_CTRL_INT_TDIV);
    _currentTimer.start();
    _adcIntegral = ((_adcIntegral * count) + analogRead(A0)) / (count + 1);
}

void BlindsControl::_setMotorSpeed(ChannelType channelType, uint16_t speed, bool direction)
{
    auto channel = *channelType;

    if (_config.swap_channels) {
        channel++;
    }
    channel %= kChannelCount;

    if (Plugins::Blinds::getChannelDirection(_config, channel)) {
        direction = !direction;
    }

    uint8_t pin1 = _pins[(channel * kChannelCount) + (direction ? 0 : 1)];
    uint8_t pin2 = _pins[(channel * kChannelCount) + (direction ? 1 : 0)];

    __LDBG_printf("channel=%u speed=%u dir=%u swapped=%u inverted=%u pins=%u,%u", channel, speed, _config.swap_channels, Plugins::Blinds::getChannelDirection(_config, channel), pin1, pin2);

    analogWrite(pin1, speed);
    analogWrite(pin2, 0);
}

void BlindsControl::_stop()
{
    __LDBG_println();

    _motorTimeout.disable();
    analogWrite(IOT_BLINDS_CTRL_M1_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M2_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M3_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M4_PIN, 0);
}

String BlindsControl::_getTopic(ChannelType channel, TopicType type)
{
    if (type == TopicType::METRICS) {
        return MQTTClient::formatTopic(FSPGM(state));
    }
    const __FlashStringHelper *str;
    switch(type) {
        case TopicType::SET:
            str = FSPGM(_set);
            break;
        case TopicType::STATE:
        default:
            str = FSPGM(_state);
            break;
    }
    return MQTTClient::formatTopic(PrintString(FSPGM(channel__u), channel), str);
}

void BlindsControl::_publishState(MQTTClient *client)
{
    if (!client) {
        client = MQTTClient::getClient();
    }
    __LDBG_printf("state %s/%s, client %p", _getStateStr(ChannelType::CHANNEL0), _getStateStr(ChannelType::CHANNEL1), client);

    if (client) {
        JsonUnnamedObject metrics(kChannelCount + 2);
        String binaryState = F("0b");
        std::array<NameType, kChannelCount> states_str;
        auto &states = metrics.addArray(FSPGM(channels), kChannelCount);
        bool busyOrError = false;

        for(const auto channel: _states.channels()) {
            auto &state = _states[channel];
            auto isOpen = state.getCharState();
            if (isBusyOrError(channel)) {
                busyOrError = true;
            }
            binaryState += isOpen;
            client->publish(_getTopic(channel, TopicType::STATE), true, String(isOpen));
            states_str[*channel] = _states[channel]._getFPStr();
            states.add(states_str[*channel]);

        }
        metrics.add(FSPGM(states), implode(',', states_str));
        metrics.add(FSPGM(binary), binaryState);
        metrics.add(F("busy_or_error"), busyOrError);
    }

    JsonUnnamedObject webUI(2);
    webUI.add(JJ(type), JJ(ue));
    getValues(webUI.addArray(JJ(events), kChannelCount * 2));
    WsWebUISocket::broadcast(WsWebUISocket::getSender(), webUI);

    _saveState();
}

void BlindsControl::_loadState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = SPIFFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
    if (file) {
        file.read(reinterpret_cast<uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

void BlindsControl::_saveState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    auto file = SPIFFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(_states.data()), _states.size() * sizeof(*_states.data()));
    }
    __LDBG_printf("file=%u state=%u,%u", (bool)file, _states[0], _states[1]);
#endif
}

void BlindsControl::_readConfig()
{
    _config = Plugins::Blinds::getConfig();
    _loadState();
}

#if IOT_BLINDS_CTRL_RPM_PIN

void BlindsControl::_rpmReset()
{
    _rpmTimer.start();
    _rpmLastInterrupt.start();
    _rpmCounter = 0;
}

void BlindsControl::_rpmIntCallback(const InterruptInfo &info)
{
    const uint8_t multiplier = IOT_BLINDS_CTRL_RPM_PULSES * 10;
    auto time = _rpmTimer.getTime(info.micro);
    _rpmLastInterrupt.start();
    _rpmTimer.start(info.micro);
    _rpmTimeIntegral = ((_rpmTimeIntegral * multiplier) + time) / (multiplier + 1);
    _rpmCounter++;
}

uint16_t BlindsControl::_getRpm()
{
    return (uint16_t)((1000000UL * 60UL / IOT_BLINDS_CTRL_RPM_PULSES) / _rpmTimeIntegral);
}

bool BlindsControl::_hasStalled()
{
    // interval for 500 rpm = ~40ms with 3 pulses
    return (_rpmLastInterrupt.getTime() > (1000000UL / 500UL/*rpm*/ * 60UL / IOT_BLINDS_CTRL_RPM_PULSES));
}

#endif

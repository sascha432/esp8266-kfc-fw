/**
 * Author: sascha_lammers@gmx.de
 */

#include "WebUISocket.h"
#include "BlindsControl.h"

#if DEBUG_IOT_BLINDS_CTRL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

PROGMEM_STRING_DEF(blinds_controller_channel1, "blinds_channel1");
PROGMEM_STRING_DEF(blinds_controller_channel2, "blinds_channel2");
PROGMEM_STRING_DEF(blinds_controller_channel1_sensor, "blinds_channel1_sensor");
PROGMEM_STRING_DEF(blinds_controller_channel2_sensor, "blinds_channel2_sensor");
PROGMEM_STRING_DEF(iot_blinds_control_state_file, "/blinds_ctrl.state");

BlindsControl::BlindsControl() : MQTTComponent(ComponentTypeEnum_t::SENSOR), WebUIInterface(), _activeChannel(0)
{
    _channels[0].setNumber(0);
    _channels[0].setController(this);
    _channels[1].setNumber(1);
    _channels[1].setController(this);
}

void BlindsControl::_setup()
{
    _debug_println();
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

    auto client = MQTTClient::getClient();
    if (client) {
        client->registerComponent(&_channels[0]);
        client->registerComponent(&_channels[1]);
        client->registerComponent(this);
    }
}

MQTTComponent::MQTTAutoDiscoveryPtr BlindsControl::nextAutoDiscovery(MQTTAutoDiscovery::FormatType format, uint8_t num)
{
    if (num >= getAutoDiscoveryCount()) {
        return nullptr;
    }
    auto discovery = new MQTTAutoDiscovery();
    switch(num) {
        case 0:
        case 1:
            discovery->create(this, PrintString(F("metrics_%u"), num), format);
            discovery->addStateTopic(_getTopic(format, num));
            break;
    }
    discovery->finalize();
    return discovery;
}

void BlindsControl::getValues(JsonArray &array)
{
    JsonUnnamedObject *obj;

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(blinds_controller_channel1_sensor));
    obj->add(JJ(value), _getStateStr(0));
    obj->add(JJ(state), true);

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(blinds_controller_channel1));
    obj->add(JJ(value), _channels[0].isOpen() ? 1 : 0);
    obj->add(JJ(state), true);

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(blinds_controller_channel2_sensor));
    obj->add(JJ(value), _getStateStr(1));
    obj->add(JJ(state), true);

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(blinds_controller_channel2));
    obj->add(JJ(value), _channels[1].isOpen() ? 1 : 0);
    obj->add(JJ(state), true);
}

void BlindsControl::onConnect(MQTTClient *client)
{
    _debug_printf_P(PSTR("client=%p\n"), client);
    _readConfig();
    _publishState(client);
}

void BlindsControl::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState)
{
    auto name = PSTR("blinds_channel");
    _debug_printf_P(PSTR("id=%s hasValue=%u value=%s hasState=%u state=%u\n"), id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        if (String_startsWith(id, name)) {
            uint8_t channel = (id.charAt(strlen_P(name)) - '1') % 2;
            setChannel(channel, value.toInt() ? BlindsChannel::CLOSED : BlindsChannel::OPEN);
        }
    }
}

void BlindsControl::setChannel(uint8_t channel, BlindsChannel::StateEnum_t state)
{
    _debug_printf_P(PSTR("channel=%u state=%u\n"), channel, state);
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
                _channels[channel].setState(BlindsChannel::STOPPED);
                _publishState();
            }
        }
        else {
            auto state = _action.getState();
            _action.clear();
            _stop();
            _activeChannel = channel;
            auto &_channel = _channels[channel].getChannel();
            _setMotorSpeed(channel, _channel.pwmValue, state == BlindsChannel::OPEN);
            _channels[channel].setState(state == BlindsChannel::OPEN ? BlindsChannel::CLOSED : BlindsChannel::OPEN);
            _motorTimeout.set(state == BlindsChannel::CLOSED ? _channel.openTime : _channel.closeTime);
            _publishState();
            _clearAdc();
        }
    }
    if (_motorTimeout.isActive()) {
        _updateAdc();
        auto &_channel = _channels[_activeChannel].getChannel();
        auto currentLimit = _channel.currentLimit;
        if (_adcIntegral > currentLimit && millis() % 2 == _currentLimitCounter % 2) { // assuming this loop runs at least once per millisecond. measured ~250-270µs, should be save...
            if (++_currentLimitCounter > _channel.currentLimitTime) {
                _debug_println(F("BlindsControl::_loopMethod(): Current limit"));
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
            _debug_println(F("BlindsControl::_loopMethod(): Stalled"));
            _stop();
            return;
        }
#endif

        if (_motorTimeout.reached()) {
            _debug_println(F("BlindsControl::_loopMethod(): Timeout"));
            _stop();
            _publishState();
        }
    }
}


const __FlashStringHelper *BlindsControl::_getStateStr(uint8_t channel) const
{
    if (_motorTimeout.isActive() && _activeChannel == channel) {
        return F("Busy");
    }
    return BlindsChannel::_stateStr(_channels[channel].getState());
}

void BlindsControl::_clearAdc()
{
    uint8_t channel = _activeChannel;
    if (_swapChannels) {
        channel++;
        _debug_printf_P(PSTR("BlindsControl::_clearAdc(): channels swapped\n"));
    }
    channel %= _channels.size();

    _debug_printf_P(PSTR("BlindsControl::_clearAdc(): rssel=%u\n"), channel == 0 ? HIGH : LOW);

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

void BlindsControl::_setMotorSpeed(uint8_t channel, uint16_t speed, bool direction)
{
    uint8_t pin1, pin2;

    if (_swapChannels) {
        channel++;
        _debug_printf_P(PSTR("BlindsControl::_setMotorSpeed(): channels swapped\n"));
    }
    channel %= _channels.size();

    if (channel == 0 && _channel0Dir) {
        _debug_printf_P(PSTR("BlindsControl::_setMotorSpeed(): direction inverted\n"));
        direction = !direction;
    }
    if (channel == 1 && _channel1Dir) {
        _debug_printf_P(PSTR("BlindsControl::_setMotorSpeed(): direction inverted\n"));
        direction = !direction;
    }

    if (channel == 0) {
        pin1 = IOT_BLINDS_CTRL_M1_PIN;
        pin2 = IOT_BLINDS_CTRL_M2_PIN;
    }
    else {
        pin1 = IOT_BLINDS_CTRL_M3_PIN;
        pin2 = IOT_BLINDS_CTRL_M4_PIN;
    }
    _debug_printf_P(PSTR("BlindsControl::_setMotorSpeed(%u, %u, %u): pins %u, %u\n"), channel, speed, direction, pin1, pin2);

    if (direction) {
        analogWrite(pin1, speed);
        analogWrite(pin2, 0);
    }
    else {
        analogWrite(pin1, 0);
        analogWrite(pin2, speed);
    }
}

void BlindsControl::_stop()
{
    _debug_println(F("BlindsControl::_stop()"));

    _motorTimeout.disable();
    analogWrite(IOT_BLINDS_CTRL_M1_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M2_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M3_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M4_PIN, 0);
}

String BlindsControl::_getTopic(MQTTTopicType type, uint8_t channel) const
{
    return MQTTClient::formatTopic(type, PrintString(FSPGM(channel__u, "channel_%u"), channel), F("/metrics"));
}

void BlindsControl::_publishState(MQTTClient *client)
{
    if (!client) {
        client = MQTTClient::getClient();
    }
    _debug_printf_P(PSTR("BlindsControl::_publishState(): state %s/%s, client %p\n"), _getStateStr(0), _getStateStr(1), client);

    if (client) {
        uint8_t _qos = MQTTClient::getDefaultQos();
        for(size_t i = 0; i < _channels.size(); i++) {
            client->publish(_getTopic(MQTTTopicType::TOPIC, i), _qos, 1, _getStateStr(i));
            _channels[i]._publishState(client, _qos);
        }
    }

    JsonUnnamedObject object(2);
    object.add(JJ(type), JJ(ue));
    auto &events = object.addArray(JJ(events), 4);
    getValues(events);
    WsWebUISocket::broadcast(WsWebUISocket::getSender(), object);

    _saveState();
}

void BlindsControl::_loadState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    BlindsChannel::StateEnum_t state[_channels.size()];
    memset(&state, 0, sizeof(state));
    auto file = SPIFFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::read);
    if (file) {
        file.read(reinterpret_cast<uint8_t *>(&state), sizeof(state));
        for(size_t i = 0; i < _channels.size(); i++) {
            _channels[i].setState(state[i]);
        }
    }
    _debug_printf_P(PSTR("file=%u state=%u,%u\n"), (bool)file, state[0], state[1]);
#endif
}

void BlindsControl::_saveState()
{
#if IOT_BLINDS_CTRL_SAVE_STATE
    BlindsChannel::StateEnum_t state[_channels.size()];
    for(size_t i = 0; i < _channels.size(); i++) {
        state[i] = _channels[i].getState();
    }
    auto file = SPIFFS.open(FSPGM(iot_blinds_control_state_file), fs::FileOpenMode::write);
    if (file) {
        file.write(reinterpret_cast<const uint8_t *>(&state), sizeof(state));
    }
    _debug_printf_P(PSTR("file=%u state=%u,%u\n"), (bool)file, state[0], state[1]);
#endif
}

void BlindsControl::_readConfig()
{
    auto cfg = config._H_GET(Config().blinds_controller);
    for(size_t i = 0; i < _channels.size(); i++) {
        _channels[i].setChannel(cfg.channels[i]);
    }
    _swapChannels = cfg.swap_channels;
    _channel0Dir = cfg.channel0_dir;
    _channel1Dir = cfg.channel1_dir;
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

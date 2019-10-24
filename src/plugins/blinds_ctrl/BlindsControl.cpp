/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_BLINDS_CTRL

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

BlindsControl::BlindsControl() : MQTTComponent(SWITCH), WebUIInterface()
{
    _activeChannel = 0;
    _state[0] = UNKNOWN;
    _state[1] = UNKNOWN;
    _readConfig();
}

void BlindsControl::createAutoDiscovery(MQTTAutoDiscovery::Format_t format, MQTTComponent::MQTTAutoDiscoveryVector &vector) {

    // String topic = MQTTClient::formatTopic(-1, F("/metrics/"));

    // auto discovery = _debug_new MQTTAutoDiscovery();
    // discovery->create(this, 0, format);
    // discovery->addStateTopic(topic + F("int_temp"));
    // discovery->addUnitOfMeasurement(F("\u00b0C"));
    // discovery->finalize();
    // vector.emplace_back(MQTTComponent::MQTTAutoDiscoveryPtr(discovery));

    // discovery = _debug_new MQTTAutoDiscovery();
    // discovery->create(this, 1, format);
    // discovery->addStateTopic(topic + F("ntc_temp"));
    // discovery->addUnitOfMeasurement(F("\u00b0C"));
    // discovery->finalize();
    // vector.emplace_back(MQTTComponent::MQTTAutoDiscoveryPtr(discovery));

    // discovery = _debug_new MQTTAutoDiscovery();
    // discovery->create(this, 2, format);
    // discovery->addStateTopic(topic + F("vcc"));
    // discovery->addUnitOfMeasurement(F("V"));
    // discovery->finalize();
    // vector.emplace_back(MQTTComponent::MQTTAutoDiscoveryPtr(discovery));

    // discovery = _debug_new MQTTAutoDiscovery();
    // discovery->create(this, 3, format);
    // discovery->addStateTopic(topic + F("frequency"));
    // discovery->addUnitOfMeasurement(F("Hz"));
    // discovery->finalize();
    // vector.emplace_back(MQTTComponent::MQTTAutoDiscoveryPtr(discovery));
}

uint8_t BlindsControl::getAutoDiscoveryCount() const {
    return 2;
}

void BlindsControl::getValues(JsonArray &array) {
    JsonUnnamedObject *obj;

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(blinds_controller_channel1_sensor));
    obj->add(JJ(value), _getStateStr(0));
    obj->add(JJ(state), true);

    obj = &array.addObject(2);
    obj->add(JJ(id), FSPGM(blinds_controller_channel1));
    obj->add(JJ(state), _state[0] == OPEN ? true : false);

    obj = &array.addObject(3);
    obj->add(JJ(id), FSPGM(blinds_controller_channel2_sensor));
    obj->add(JJ(value), _getStateStr(1));
    obj->add(JJ(state), true);

    obj = &array.addObject(2);
    obj->add(JJ(id), FSPGM(blinds_controller_channel2));
    obj->add(JJ(state), _state[1] == OPEN ? true : false);
}

void BlindsControl::setValue(const String &id, const String &value, bool hasValue, bool state, bool hasState) {
    _debug_printf_P(PSTR("BlindsControl::setValue(): %s value(%u) %s state(%u) %u\n"), id.c_str(), hasValue, value.c_str(), hasState, state);
    if (hasValue) {
        auto ptr = id.c_str();
        if (strncmp_P(ptr, PSTR("blinds_channel"), 14) == 0) {
            uint8_t channel = (ptr[14] - '1') % 2;
            setChannel(channel, value.toInt() ? CLOSED : OPEN);
        }
    }
}

void BlindsControl::setChannel(uint8_t channel, StateEnum_t state) {
    _debug_printf_P(PSTR("BlindsControl::setChannel(%u, %s): busy=%u\n"), channel, _stateStr(state), _motorTimeout.isActive());
    if (_motorTimeout.isActive() && _activeChannel == channel) {
        _stop();
    }
    else {
        _stop();
        _readConfig();
        _activeChannel = channel;
        _setMotorSpeed(channel, _channels[channel].pwmValue, state == OPEN);
        _state[channel] = state == OPEN ? CLOSED : OPEN;
        _motorTimeout.set(state == OPEN ? _channels[channel].openTime : _channels[channel].closeTime);
        _publishState();
        _clearAdc();
    }
}

void BlindsControl::_loopMethod() {
    if (_motorTimeout.isActive()) {
        _updateAdc();
        auto currentLimit = _channels[_activeChannel].currentLimit;
        if (_adcIntegral > currentLimit && millis() % 2 == _currentLimitCounter % 2) { // assuming this loop runs at least once per millisecond. measured ~250-270µs, should be save...
            if (++_currentLimitCounter > _channels[_activeChannel].currentLimitTime) {
                _debug_println(F("BlindsControl::_loopMethod(): Current limit"));
                _stop();
                _publishState();
                return;
            }
        }
        else if (_adcIntegral < currentLimit * 0.8) {   // reset current limit timer if it drops below 80%
            _currentLimitCounter = 0;
        }

        if (_motorTimeout.reached()) {
            _debug_println(F("BlindsControl::_loopMethod(): Timeout"));
            _stop();
            _publishState();
        }
    }
}

PGM_P BlindsControl::_stateStr(StateEnum_t state) {
    switch(state) {
        case OPEN:
            return PSTR("Open");
        case CLOSED:
            return PSTR("Closed");
        default:
            return PSTR("???");
    }
}

const __FlashStringHelper *BlindsControl::_getStateStr(uint8_t channel) const {
    _debug_printf_P(PSTR("BlindsControl::_getStateStr(%u): active=%u\n"), channel, _motorTimeout.isActive() && _activeChannel == channel);

    if (_motorTimeout.isActive() && _activeChannel == channel) {
        return F("Bu<b>sy</b>");
    }
    return FPSTR(_stateStr(_state[channel]));
}

void BlindsControl::_clearAdc() {
    _debug_printf_P(PSTR("BlindsControl::_clearAdc(): rssel=%u\n"), _activeChannel == 0 ? HIGH : LOW);

    // select shunt
    digitalWrite(IOT_BLINDS_CTRL_RSSEL_PIN, _activeChannel == 0 ? HIGH : LOW);
    delay(IOT_BLINDS_CTRL_RSSEL_WAIT);

    _adcIntegral = 0;
    _currentLimitCounter = 0;
    _currentTimer.start();
}

void BlindsControl::_updateAdc() {
    uint16_t count = (_currentTimer.getTime() / IOT_BLINDS_CTRL_INT_TDIV);
    _currentTimer.start();
    _adcIntegral = ((_adcIntegral * count) + analogRead(A0)) / (count + 1);
}

void BlindsControl::_setMotorSpeed(uint8_t channel, uint16_t speed, bool direction) {
    uint8_t pin1, pin2;
    if (channel == 0) {
        pin1 = IOT_BLINDS_CTRL_M1_PIN;
        pin2 = IOT_BLINDS_CTRL_M2_PIN;
    }
    else if (channel == 1) {
        pin1 = IOT_BLINDS_CTRL_M3_PIN;
        pin2 = IOT_BLINDS_CTRL_M4_PIN;
    }
    else {
        return;
    }
    _debug_printf_P(PSTR("BlindsControl::_setMotorSpeed(%u, %u, %u): pins %u, %u\n"), channel, speed, direction, pin1, pin2);

    if (direction) {
        // analogWrite(pin1, speed);
        // analogWrite(pin2, 0);
    }
    else {
        // analogWrite(pin1, 0);
        // analogWrite(pin2, speed);
    }
}

void BlindsControl::_stop() {
    _debug_println(F("BlindsControl::_stop()"));
    _motorTimeout.disable();
    analogWrite(IOT_BLINDS_CTRL_M1_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M2_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M3_PIN, 0);
    analogWrite(IOT_BLINDS_CTRL_M4_PIN, 0);
}

void BlindsControl::_publishState() {
    _debug_println(F("BlindsControl::_publishState()"));

    JsonUnnamedObject object(2);
    object.add(JJ(type), JJ(ue));
    auto &events = object.addArray(JJ(events), 4);
    getValues(events);
    WsWebUISocket::broadcast(WsWebUISocket::getSender(), object);
}

void BlindsControl::_readConfig() {
    _channels[0] = config._H_GET(Config().blinds_controller).channels[0];
    _channels[1] = config._H_GET(Config().blinds_controller).channels[1];
}

#endif

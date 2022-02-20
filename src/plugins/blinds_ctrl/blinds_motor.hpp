/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "BlindsControl.h"

inline void BlindsControl::_startMotor(ChannelType channel, bool open)
{
    // __LDBG_printf("channel=%u open=%u", channel, open);

    // disables all motor pins
    _stop();
    // set active channel
    _activeChannel = channel;

    auto &cfg = _config.channels[*channel];
    _currentLimit = cfg.current_limit_mA * BlindsControllerConversion::kConvertCurrentToADCValueMultiplier;

    _states[channel] = open ? StateType::OPEN : StateType::CLOSED;

    _publishState();

    _motorTimeout.set(open ? cfg.open_time_ms : cfg.close_time_ms);
    _adcIntegralMultiplier = cfg.current_avg_period_us / kAdcIntegralMultiplierDivider;
    // clear ADC last
    _clearAdc();

    if (_config.pwm_softstart_time > 0) {

        // soft start enabled
        _setMotorSpeed(channel, 0, open);

        // set timer, pwm and pin
        // the pwm value is updated in the loop function
        _motorStartTime = micros();
        if (_motorStartTime == 0) {
            _motorStartTime++;
        }
    }
    else {
        _setMotorSpeed(channel, cfg.pwm_value, open);
    }
    _motorPWMValue = cfg.pwm_value;
    #if DEBUG_IOT_BLINDS_CTRL
        _softStartUpdateCount = 0;
    #endif

    __LDBG_printf("pin=%u pwm=%u soft-start=%ums I-limit=%umA (%.2f) dir=%s adc-mul=%.6f timeout=%ums",
        _motorPin, _motorPWMValue, _config.pwm_softstart_time,
        cfg.current_limit_mA, _currentLimit,
        open ? PSTR("open") : PSTR("close"),
        _adcIntegralMultiplier,
        _motorTimeout.getTimeLeft()
    );
}

inline bool BlindsControl::_checkMotor()
{
    _updateAdc();
    if (_adcIntegral > _currentLimit) {
        __LDBG_printf("current limit time=%.2f @ %u ms", _adcIntegral, _motorTimeout.getDelay() - _motorTimeout.getTimeLeft());
        Logger_notice("Channel %u, current limit %umA (%.2f/%.2f) triggered after %ums (max. %ums)", _activeChannel, (uint32_t)(_adcIntegral * BlindsControllerConversion::kConvertADCValueToCurrentMultiplier), _adcIntegral, _currentLimit, _motorTimeout.getDelay() - _motorTimeout.getTimeLeft(), _motorTimeout.getDelay());
        return true;
    }
    if (_motorTimeout.reached()) {
        __LDBG_printf("timeout");
        Logger_notice("Channel %u, motor stopped after %ums timeout, peak current %umA (%.2f/%.2f)", _activeChannel, _motorTimeout.getDelay(), (uint32_t)(_adcIntegralPeak * BlindsControllerConversion::kConvertADCValueToCurrentMultiplier), _adcIntegralPeak, _currentLimit);
        return true;
    }
    return false;
}

inline void BlindsControl::_monitorMotor(ChannelAction &action)
{
    if (!_motorTimeout.isActive()) {
        __DBG_panic("motor timeout inactive");
    }
    if (_checkMotor()) {
        action.next();
        _stop();
        if (_cleanQueue()) {
            _saveState();
        }
        _publishState();
    }
}

inline void BlindsControl::_setMotorBrake(ChannelType channel)
{
    // shorts motor wires and brakes in slow decay mode
    // both pins low leaves motor wires floating -> _setMotorSpeed(channel, 0, ...)

    __LDBG_printf("channel=%u braking pins=%u,%u", channel, *(&_config.pins[(*channel * kChannelCount)]), *(&_config.pins[(*channel * kChannelCount)] + 1));

    _motorStartTime = 0;
    _motorPWMValue = 0;
    // make sure both pins are set to high at exactly the same time
    noInterrupts();
    auto ptr = &_config.pins[(*channel * kChannelCount)];
    digitalWrite(*ptr++, HIGH);
    digitalWrite(*ptr, HIGH);
    interrupts();
}

inline void BlindsControl::_setMotorSpeed(ChannelType channelType, uint16_t speed, bool open)
{
    auto channel = *channelType;
    _motorPin = _config.pins[(channel * kChannelCount) + !open]; // first pin is open pin
    uint8_t pin2 = _config.pins[(channel * kChannelCount) + open]; // second is close pin
    #if DEBUG_IOT_BLINDS_CTRL
        __LDBG_printf("channel=%u speed=%u open=%u pins=%u,%u frequency=%u", channel, speed, open, _motorPin, pin2, _config.pwm_frequency);
    #endif

    _motorStartTime = 0;
    analogWriteRange(PWMRANGE);
    analogWriteFreq(_config.pwm_frequency);

    // lock interrupts to avoid the driver going reverse for a few
    InterruptLock lock;
    if (speed == 0) {
        digitalWrite(_motorPin, LOW);
        _motorPWMValue = 0;
    }
    else {
        analogWrite(_motorPin, speed);
    }
    digitalWrite(pin2, LOW);
}

inline void BlindsControl::_setDac(uint16_t pwm)
{
    analogWriteRange(PWMRANGE);
    analogWriteFreq(kPwmMaxFrequency);
    analogWrite(_config.pins[kDACPin], pwm);
    __LDBG_printf("dac pin=%u pwm=%u frequency=%u", _config.pins[kDACPin], pwm, kPwmMaxFrequency);
}

inline void BlindsControl::_disableMotors()
{
    // do not allow interrupts when changing a pin pair from high/high to low/low and vice versa
    InterruptLock lock;
    for(uint8_t i = 0; i < 2 * kChannelCount; i++) {
        digitalWrite(_config.pins[i], LOW);
    }
}

inline void BlindsControl::_stop()
{
    _stopToneTimer();

    __LDBG_printf("pwm=%u pin=%u adc=%.2f peak=%.2f stopping motor", _motorPWMValue, _motorPin, _adcIntegral, _adcIntegralPeak);

    _motorStartTime = 0;
    _motorPWMValue = 0;
    _motorTimeout.disable();

    _activeChannel = ChannelType::NONE;
    _disableMotors();
    digitalWrite(_config.pins[kMultiplexerPin], LOW);
    digitalWrite(_config.pins[kDACPin], LOW);

    _adcIntegral = 0;
}

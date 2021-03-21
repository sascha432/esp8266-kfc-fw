/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#ifndef __DEEP_SLEEP_INSIDE_INCLUDE
#define __DEEP_SLEEP_INLINE__
#define __DEEP_SLEEP_INLINE_AWLAYS__
#define __DEEP_SLEEP_INLINE_INLINE_DEFINED__
#else
#define __DEEP_SLEEP_INLINE__ inline
#define __DEEP_SLEEP_INLINE_AWLAYS__ inline __attribute__((__always_inline__))
#define __DEEP_SLEEP_INLINE_INLINE_DEFINED__
#include "deep_sleep.h"
#endif

// ------------------------------------------------------------------------
// DeepSleep::PinState
// ------------------------------------------------------------------------

__DEEP_SLEEP_INLINE_AWLAYS__
DeepSleep::PinState::PinState() : _time(0), _state(0)
{
}

__DEEP_SLEEP_INLINE__
void DeepSleep::PinState::init()
{
    _time = micros();
#if DEBUG_PIN_STATE
    _states[0] = _readStates();
    _times[0] = micros();
    _count = 1;
    _setStates(_states[0]);
#else
    _setStates(_readStates());
#endif
}

__DEEP_SLEEP_INLINE__
void DeepSleep::PinState::merge()
{
#if DEBUG_PIN_STATE
    _states[_count] = _readStates();
    _times[_count] = micros();
    _mergeStates(_states[_count]);
    if (_count < sizeof(_states) / sizeof(_states[0]) - 1) {
        _count++;
    }
#else
    _mergeStates(_readStates());
#endif
}

__DEEP_SLEEP_INLINE_AWLAYS__
bool DeepSleep::PinState::isValid() const
{
    return _time != 0;
}

__DEEP_SLEEP_INLINE_AWLAYS__
bool DeepSleep::PinState::anyPressed() const
{
    return _state & kButtonMask;
}

__DEEP_SLEEP_INLINE_AWLAYS__
uint32_t DeepSleep::PinState::getStates() const
{
    return _state;
}

__DEEP_SLEEP_INLINE_AWLAYS__
uint32_t DeepSleep::PinState::getValues() const
{
    return getValues(getStates());
}

__DEEP_SLEEP_INLINE_AWLAYS__
uint32_t DeepSleep::PinState::getValues(uint32_t states) const
{
    return activeHigh() ? states : ~states;
}

__DEEP_SLEEP_INLINE_AWLAYS__
bool DeepSleep::PinState::getState(uint8_t pin) const
{
    return _state & _BV(pin);
}

__DEEP_SLEEP_INLINE_AWLAYS__
bool DeepSleep::PinState::getValue(uint8_t pin) const
{
    return (_state & _BV(pin)) == activeHigh();
}

__DEEP_SLEEP_INLINE_AWLAYS__
uint32_t DeepSleep::PinState::_readStates() const
{
    uint32_t state = GPI;
    if (GP16I & 0x01) {
        state |= _BV(16);
    }
    return activeHigh() ? state : ~state;
}

__DEEP_SLEEP_INLINE_AWLAYS__
void DeepSleep::PinState::_setStates(uint32_t state)
{
    _state = state;
}

__DEEP_SLEEP_INLINE_AWLAYS__
void DeepSleep::PinState::_mergeStates(uint32_t state) {
    _state |= state;
}

__DEEP_SLEEP_INLINE_AWLAYS__
uint32_t DeepSleep::PinState::getMillis() const {
    return _time / 1000;
}

__DEEP_SLEEP_INLINE_AWLAYS__
uint32_t DeepSleep::PinState::getMicros() const {
    return _time;
}

// ------------------------------------------------------------------------
// DeepSleepParam
// ------------------------------------------------------------------------


__DEEP_SLEEP_INLINE_AWLAYS__
DeepSleepParam::DeepSleepParam() :
    _totalSleepTime(0)
{
}

__DEEP_SLEEP_INLINE_AWLAYS__
DeepSleepParam::DeepSleepParam(WakeupMode wakeupMode) :
    _totalSleepTime(0),
    _wakeupMode(wakeupMode)
{
}

__DEEP_SLEEP_INLINE_AWLAYS__
DeepSleepParam::DeepSleepParam(minutes deepSleepTime, RFMode mode) :
DeepSleepParam(std::chrono::duration_cast<milliseconds>(deepSleepTime), mode)
{
}

__DEEP_SLEEP_INLINE_AWLAYS__
DeepSleepParam::DeepSleepParam(seconds deepSleepTime, RFMode mode) :
    DeepSleepParam(std::chrono::duration_cast<milliseconds>(deepSleepTime), mode)
{
}

__DEEP_SLEEP_INLINE_AWLAYS__
DeepSleepParam::DeepSleepParam(milliseconds deepSleepTime, RFMode rfMode) :
            _totalSleepTime(deepSleepTime.count()),
            _remainingSleepTime(deepSleepTime.count()),
            _currentSleepTime(0),
            _runtime(0),
            _counter(0),
            _rfMode(rfMode),
            _wakeupMode(WakeupMode::NONE)
{
}


__DEEP_SLEEP_INLINE_AWLAYS__
bool DeepSleepParam::isValid() const
{
    return _totalSleepTime != 0;
}

__DEEP_SLEEP_INLINE_AWLAYS__
uint32_t DeepSleepParam::getDeepSleepMaxMillis()
{
    return ESP.deepSleepMax() / (1000 + 150/*some extra margin*/);
}

// total sleep time in seconds
__DEEP_SLEEP_INLINE_AWLAYS__
float DeepSleepParam::getTotalTime() const
{
    return _totalSleepTime / 1000.0;
}

__DEEP_SLEEP_INLINE_AWLAYS__
void DeepSleepParam::updateRemainingTime()
{
    _currentSleepTime = _updateRemainingTime();
}

__DEEP_SLEEP_INLINE_AWLAYS__
uint64_t DeepSleepParam::getDeepSleepTimeMicros() const
{
    return _currentSleepTime * 1000ULL;
}

__DEEP_SLEEP_INLINE__
uint32_t DeepSleepParam::_updateRemainingTime()
{
    auto maxSleep = getDeepSleepMaxMillis();
    if (_remainingSleepTime / 2 >= maxSleep) {
        _remainingSleepTime -= maxSleep;
        return maxSleep;
    }
    else if (_remainingSleepTime >= maxSleep) {
        _remainingSleepTime /= 2;
        return _remainingSleepTime;
    }
    else {
        maxSleep = _remainingSleepTime;
        _remainingSleepTime = 0;
        return maxSleep;
    }
}

__DEEP_SLEEP_INLINE_AWLAYS__
WakeupMode DeepSleepParam::getWakeupMode() const
{
    return _wakeupMode;
}

__DEEP_SLEEP_INLINE_AWLAYS__
void DeepSleepParam::setRFMode(RFMode rfMode)
{
    _rfMode = rfMode;
}


#ifdef __DEEP_SLEEP_INLINE_INLINE_DEFINED__
#undef __DEEP_SLEEP_INLINE__
#undef __DEEP_SLEEP_INLINE_INLINE_DEFINED__
#ifdef __DEEP_SLEEP_NOINLINE__
#undef __DEEP_SLEEP_NOINLINE__
#endif
#endif

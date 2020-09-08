/**
 * Author: sascha_lammers@gmx.de
 */

#include <MicrosTimer.h>
#include "push_button.h"
#include "monitor.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

void PushButton::event(StateType state, TimeType now)
{
    __LDBG_printf("%s EVENT state=%s time=%u", name(), Monitor::stateType2String(state), now);
    if (state == StateType::DOWN) {

        _startTimer = now;
        _startTimerRunning = true;
        _repeatCount = 0;
        if (_clickRepeatCount == kClickRepeatCountMax) {
            _clickRepeatCount = 0;
            __LDBG_assert(_clickRepeatTimerRunning == false);
        }
        __LDBG_printf("%s PRESSED no_repeat_timer_running=%u no_repeat_count=%u", name(), _clickRepeatTimerRunning, _clickRepeatCount);
        _fireEvent(EventType::DOWN);
        return;

    }
    else if (_startTimerRunning == false) {

        // ignore up event without start timer running
        __LDBG_printf("%s ERROR timer=%u not running", name(), _startTimer);
        return;

    }
    _buttonReleased();
    _fireEvent(EventType::UP);
    _startTimerRunning = false;
}

void PushButton::loop()
{
    if (_startTimerRunning) {
        _duration = get_time_diff(_startTimer, millis());
        // between down and up
        if (_duration >= _longpressTime) {
            // start with 1...
            uint16_t repeatCount = 1 + ((_duration - _longpressTime) / _repeatTime);
            bool repeatChanged = repeatCount != _repeatCount;
            _repeatCount = repeatCount;
            if (repeatChanged) {
                __LDBG_printf("%s REPEAT_CHANGED count=%u duration=%u", name(), _repeatCount, _duration);
                _fireEvent(EventType::REPEAT);
            }
        }
    }
    else if (_clickRepeatTimerRunning) {
        // after up and next down
        uint16_t duration = get_time_diff(_clickRepeatTimer, millis());
        if (duration > _shortpressNoRepeatTime) {
            __LDBG_printf("%s no_repeat_time=%u duration=%u no_repeat_count=%u", name(), _shortpressNoRepeatTime, duration, _clickRepeatCount);

            if (!(
                (_clickRepeatCount == 1 && _fireEvent(EventType::SINGLE_CLICK)) ||
                (_clickRepeatCount == 2 && _fireEvent(EventType::DOUBLE_CLICK))
            )) {
                _fireEvent(EventType::REPEATED_CLICK);
            }
            _clickRepeatCount = kClickRepeatCountMax;
            _clickRepeatTimerRunning = false;
        }
    }
}

void PushButton::_buttonReleased()
{
    uint32_t ms = millis();
    _duration = get_time_diff(_startTimer, ms);
    _clickRepeatTimer = ms;
    _clickRepeatTimerRunning = true;
    if (_clickRepeatCount < kClickRepeatCountMax - 1) {
        _clickRepeatCount++;
    }
    __LDBG_printf("%s no_repeat_timer_running=%u no_repeat_count=%u", name(), _clickRepeatTimerRunning, _clickRepeatCount);

    if (_duration < _shortpressTime) {

        __LDBG_printf("%s CLICK=%u duration=%u", name(), _shortpressTime, _duration);
        _fireEvent(EventType::CLICK);
        return;

    }

    if (_duration < _longpressTime) {

        __LDBG_printf("%s LONG_CLICK=%u duration=%u", name(), _longpressTime, _duration);
        _fireEvent(EventType::LONG_CLICK);
        return;

    }
}

const __FlashStringHelper *PushButton::eventTypeToString(EventType eventType)
{
    switch(eventType) {
        case EventType::DOWN:
            return F("DOWN");
        case EventType::UP:
            return F("UP");
        case EventType::PRESSED:
            return F("PRESSED");
        case EventType::LONG_PRESSED:
            return F("LONG_PRESSED");
        case EventType::HELD:
            return F("HELD");
        case EventType::REPEAT:
            return F("REPEAT");
        case EventType::REPEATED_CLICK:
            return F("REPEATED_CLICK");
        case EventType::SINGLE_CLICK:
            return F("SINGLE_CLICK");
        case EventType::DOUBLE_CLICK:
            return F("DOUBLE_CLICK");
        case EventType::ANY:
            return F("ANY");
        default:
        case EventType::NONE:
            break;
    }
    return F("NONE");
}

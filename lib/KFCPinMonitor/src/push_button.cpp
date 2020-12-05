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

#if PIN_MONITOR_BUTTON_GROUPS
SingleClickGroupPtr::SingleClickGroupPtr(PushButton &button) : PtrType(button._singleClickGroup)
{
}
#endif

void PushButton::event(StateType state, uint32_t now)
{
    __LDBG_printf("%s EVENT state=%s time=%u", name(), Monitor::stateType2String(state), now);
    if (state == StateType::DOWN) {

        _startTimer = now;
        _startTimerRunning = true;
        _repeatCount = 0;

#if PIN_MONITOR_BUTTON_GROUPS
        _singleClickGroup->pressed();
        __LDBG_printf("%s PRESSED sp_timer=%u sp_count=%u", name(), _singleClickGroup->isTimerRunning(this), _singleClickGroup->getRepeatCount());
#else
        __LDBG_printf("%s PRESSED", name());
#endif

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

void PushButton::event(EventType eventType, uint32_t now)
{
    __LDBG_printf("PURE VIRTUAL: event_type=%u now=%u", eventType, now);
}

void PushButton::loop()
{
    if (_startTimerRunning && _repeatTime) {
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
#if PIN_MONITOR_BUTTON_GROUPS
    else if (_singleClickGroup->isTimerRunning(this)) {
        // after up and next down
        uint16_t duration = _singleClickGroup->getDuration();
        if (duration > _singleClickGroup->getTimeout()) {
            __LDBG_printf("%s duration=%u sp_timeout=%u sp_count=%u", name(), duration, _singleClickGroup->getTimeout(), _singleClickGroup->getRepeatCount());

            if (!(
                (_singleClickGroup->getRepeatCount() == 1 && _fireEvent(EventType::SINGLE_CLICK)) ||
                (_singleClickGroup->getRepeatCount() == 2 && _fireEvent(EventType::DOUBLE_CLICK))
            )) {
                _fireEvent(EventType::REPEATED_CLICK);
            }
            _singleClickGroup->stopTimer(this);
        }
    }
#endif
}

void PushButton::_buttonReleased()
{
    uint32_t ms = millis();
    _duration = get_time_diff(_startTimer, ms);

#if PIN_MONITOR_BUTTON_GROUPS
    _singleClickGroup->released(this, ms);
    __LDBG_printf("%s sp_timer=%u sp_count=%u", name(), _singleClickGroup->isTimerRunning(this), _singleClickGroup->getRepeatCount());
#endif

    if (_duration < _clickTime) {

        __LDBG_printf("%s CLICK=%u duration=%u repeat=%u", name(), _clickTime, _duration, _repeatCount);
        _fireEvent(EventType::CLICK);
        return;

    }
#if PIN_MONITOR_BUTTON_GROUPS
    // reset click counter and timer after _clickTime is over
    _singleClickGroup->stopTimer(this);
#endif

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

#if DEBUG
void PushButton::dumpConfig(Print &output)
{
    Pin::dumpConfig(output);

    output.print(F(" events="));
    for(int i = 0; i < (int)EventType::MAX_BITS; i++) {
        if (i > 0) {
            output.print('|');
        }
        output.print(eventTypeToString((EventType)((int)_subscribedEvents & (1 << i))));
    }

#if PIN_MONITOR_BUTTON_GROUPS
    output.printf_P(PSTR(" sp_time=%u lp_time=%u rep_time=%u steps=%u sp_group=%p(%u) timeout=%u"), _clickTime, _longpressTime, _repeatTime, _singleClickSteps, _singleClickGroup.get(), _singleClickGroup.use_count(), _singleClickGroup->getTimeout());
#else
    output.printf_P(PSTR(" sp_time=%u lp_time=%u rep_time=%u steps=%u"), _clickTime, _longpressTime, _repeatTime, _singleClickSteps);
#endif
}
#endif

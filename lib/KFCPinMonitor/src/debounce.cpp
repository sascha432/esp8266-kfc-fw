/**
  Author: sascha_lammers@gmx.de
*/

#include <MicrosTimer.h>
#include "debounce.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#endif

using namespace PinMonitor;

void Debounce::event(uint8_t value, uint32_t micros, uint32_t milliseconds)
{
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_DEBOUNCER
    uint32_t diff = _debounceTimer ? get_time_diff(_debounceTimer, milliseconds) : 0;
#endif

    // timer not active
    if (_debounceTimer == 0) {
        // update state and start timer
        _debounceTimer = milliseconds;
        _isRising = value;
        _isFalling = !value;
        _value = value;
        _state = value;
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_DEBOUNCER
        _bounceCounter = 0;
        __LDBG_printf("start value=%u rising=%u falling=%u", _isRising, _isFalling);
#endif
    }
    else if (_value != value) {
        // reset timer if state changed
        _debounceTimer = milliseconds;
        _value = value;
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_DEBOUNCER
        _bounceCounter++;
        __LDBG_printf("bounced value=%u time=%u bounced=%u", value, diff, _bounceCounter);
#endif
    }
    else if (get_time_diff(_debounceTimer, milliseconds) > _debounceTime) {
        // the state is stable for debounce time milliseconds
        _value = value;
        // store changed state
        _stateChanged = value ? 1 : -1;
        if (_state == value) {
            // we reached the expected state
            _debounceTimer = 0;
            // _isRising = false;
            // _isFalling = false;
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_DEBOUNCER
            __LDBG_printf("reached state=%u time=%u change=%d bounced=%u", _state, diff, _stateChanged, _bounceCounter);
#endif
        }
        else {
            // timeout, force new state
            _debounceTimer = milliseconds;
            _isRising = value;
            _isFalling = !value;
            _value = value;
            _state = value;
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_DEBOUNCER
            __LDBG_printf("timeout state=%u time=%u change=%d bounced=%u rising=%u falling=%u", _state, diff, _stateChanged, _bounceCounter, _isRising, _isFalling);
#endif
        }
    }
}

StateType Debounce::pop_state(uint32_t now)
{
    if (_debounceTimer) {
        event(_value, 0, now);
    }
    if (_isRising) {
        _isRising = false;
        return StateType::IS_RISING;
    }
    else if (_isFalling) {
        _isFalling = false;
        return StateType::IS_FALLING;
    }
    else if (_stateChanged == 1) {
        _stateChanged = 0;
        return StateType::IS_HIGH;
    }
    else if (_stateChanged == -1) {
        _stateChanged = 0;
        return StateType::IS_LOW;
    }
    return StateType::NONE;
}

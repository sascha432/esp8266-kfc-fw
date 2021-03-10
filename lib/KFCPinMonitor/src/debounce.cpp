/**
  Author: sascha_lammers@gmx.de
*/

#include <MicrosTimer.h>
#include "pin_monitor.h"
#include "debounce.h"
#include "monitor.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#endif

using namespace PinMonitor;

StateType Debounce::_debounce(bool lastValue, uint16_t interruptCount, uint32_t last, uint32_t now, uint32_t _micros)
{
    if (interruptCount) {

        // set start to time when we received the last pin change interrupt
        _debounceTimer = now - (get_time_diff(last, _micros) / 1000U);

        // debounce timer running?
        if (_debounceTimerRunning == false) {

            _debounceTimerRunning = true;

            // the expected start is the opposite of the stored value
            _state = !_value;
            // save last value read from the pin
            _value = lastValue;

#if DEBUG_PIN_MONITOR_EVENTS
            _bounceCounter = interruptCount;
            _startDebounce = _debounceTimer; // not exactly the start but close to it
            _debounceTime = 0;
#endif

            return _state ? StateType::IS_RISING : StateType::IS_FALLING;
        }

        // save last value
        _value = lastValue;
#if DEBUG_PIN_MONITOR_EVENTS
        _bounceCounter += interruptCount;
        _debounceTime = get_time_diff(_startDebounce, now);
#endif

        return StateType::NONE;
    }

    if (_debounceTimerRunning && get_time_diff(_debounceTimer, now) >= pinMonitor.getDebounceTime()) {

#if DEBUG_PIN_MONITOR_EVENTS
        _bounceCounter += interruptCount;
        _debounceTime = get_time_diff(_startDebounce, now);
#endif

        // we did not register any changes and the debounce timout has expired
        // remove timer
        _debounceTimerRunning = false;

        // save last value
        _value = lastValue;
        if (_value != _state) {

            // our state was not reached within debounce time
            return _state ? StateType::RISING_BOUNCED : StateType::FALLING_BOUNCED;
        }

        // __LDBG_printf("state=%u value=%u lastValue=%u diff=%u icount=%u diff=%u", _state, _value, lastValue, lastChangeTimeMillis, interruptCount,tmp);

        // true is rising, false = falling
        return _state ? StateType::IS_HIGH : StateType::IS_LOW;
    }

    return StateType::NONE;
}

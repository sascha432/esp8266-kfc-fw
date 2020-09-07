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

StateType Debounce::debounce(bool lastValue, uint16_t interruptCount, TimeType last, TimeType now)
{
    if (interruptCount) {

        // debounce timer running?
        bool timerNotRunning = (_debounceTimer == 0);

        // set start to time when we received the last pin change interrupt
        if (!(_debounceTimer = now - (get_time_diff(last, micros()) / 1000U))) {
            _debounceTimer++;
        }

        if (timerNotRunning) {

            // the expected start is the opposite of the stored value
            _state = !_value;
            // save last value read from the pin
            _value = lastValue;

#if DEBUG_PIN_MONITOR
            _bounceCounter = interruptCount;
            _startDebounce = _debounceTimer; // not exactly the start but close to it
            _debounceTime = 0;
#endif

            return _state ? StateType::IS_RISING : StateType::IS_FALLING;
        }

        // save last value
        _value = lastValue;
#if DEBUG_PIN_MONITOR
        _bounceCounter += interruptCount;
        _debounceTime = get_time_diff(_startDebounce, now);
#endif

        return StateType::NONE;
    }

    if (_debounceTimer != 0 && get_time_diff(_debounceTimer, now) >= pinMonitor.getDebounceTime()) {

#if DEBUG_PIN_MONITOR
        _bounceCounter += interruptCount;
        _debounceTime = get_time_diff(_startDebounce, now);
#endif

        // we did not register any changes and the timeout has expired
        // remove timer
        _debounceTimer = 0;
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

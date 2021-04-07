/**
  Author: sascha_lammers@gmx.de
*/

#include <MicrosTimer.h>
#include "pin_monitor.h"
#include "debounce.h"
#include "monitor.h"

#if DEBUG_PIN_MONITOR_DEBOUNCE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

StateType Debounce::_debounce(bool lastValue, uint16_t interruptCount, uint32_t last, uint32_t now)
{
    if (interruptCount) {

        // set start to time when we received the last pin change interrupt
        _debounceTimer = now - (get_time_diff(last, micros()) / 1000U);

        // debounce timer running?
        if (_debounceTimerRunning == false) {

            _debounceTimerRunning = true;

            // the expected start is the opposite of the stored value
            _state = !_value;
            // save last value read from the pin
            _value = lastValue;

            __LDBG_printf("interruptCount>0: debounce timer started=%u _state=%s _value=%u", _debounceTimer, _state ? PSTR("RISING") : PSTR("FALLING"), _value);

            return _state ? StateType::IS_RISING : StateType::IS_FALLING;
        }

        // save last value
        _value = lastValue;

        __LDBG_printf("interruptCount>0: debounce timer running=%u _state=NONE _value=%u", _debounceTimer, _value);

        return StateType::NONE;
    }

#if DEBUG_PIN_MONITOR_DEBOUNCE
    if (_debounceTimerRunning) {
        __DBG_printf("interruptCount==0: debounce timer running=%u timeout=%u/%u", _debounceTimerRunning, get_time_diff(_debounceTimer, now), pinMonitor.getDebounceTime());
    }
#endif

    if (_debounceTimerRunning && get_time_diff(_debounceTimer, now) >= pinMonitor.getDebounceTime()) {

        // we did not register any changes and the debounce timout has expired
        // remove timer
        _debounceTimerRunning = false;

        // save last value
        _value = lastValue;
        if (_value != _state) {

            __LDBG_printf("interruptCount==0: event bounced state=%u value=%u", _state, _value);

            // our state was not reached within debounce time
            return _state ? StateType::RISING_BOUNCED : StateType::FALLING_BOUNCED;
        }

        // __LDBG_printf("state=%u value=%u lastValue=%u diff=%u icount=%u diff=%u", _state, _value, lastValue, lastChangeTimeMillis, interruptCount,tmp);

        __LDBG_printf("interruptCount==0: state=%s reached", _state ? PSTR("HIGH") : PSTR("LOW"));

        // true is rising, false = falling
        return _state ? StateType::IS_HIGH : StateType::IS_LOW;
    }

    return StateType::NONE;
}

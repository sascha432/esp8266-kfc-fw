/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "pin_monitor.h"

namespace PinMonitor {

    class Debounce {
    public:

        Debounce(uint8_t debounceTime = 20) :
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_DEBOUNCER
            _bounceCounter(0),
#endif
            _debounceTime(debounceTime),
            _stateChanged(false),
            _isRising(false),
            _isFalling(false),
            _state(false),
            _debounceTimer(0)
        {
        }

        // debounce events for a pin
        void event(uint8_t value, uint32_t micros, uint32_t millis);
        // return last state of the pin
        StateType pop_state(uint32_t millis);

    private:
#if DEBUG_PIN_MONITOR && DEBUG_PIN_MONITOR_DEBOUNCER
        uint16_t _bounceCounter;
#endif
        uint8_t _debounceTime;
        int8_t _stateChanged: 2;        // -1=LOW, 1=HIGH
        bool _isRising: 1;
        bool _isFalling: 1;
        bool _state: 1;
        bool _value: 1;
        uint32_t _debounceTimer;
    };


}

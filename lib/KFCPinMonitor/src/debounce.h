/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_PIN_MONITOR_DEBOUNCE
#define DEBUG_PIN_MONITOR_DEBOUNCE          1
#endif

#if DEBUG_PIN_MONITOR_DEBOUNCE
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace PinMonitor {

    class Monitor;

    // default settings is active high = button is pressed when the pin reads high
    enum class StateType : uint8_t {
        NONE                   = 0,
        IS_RISING              = 0x01,
        IS_FALLING             = 0x02,
        IS_HIGH                = 0x04,
        IS_LOW                 = 0x08,
        RISING_BOUNCED         = 0x10,
        FALLING_BOUNCED        = 0x20,
        // aliases
        UP                     = IS_LOW,
        DOWN                   = IS_HIGH,
        PRESSED                = IS_HIGH,
        RELEASED               = IS_LOW,
        // combinations
        HIGH_LOW               = IS_HIGH | IS_LOW,
        UP_DOWN                = UP | DOWN,
        BOUNCED                = RISING_BOUNCED | FALLING_BOUNCED,
        RISING_FALLING         = IS_RISING | IS_FALLING,
        UNSTABLE               = IS_RISING | IS_FALLING | RISING_BOUNCED | FALLING_BOUNCED,
        ANY                    = 0xff
    };

    enum class ActiveStateType : bool {
        ACTIVE_HIGH             = true,
        ACTIVE_LOW              = false,
        PRESSED_WHEN_HIGH       = ACTIVE_HIGH,
        PRESSED_WHEN_LOW        = ACTIVE_LOW,
        NON_INVERTED            = ACTIVE_HIGH,
        INVERTED                = ACTIVE_LOW,
    };

    static inline bool isPressed(StateType state) {
        return state == StateType::PRESSED;
    }

    static inline bool isReleased(StateType state) {
        return state == StateType::RELEASED;
    }

    static inline bool isStable(StateType state) {
        return (static_cast<uint8_t>(state) & static_cast<uint8_t>(StateType::UNSTABLE)) == static_cast<uint8_t>(StateType::NONE);
    }

    static inline bool isUnstable(StateType state) {
        return !isStable(state);
    }

    static inline StateType getInvertedState(StateType state) {
        auto tmp = static_cast<uint8_t>(state);
        // swap pairs of bits
        return static_cast<StateType>(((tmp & 0xaa) >> 1) | ((tmp & 0x55) << 1));
    }

    class Debounce {
    public:

        Debounce(bool value) :
            _debounceTimer(0),
            _state(value),
            _value(value),
            _debounceTimerRunning(false)
        {
        }

        inline StateType debounce(bool lastValue, uint16_t interruptCount, uint32_t last, uint32_t now, uint32_t _micros) {
#if DEBUG_PIN_MONITOR_DEBOUNCE
            auto tmp = _debounce(lastValue, interruptCount, last,now, _micros);
            if (interruptCount) {
                __DBG_printf("debounce=%u value=%u int_count=%u last=%u now=%u micros=%u", tmp, lastValue, interruptCount, last, now, _micros);
            }
            return tmp;
#else
            return _debounce(lastValue, interruptCount, last,now, _micros);
#endif
        }
        void setState(bool state, uint32_t debounceTimer = 0);

    private:
        StateType _debounce(bool lastValue, uint16_t interruptCount, uint32_t last, uint32_t now, uint32_t _micros);

    private:
        friend Monitor;
        uint32_t _debounceTimer;
        bool _state: 1;
        bool _value: 1;
        bool _debounceTimerRunning: 1;
    };

    inline void Debounce::setState(bool state, uint32_t debounceTimer)
    {
        _state = state;
        _value = state;
        if (debounceTimer) {
            _debounceTimerRunning = true;
            _debounceTimer = debounceTimer;
        }
        else {
            _debounceTimerRunning = false;
        }
    }

}

#include <debug_helper_disable.h>

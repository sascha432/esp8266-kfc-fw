/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

namespace PinMonitor {

    class Monitor;

    using TimeType = uint32_t;

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
#if DEBUG_PIN_MONITOR_EVENTS
            _bounceCounter(0),
            _startDebounce(0),
            _debounceTime(0),
#endif
            _debounceTimer(0),
            _state(value),
            _value(value),
            _debounceTimerRunning(false)
        {
        }

        StateType debounce(bool lastValue, uint16_t interruptCount, TimeType last, TimeType now);

    private:
        friend Monitor;

#if DEBUG_PIN_MONITOR_EVENTS
        uint16_t _bounceCounter;
        TimeType _startDebounce;
        uint16_t _debounceTime;
#endif
        TimeType _debounceTimer;
        bool _state: 1;
        bool _value: 1;
        bool _debounceTimerRunning: 1;
    };

}

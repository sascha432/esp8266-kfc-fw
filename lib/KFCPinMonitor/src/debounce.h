/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

namespace PinMonitor {

    class Monitor;

    using TimeType = uint32_t;

    enum class StateType : uint8_t {
        NONE                   = 0,
        IS_RISING              = 0x01,
        IS_FALLING             = 0x02,
        IS_HIGH                = 0x04,
        IS_LOW                 = 0x08,
        RISING_BOUNCED         = 0x10,
        FALLING_BOUNCED        = 0x20,
        HIGH_LOW               = IS_HIGH | IS_LOW,
        BOUNCED                = RISING_BOUNCED | FALLING_BOUNCED,
        RISING_FALLING         = IS_RISING | IS_FALLING,
        RISING_FALLING_BOUNCED = IS_RISING | IS_FALLING | BOUNCED,
        ANY                    = 0xff
    };

    class Debounce {
    public:

        Debounce(bool value) :
#if DEBUG_PIN_MONITOR
            _bounceCounter(0),
            _startDebounce(0),
            _debounceTime(0),
#endif
            _debounceTimer(0),
            _state(value),
            _value(value)
        {
        }

        StateType debounce(bool lastValue, uint16_t interruptCount, TimeType last, TimeType now);

    private:
        void setRisingFalling(bool value, TimeType now);

    private:
        friend Monitor;

#if DEBUG_PIN_MONITOR
        uint16_t _bounceCounter;
        TimeType _startDebounce;
        uint16_t _debounceTime;
#endif
        TimeType _debounceTimer;
        bool _state: 1;
        bool _value: 1;
    };

}

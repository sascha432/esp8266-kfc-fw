/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <MicrosTimer.h>
#include "pin.h"
#include "push_button.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace PinMonitor {

    enum class RotaryEncoderEventType : uint8_t {
        NONE                            = 0,
        RIGHT                           = 0x10,
        CLOCK_WISE                      = RIGHT,
        LEFT                            = 0x20,
        COUNTER_CLOCK_WISE              = LEFT,
        ANY                             = 0xff,
        MAX_BITS                        = 8
    };

    enum class RotaryEncoderDirection : uint8_t {
        LEFT = 0,
        RIGHT = 1,
        LAST = RIGHT
    };

    class RotaryEncoder;

    class RotaryEncoderPin : public Pin {
    public:
        using EventType = RotaryEncoderEventType;

    public:
        RotaryEncoderPin(uint8_t pin, RotaryEncoderDirection direction, RotaryEncoder *encoder, ActiveStateType state) :
            Pin(pin, encoder, StateType::UP_DOWN, state),
            _direction(direction)
        {
        }
        virtual ~RotaryEncoderPin();

    public:
        virtual void loop() override;

    protected:
        // event for StateType is final
        virtual void event(StateType state, uint32_t now) final {}

        void _buttonReleased();
        bool _fireEvent(EventType eventType);
        void _reset();

    protected:
        RotaryEncoderDirection _direction;
    };

    class RotaryEncoder {
    public:
        using EventType = RotaryEncoderEventType;

    public:
        RotaryEncoder(ActiveStateType state) :
            _activeState(state),
            _state(0)
        {
#if !defined(PIN_MONITOR_HAVE_ROTARY_ENCODER) || PIN_MONITOR_HAVE_ROTARY_ENCODER == 0
            __DBG_panic("PIN_MONITOR_HAVE_ROTARY_ENCODER=1 not set");
#endif
        }
        virtual ~RotaryEncoder() {}

        virtual void event(EventType eventType, uint32_t now) {
            __LDBG_printf("event_type=%u now=%u", eventType, now);
        }

        void attachPins(uint8_t pin1, uint8_t pin2);
        void loop();

    protected:
        uint8_t _process(uint8_t pinState);

    protected:
        friend HardwarePin;

        stdex::fixed_circular_buffer<uint8_t, 128> _states;
        ActiveStateType _activeState;
        uint8_t _state;
        uint8_t _pin1;
        uint8_t _pin2;
    };
}

#include <debug_helper_disable.h>

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
        LEFT                            = _BV(0),
        RIGHT                           = _BV(1),
        ANY                             = 0xff,
        MAX_BITS                        = 8
    };

    class RotaryEncoder;

    class RotaryEncoderPin : public Pin {
    public:
        using EventType = RotaryEncoderEventType;

    public:
        RotaryEncoderPin(uint8_t pin, uint8_t direction, RotaryEncoder *encoder, ActiveStateType state) :
            Pin(pin, encoder, StateType::UP_DOWN, state),
            _direction(direction)
        {
        }
        ~RotaryEncoderPin();

    public:
        virtual void event(EventType eventType, uint32_t now);
        virtual void loop() override;

    protected:
        // event for StateType is final
        virtual void event(StateType state, uint32_t now) final;

        void _buttonReleased();
        bool _fireEvent(EventType eventType);
        void _reset();

#if DEBUG_PIN_MONITOR_BUTTON_NAME
    public:
        void setName(const String &name) {
            _name = String(F("name=<")) + name + '>';
        }

        const char *name() const {
            return _name.c_str();
        }

    private:
        String _name;
#endif


    protected:
        uint8_t _direction: 1;
    };

    class RotaryEncoder {
    public:
        using EventType = RotaryEncoderEventType;

    public:
        RotaryEncoder() {}
    };


}

#include <debug_helper_disable.h>

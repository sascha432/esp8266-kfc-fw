/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <MicrosTimer.h>
#include "push_button.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace PinMonitor {

    class RotaryEncoderPin : public Pin {
    public:
        using EventType = RotaryEncoderEventType;

    public:
        RotaryEncoderPin(uint8_t pin, RotaryEncoderDirection direction, RotaryEncoder *encoder, ActiveStateType state);
        virtual ~RotaryEncoderPin();

    public:
        virtual void loop() override;

        RotaryEncoder *getEncoder() const {
            return reinterpret_cast<RotaryEncoder *>(const_cast<void *>(getArg()));
        }

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
            _mask1(0),
            _mask2(0),
            _state(0)
        {
        }
        virtual ~RotaryEncoder() {}

        virtual void event(EventType eventType, uint32_t now) {
            __LDBG_printf("event_type=%u now=%u", eventType, now);
        }

        void attachPins(uint8_t pin1, uint8_t pin2);

        // void push_back(uint32_t time, uint8_t pin, uint16_t gpi_register);
        // void push_back(uint32_t time, uint8_t pin);

        void processEvent(const Interrupt::Event &event);

    public:
        friend HardwarePin;

        // uint8_t _process(uint8_t pinState);

        // stdex::fixed_circular_buffer<uint8_t, 128> _states;
        ActiveStateType _activeState;
        uint16_t _mask1;
        uint16_t _mask2;
        uint8_t _state;
        // uint8_t _pin1: 4;
        // uint8_t _pin2: 4;
    };


    // inline __attribute__((__always_inline__))
    // void RotaryEncoder::push_back(uint32_t time, uint8_t pin, uint16_t gpi)
    // {
    //     eventBuffer.emplace_back(micros(), pin, ((gpi & _mask1) != 0) | (((gpi & _mask2) != 0) << 1));
    // }


    // inline __attribute__((__always_inline__))
    // void RotaryEncoder::push_back(uint32_t time, uint8_t pin)
    // {
    //     eventBuffer.emplace_back(micros(), pin, ((GPI & _mask1) != 0) | (((GPI & _mask2) != 0) << 1));
    // }

}

#include <debug_helper_disable.h>

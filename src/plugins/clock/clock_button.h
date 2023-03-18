/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

//
//
//
// button:
//  short/long press while off = turn on
//  short press = toggle rotary encoder action (goes back to default action after 5 seconds)
//  long press = turn off
//  long press >5s = software reset (starts to blink red for 2 seconds)
//  long press >8.2s = hardware reset (this works even if the software is not responding anymore)
//
// rotary encoder:
//  default action (with acceleration):
//      clock wise = increase brightness
//      counter clock wise = decrease brightness
//  action 1 (once per second):
//      change animation
//

#if IOT_CLOCK_BUTTON_PIN != -1

#include <Arduino_compat.h>
#include "clock_base.h"

using namespace PinMonitor;

namespace Clock {

    class ButtonConfig : public PushButtonConfig
    {
    public:
        using PushButtonConfig::PushButtonConfig;

        ButtonConfig(uint8_t button) : PushButtonConfig(EventType::PRESSED_LONG_HOLD_SINGLE_DOUBLE_CLICK, 350, 1500, 250) {}
    };

    class Button : public PushButton {
    public:
        Button();
        Button(uint8_t pin, uint8_t button, ClockPlugin &clock);

        uint8_t getButtonNum() const;

        virtual void event(EventType state, uint32_t now) override;

        ClockPlugin &getBase() const;

    protected:
        uint8_t _button;
    };

    inline Button::Button() :
        PushButton(),
        _button(0)
    {
    }

    inline Button::Button(uint8_t pin, uint8_t button, ClockPlugin &clock) :
        PushButton(pin, &clock, std::move(ButtonConfig(button))),
        _button(button)
    {
    }

    inline uint8_t Button::getButtonNum() const
    {
        return _button;
    }

    inline ClockPlugin &Button::getBase() const
    {
        return *reinterpret_cast<ClockPlugin *>(const_cast<void *>(getArg()));
    }

    class TouchButton : public Button {
    public:
        using Button::getButtonNum;
        using Button::getBase;

        TouchButton() : Button() {}
        TouchButton(uint8_t pin, uint8_t button, ClockPlugin &clock) :
            Button(pin, button, clock)
        {
            _activeState = true; //ActiveStateType::PRESSED_WHEN_HIGH;
            _subscribedEvents = PushButtonConfig::EventType::PRESSED;
            _longPressTime = ~0;
            _clickTime = 350;
            _repeatTime = ~0;
        }
    };

#if IOT_CLOCK_HAVE_ROTARY_ENCODER

    class RotaryEncoder : public PinMonitor::RotaryEncoder {
        using PinMonitor::RotaryEncoder::RotaryEncoder;
        using PinMonitor::RotaryEncoder::EventType;

        virtual void event(EventType eventType, uint32_t now) override;
    };

#endif

}

#endif

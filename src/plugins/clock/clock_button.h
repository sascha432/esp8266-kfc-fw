/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#if IOT_CLOCK_BUTTON_PIN != -1

#include <Arduino_compat.h>
#include "clock_base.h"

using namespace PinMonitor;

namespace Clock {

    class ButtonConfig : public PushButtonConfig
    {
    public:
        using PushButtonConfig::PushButtonConfig;

        ButtonConfig(uint8_t button) : PushButtonConfig(EventType::PRESSED_LONG_HELD_SINGLE_DOUBLE_CLICK, 350, 1500, 250) {}
    };

    class Button : public PushButton {
    public:
        Button();
        Button(uint8_t pin, uint8_t button, ClockPlugin &clock);

        uint8_t getButtonNum() const;

        virtual void event(EventType state, uint32_t now) override;

        ClockPlugin &getBase() const;

    private:
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
#if DEBUG_PIN_MONITOR_BUTTON_NAME
        setName(PrintString(F("BUTTON:%u arg=%p btn=%p"), button, getArg(), this));
#endif
    }

    inline uint8_t Button::getButtonNum() const
    {
        return _button;
    }

    inline ClockPlugin &Button::getBase() const
    {
        return *reinterpret_cast<ClockPlugin *>(const_cast<void *>(getArg()));
    }

}

#endif

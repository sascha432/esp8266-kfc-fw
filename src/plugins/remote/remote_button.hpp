/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "remote_def.h"
#include "remote_base.h"
#include "remote_button.h"

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace PinMonitor;

 namespace RemoteControl {

    inline Button::Button() :
        PushButton(),
        _button(0),
        _timeOffset(0)
    {
    }

    inline Button::Button(uint8_t pin, uint8_t button, Base *base) :
        PushButton(pin, base, std::move(ButtonConfig(base->_getConfig(), button))),
        _button(button),
        _timeOffset(0)
    {
    }

    inline uint8_t Button::getButtonNum() const
    {
        return _button;
    }

    inline bool Button::isPressed() const
    {
        return getBase()->_isPressed(_button);
    }

    inline Base *Button::getBase() const
    {
        return reinterpret_cast<Base *>(const_cast<void *>(getArg()));
    }

    inline uint32_t Button::_getEventTimeForFirstEvent()
    {
        _timeOffset = micros();
        return 0;
    }

    inline uint32_t Button::_getEventTime()
    {
        return micros() - _timeOffset;
    }

}

#include <debug_helper_disable.h>

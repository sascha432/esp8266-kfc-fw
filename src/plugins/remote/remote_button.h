/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "remote_def.h"
#include "remote_base.h"

using namespace PinMonitor;
namespace RemoteControl {

    class ButtonConfig : public PushButtonConfig
    {
    public:
        using PushButtonConfig::PushButtonConfig;

        ButtonConfig(ConfigType &config, uint8_t button, bool testMode = false) :
            PushButtonConfig(
                _getEventTypes(config.actions[button], testMode),
                config.shortpressTime,
                config.longpressTime,
                config.repeatTime
            )
        {
        }

        inline static EventType _getEventTypes(ActionType &config, bool testMode) {
            if (testMode) {
                return static_cast<EventType>(
                    static_cast<uint8_t>(EventType::UP)|
                    static_cast<uint8_t>(EventType::DOWN)|
                    static_cast<uint8_t>(EventType::REPEATED_CLICK)|
                    static_cast<uint8_t>(EventType::LONG_PRESSED)|
                    static_cast<uint8_t>(EventType::HELD)
                );
            }
            else {
                uint8_t tmp = static_cast<uint8_t>(EventType::UP)|static_cast<uint8_t>(EventType::DOWN);
                if (config.single_click) {
                    tmp |= static_cast<uint8_t>(EventType::SINGLE_CLICK);
                }
                if (config.double_click) {
                    tmp |= static_cast<uint8_t>(EventType::DOUBLE_CLICK);
                }
                if (config.hasMultiClick()) {
                    tmp |= static_cast<uint8_t>(EventType::REPEATED_CLICK);
                }
                if (config.longpress) {
                    tmp |= static_cast<uint8_t>(EventType::LONG_PRESSED);
                }
                if (config.held) {
                    tmp |= static_cast<uint8_t>(EventType::HELD);
                }
                return static_cast<EventType>(tmp);
            }
        }
    };


    inline Button::Button() : PushButton()
    {
    }

    inline Button::Button(uint8_t pin, uint8_t button, Base *base, bool testMode) :
        PushButton(pin, base, std::move(ButtonConfig(base->_getConfig(), button, testMode))),
        _button(button)
    {
#if DEBUG_PIN_MONITOR_BUTTON_NAME
        setName(PrintString(F("BUTTON:%u arg=%p btn=%p"), button, getArg(), this));
#endif
    }

    inline uint8_t Button::getNum() const
    {
        return _button;
    }

    inline bool Button::isPressed() const
    {
        return getBase()->_isPressed(_button);
    }



    inline ButtonEvent::ButtonEvent()
    {
        _timestamp = 0;
        _duration = 0;
        _repeat = 0;
        _type = 0;
        _button = 0;
        _comboButton = 0;
    }

    inline ButtonEvent::ButtonEvent(uint8_t button, EventType type, uint32_t duration, uint32_t repeat, uint32_t comboButton) :
        _timestamp(millis()),
        _duration(duration),
        _repeat(repeat),
        _type(static_cast<uint32_t>(type)),
        _button(button),
        _comboButton(comboButton)
    {
    }

    inline uint8_t ButtonEvent::getButton() const
    {
        return _button;
    }

    // inline int8_t ButtonEvent::getComboButton() const
    // {
    //     return _comboButton;
    // }

    // inline ComboActionType ButtonEvent::getCombo(const ActionType &actions) const
    // {
    //     if (_comboButton != kComboButtonNone) {
    //         return actions.combo[_comboButton];
    //     }
    //     return ComboActionType();
    // }

    inline EventType ButtonEvent::getType() const
    {
        return type();
    }

    inline uint16_t ButtonEvent::getDuration() const
    {
        return _duration;
    }

    inline uint16_t ButtonEvent::getRepeat() const
    {
        return _repeat;
    }

    inline uint32_t ButtonEvent::getTimestamp() const
    {
        return _timestamp;
    }

    inline void ButtonEvent::remove()
    {
        type(EventType::NONE);
    }

#if DEBUG_IOT_REMOTE_CONTROL
    inline void ButtonEvent::printTo(Print &output) const
    {
        output.printf_P(PSTR("%u="), _button);
        switch(type()) {
            case EventType::PRESS:
                output.printf_P(PSTR("press,d=%u"), _duration);
                break;
            case EventType::RELEASE:
                output.printf_P(PSTR("release,d=%u"), _duration);
                break;
            case EventType::REPEAT:
                output.printf_P(PSTR("repeat,d=%u,c=%u"), _duration, _repeat);
                break;
            case EventType::COMBO_RELEASE:
                output.printf_P(PSTR("release,combo=%d,d=%u"), _comboButton, _duration);
                break;
            case EventType::COMBO_REPEAT:
                output.printf_P(PSTR("repeat,combo=%d,d=%u,c=%u"), _comboButton, _duration, _repeat);
                break;
            case EventType::NONE:
                output.printf_P(PSTR("none"));
                break;
            default:
                break;
        }
        output.printf_P(PSTR(",ts=%u"), _timestamp);
    }

    inline String ButtonEvent::toString() const
    {
        PrintString str;
        printTo(str);
        return str;
    }
#endif

    inline EventType ButtonEvent::type() const
    {
        return static_cast<EventType>(_type);
    }

    inline void ButtonEvent::type(const EventType type)
    {
        _type = static_cast<uint32_t>(type);
    }

}

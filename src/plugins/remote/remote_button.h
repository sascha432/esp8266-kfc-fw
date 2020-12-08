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

        static constexpr EventType kDefaultEvents = EventType::ALL;

        ButtonConfig(ConfigType &config, uint8_t button) :
            PushButtonConfig(
                kDefaultEvents,
                config.shortpressTime,
                config.longpressTime,
                config.repeatTime
            )
        {
        }
    };

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

//     inline EventType ButtonEvent::getType() const
//     {
//         return type();
//     }

//     inline uint16_t ButtonEvent::getDuration() const
//     {
//         return _duration;
//     }

//     inline uint16_t ButtonEvent::getRepeat() const
//     {
//         return _repeat;
//     }

//     inline uint32_t ButtonEvent::getTimestamp() const
//     {
//         return _timestamp;
//     }

//     inline void ButtonEvent::remove()
//     {
//         type(EventType::NONE);
//     }

// #if DEBUG_IOT_REMOTE_CONTROL
//     inline void ButtonEvent::printTo(Print &output) const
//     {
//         output.printf_P(PSTR("%u="), _button);
//         switch(type()) {
//             case EventType::PRESS:
//                 output.printf_P(PSTR("press,d=%u"), _duration);
//                 break;
//             case EventType::RELEASE:
//                 output.printf_P(PSTR("release,d=%u"), _duration);
//                 break;
//             case EventType::REPEAT:
//                 output.printf_P(PSTR("repeat,d=%u,c=%u"), _duration, _repeat);
//                 break;
//             case EventType::COMBO_RELEASE:
//                 output.printf_P(PSTR("release,combo=%d,d=%u"), _comboButton, _duration);
//                 break;
//             case EventType::COMBO_REPEAT:
//                 output.printf_P(PSTR("repeat,combo=%d,d=%u,c=%u"), _comboButton, _duration, _repeat);
//                 break;
//             case EventType::NONE:
//                 output.printf_P(PSTR("none"));
//                 break;
//             default:
//                 break;
//         }
//         output.printf_P(PSTR(",ts=%u"), _timestamp);
//     }

//     inline String ButtonEvent::toString() const
//     {
//         PrintString str;
//         printTo(str);
//         return str;
//     }
// #endif

//     inline EventType ButtonEvent::type() const
//     {
//         return static_cast<EventType>(_type);
//     }

//     inline void ButtonEvent::type(const EventType type)
//     {
//         _type = static_cast<uint32_t>(type);
//     }
}

#include "remote_button.hpp"

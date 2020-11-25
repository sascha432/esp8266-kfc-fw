/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <PinMonitor.h>
#include "remote_def.h"

using namespace PinMonitor;

namespace RemoteControl {

    class ButtonConfig : public PushButtonConfig
    {
    public:
        ButtonConfig(ActionType &config) :
            PushButtonConfig(
                EnumHelper::Bitset::all(EventType::REPEAT, EventType::CLICK, EventType::LONG_CLICK),
                config.shortpress,
                config.longpress,
                config.repeat
            )
        {
        }
    };

    class Button : public PushButton {
    public:
        Button() : PushButton(), _base(nullptr) {}

        Button(uint8_t pin, uint8_t button, Base *base) :
            PushButton(pin, base, std::move(ButtonConfig(base->_getConfig().actions[button]))),
            _base(base),
            _button(button),
            _pressed(false)
        {
        #if DEBUG_PIN_MONITOR
            setName(PrintString(F("BUTTON:%u"), button));
        #endif
        }

        uint8_t getNum() const {
            return _button;
        }
        bool isPressed() const {
            return _pressed;
        }


        virtual void event(EventType state, uint32_t now) override;

    // private:
    //     void _setLevel(int32_t newLevel, int16_t curLevel, float fadeTime);
    //     void _setLevel(int32_t newLevel, float fadeTime);
    //     void _changeLevel(int32_t changeLevel, float fadeTime);
    //     void _changeLevelSingle(uint16_t steps, bool invert);
    //     void _changeLevelRepeat(uint16_t repeatTime, bool invert);

    private:
        Base *_base;
         uint8_t _button;
         bool _pressed: 1;
        //Buttons &_dimmer;
        // int16_t _level;
        // uint8_t _channel: 3;
        // uint8_t _button: 1;
    };



    class ButtonEvent {
    public:
        static constexpr uint32_t kComboButtonNone = (1 << 3) - 1;

        ButtonEvent(uint8_t button, EventTypeEnum_t type, uint32_t duration = 0, uint32_t repeat = 0, uint32_t comboButton = kComboButtonNone) :
            _timestamp(millis()),
            _duration(duration),
            _repeat(repeat),
            _type(static_cast<uint32_t>(type)),
            _button(button),
            _comboButton(comboButton)
        {
        }

        uint8_t getButton() const {
            return _button;
        }

        int8_t getComboButton() const {
            return _comboButton;
        }

        ComboAction_t getCombo(const Action_t &actions) const {
            if (_comboButton != kComboButtonNone) {
                return actions.combo[_comboButton];
            }
            return ComboAction_t();
        }

        EventTypeEnum_t getType() const {
            return type();
        }

        uint16_t getDuration() const {
            return _duration;
        }

        uint16_t getRepeat() const {
            return _repeat;
        }

        uint32_t getTimestamp() const {
            return _timestamp;
        }

        void remove() {
            type(EventType::NONE);
        }

#if DEBUG_IOT_REMOTE_CONTROL
        void printTo(Print &output) const {
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
            }
            output.printf_P(PSTR(",ts=%u"), _timestamp);
        }

        String toString() const {
            PrintString str;
            printTo(str);
            return str;
        }
#endif

    private:
        EventType type() const {
            return static_cast<EventType>(_type);
        }

        void type(const EventType type) {
            _type = static_cast<uint32_t>(type);
        }

    private:
        uint32_t _timestamp;
        uint32_t _duration : 14;            // max 16 seconds
        uint32_t _repeat : 9;               // 512x times
        uint32_t _type: 3;
        uint32_t _button: 3;
        uint32_t _comboButton: 3;

        static_assert((int)EventType::MAX < (1U << 3), "too many buttons for _type");
        static_assert(IOT_REMOTE_CONTROL_BUTTON_COUNT < (1U << 3) - 1, "too many buttons for _button");
        static_assert(IOT_REMOTE_CONTROL_BUTTON_COUNT < kComboButtonNone, "too many buttons for _comboButton");
    };

    static constexpr size_t ButtonEventSize = sizeof(ButtonEvent);

}

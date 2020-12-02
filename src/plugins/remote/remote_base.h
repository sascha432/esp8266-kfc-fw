/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "remote_def.h"

namespace RemoteControl {

    static constexpr auto _buttonPins  = ButtonPinsArray(IOT_REMOTE_CONTROL_BUTTON_PINS);

    class Base {
    public:

        Base() :
            _config(),
            _autoSleepTimeout(15000),
            _buttonsLocked(~0),
            _longPress(0),
            _comboButton(-1)
        {}

        // void _onButtonPressed(Button& btn);
        virtual void _onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount) = 0;
        virtual void _onButtonReleased(Button& btn, uint16_t duration) = 0;

        bool _anyButton(const Button *btn = nullptr) const {
            for(const auto &button : _buttons) {
                if (&button != btn && button.isPressed()) {
                    return true;
                }
            }
            return false;
        }
        // void _onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
        // void _onButtonReleased(Button& btn, uint16_t duration);

    public:
        ConfigType &_getConfig() {
            return _config;
        }

    protected:
        void _resetAutoSleep() {
            if (_autoSleepTimeout && _autoSleepTimeout != kAutoSleepDisabled) {
                _autoSleepTimeout = millis() + (_config.autoSleepTime * 1000UL);
                __LDBG_printf("auto deep sleep set %u", _autoSleepTimeout);
            }
        }

    protected:
        ButtonArray _buttons;
        ButtonEventList _events;
        ConfigType _config;
        uint32_t _autoSleepTimeout;
        uint32_t _buttonsLocked;
        uint8_t _longPress;
        int8_t _comboButton;
    };
}

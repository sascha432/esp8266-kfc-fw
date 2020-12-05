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
            _comboButton(-1),
            _pressed(0)
        {}

        virtual void _onShortPress(Button &button) = 0;
        virtual void _onLongPress(Button &button) = 0;
        virtual void _onRepeat(Button &button) = 0;

        // void _onButtonPressed(Button& btn);
        // virtual void _onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount) = 0;
        // virtual void _onButtonReleased(Button& btn, uint16_t duration) = 0;

        // // return if any buttopn is pressed
        // bool _anyButton(const Button *ignoreButton = nullptr) const {
        //     for(const auto &buttonPtr: pinMonitor.getVector()) {
        //         if (buttonPtr.get() != ignoreButton && buttonPtr->getArg() == this && buttonPtr->isEnabled() && buttonPtr->isPressed()) {
        //             return true;
        //         }
        //     }
        //     return false;
        // }
        // void _onButtonHeld(Button& btn, uint16_t duration, uint16_t repeatCount);
        // void _onButtonReleased(Button& btn, uint16_t duration);

    public:
        ConfigType &_getConfig() {
            return _config;
        }

        bool _isPressed(uint8_t buttonNum) const {
            return _pressed & _getPressedMask(buttonNum);
        }

        uint8_t _getPressedMask(uint8_t buttonNum) const {
            return 1 << buttonNum;
        }

        bool _anyButton(uint8_t ignoreMask) const {
            return (_pressed & ~ignoreMask) != 0;
        }

        bool _anyButton() const {
            return _pressed != 0;
        }

#if DEBUG_IOT_REMOTE_CONTROL
        const char *_getPressedButtons() const;
#endif

    protected:
        void _resetAutoSleep() {
            if (_autoSleepTimeout && _autoSleepTimeout != kAutoSleepDisabled) {
                _autoSleepTimeout = millis() + (_config.autoSleepTime * 1000UL);
                __LDBG_printf("auto deep sleep set %u", _autoSleepTimeout);
            }
        }

    protected:
        friend Button;

        //ButtonArray _buttons;
        ButtonEventList _events;
        ConfigType _config;
        uint32_t _autoSleepTimeout;
        uint32_t _buttonsLocked;
        uint8_t _longPress;
        int8_t _comboButton;
        uint8_t _pressed;
    };
}

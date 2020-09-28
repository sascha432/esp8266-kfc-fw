/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <EventScheduler.h>
#include <stl_ext/fixed_circular_buffer.h>
#include <kfc_fw_config_classes.h>

#ifndef DEBUG_IOT_REMOTE_CONTROL
#define DEBUG_IOT_REMOTE_CONTROL                                1
#endif

#if DEBUG_IOT_REMOTE_CONTROL
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

// number of buttons available
#ifndef IOT_REMOTE_CONTROL_BUTTON_COUNT
#define IOT_REMOTE_CONTROL_BUTTON_COUNT                         4
#endif

// button pins
#ifndef IOT_REMOTE_CONTROL_BUTTON_PINS
#define IOT_REMOTE_CONTROL_BUTTON_PINS                          { 14, 4, 12, 13 }   // D5, D2, D6, D7
#endif

// set high while running
#ifndef IOT_REMOTE_CONTROL_AWAKE_PIN
#define IOT_REMOTE_CONTROL_AWAKE_PIN                            15
#endif

// signal led
#ifndef IOT_REMOTE_CONTROL_LED_PIN
#define IOT_REMOTE_CONTROL_LED_PIN                              2
#endif

#ifndef IOT_REMOTE_CONTROL_COMBO_BTN
#define IOT_REMOTE_CONTROL_COMBO_BTN                            1
#endif


class HassPlugin;

namespace RemoteControl {

    static const uint32_t kAutoSleepDisabled = ~0;

    class ButtonEvent;
    class Button;

    using KFCConfigurationClasses::Plugins;
    using ConfigType = Plugins::RemoteControl::Config_t;
    using ActionType = Plugins::RemoteControl::Action_t;
    using ComboActionType = Plugins::RemoteControl::ComboAction_t;
    using ButtonPinsArray = std::array<const uint8_t, IOT_REMOTE_CONTROL_BUTTON_COUNT>;
    using ButtonArray = std::array<Button, IOT_REMOTE_CONTROL_BUTTON_COUNT>;
    using ButtonEventList = stdex::fixed_circular_buffer<ButtonEvent, 32>;

    static constexpr auto _buttonPins  = ButtonPinsArray(IOT_REMOTE_CONTROL_BUTTON_PINS);

    enum class EventType : uint8_t {
        NONE = 0,
        REPEAT,
        PRESS,
        RELEASE,
        COMBO_REPEAT,
        COMBO_RELEASE,
        MAX
    };

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

    using Action_t =  ActionType;
    using ComboAction_t = ComboActionType;
    using EventTypeEnum_t = EventType;

}


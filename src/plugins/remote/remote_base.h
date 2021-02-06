/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "remote_def.h"
#include "remote_event_queue.h"

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
            _pressed(0),
            _testMode(0)
        {}

        virtual void _onShortPress(Button &button) = 0;
        virtual void _onLongPress(Button &button) = 0;
        virtual void _onRepeat(Button &button) = 0;

        // enable test mode
        void setTestMode(bool testMode) {
            _testMode = testMode;
        }

        bool isTestMode() const {
            return _testMode;
        }

        void queueEvent(Button::EventType type, uint8_t buttonNum, uint32_t eventTime, uint16_t actionId);
        void queueEvent(Button::EventType type, uint8_t buttonNum, uint16_t eventCount, uint32_t eventTime, uint16_t actionId);

        void timerCallback(Event::CallbackTimerPtr timer);

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
                _autoSleepTimeout = millis() + (_config.auto_sleep_time * 1000UL);
                __LDBG_printf("auto deep sleep set %u", _autoSleepTimeout);
            }
        }

    protected:
        friend Button;

        //ButtonArray _buttons;
        // ButtonEventList _events;
        EventQueue _queue;
        Queue::Lock _lock;
        Event::Timer _queueTimer;
        ConfigType _config;
        uint32_t _autoSleepTimeout;
        uint32_t _buttonsLocked;
        uint8_t _longPress;
        int8_t _comboButton;
        uint8_t _pressed;
        bool _testMode;
    };

    inline void Base::queueEvent(Button::EventType type, uint8_t buttonNum, uint32_t eventTime, uint16_t actionId)
    {
        queueEvent(type, buttonNum, 0, eventTime, actionId);
    }

}
/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "remote_def.h"
#include "remote_event_queue.h"
#include <kfc_fw_config.h>

namespace RemoteControl {

    static constexpr auto _buttonPins  = ButtonPinsArray(IOT_REMOTE_CONTROL_BUTTON_PINS);

    using EventNameType = Plugins::RemoteControl::RemoteControl::EventNameType;

    inline static EventNameType buttonEventToEventBitMask(Button::EventType type) {
        switch(type) {
            case Button::EventType::DOWN:
                return EventNameType::BV_BUTTON_DOWN;
            case Button::EventType::UP:
                return EventNameType::BV_BUTTON_UP;
            case Button::EventType::PRESSED:
                return EventNameType::BV_BUTTON_PRESS;
            case Button::EventType::LONG_PRESSED:
                return EventNameType::BV_BUTTON_LONG_PRESS;
            case Button::EventType::SINGLE_CLICK:
                return EventNameType::BV_BUTTON_SINGLE_CLICK;
            case Button::EventType::DOUBLE_CLICK:
                return EventNameType::BV_BUTTON_DOUBLE_CLICK;
            case Button::EventType::REPEATED_CLICK:
                return EventNameType::BV_BUTTON_MULTI_CLICK;
            case Button::EventType::HOLD:
            case Button::EventType::HOLD_START:
                return EventNameType::BV_BUTTON_HOLD_REPEAT;
            case Button::EventType::HOLD_RELEASE:
                return EventNameType::BV_BUTTON_HOLD_RELEASE;
            default:
                break;
        }
        return EventNameType::BV_NONE;
    }

    inline static uint16_t getEventBitmask(Button::EventType type) {
        return static_cast<uint16_t>(buttonEventToEventBitMask(type));
    }

    // 0 {button_name}-down
    // 1 {button_name}-up
    // 2 {button_name}-press
    // 3 {button_name}-long-press
    // 4 {button_name}-single-click
    // 5 {button_name}-double-click
    // 6 {button_name}-multi-{repeat}-click
    // 7 {button_name}-hold
    // 8 {button_name}-hold-release

    inline static EventNameType buttonEventToEventType(Button::EventType type)
    {
        using EventNameType = Plugins::RemoteControl::RemoteControl::EventNameType;
        switch(type) {
            case Button::EventType::DOWN:
                return EventNameType::BUTTON_DOWN;
            case Button::EventType::UP:
                return EventNameType::BUTTON_UP;
            case Button::EventType::PRESSED:
                return EventNameType::BUTTON_PRESS;
            case Button::EventType::LONG_PRESSED:
                return EventNameType::BUTTON_LONG_PRESS;
            case Button::EventType::SINGLE_CLICK:
                return EventNameType::BUTTON_SINGLE_CLICK;
            case Button::EventType::DOUBLE_CLICK:
                return EventNameType::BUTTON_DOUBLE_CLICK;
            case Button::EventType::REPEATED_CLICK:
                return EventNameType::BUTTON_MULTI_CLICK;
            case Button::EventType::HOLD_REPEAT:
                return EventNameType::BUTTON_HOLD_REPEAT;
            case Button::EventType::HOLD_RELEASE:
                return EventNameType::BUTTON_HOLD_RELEASE;
            default:
                break;
        }
        return EventNameType::ANY;
    }

    inline static uint16_t getEventTypeInt(Button::EventType type)
    {
        return static_cast<uint16_t>(buttonEventToEventType(type));
    }


    class Base {
    public:
        using EventType = Button::EventType;

        Base() :
            _config(),
            _autoSleepTimeout(kAutoSleepDefault),
            _buttonsLocked(~0),
            _longPress(0),
            _comboButton(-1),
            _pressed(0),
            _autoDiscoveryRunOnce(true),
            _testMode(false),
            _signalWarning(false)
        {}

        // enable test mode
        void setTestMode(bool testMode) {
            _testMode = testMode;
        }

        bool isTestMode() const {
            return _testMode;
        }

        String createJsonEvent(const String &action, EventType type, uint8_t buttonNum, uint16_t eventCount, uint32_t eventTime, bool reset, const char *combo = nullptr) const;
        void queueEvent(EventType type, uint8_t buttonNum, uint16_t eventCount, uint32_t eventTime, uint16_t actionId);

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
            if (_autoSleepTimeout != kAutoSleepDisabled) {
                _autoSleepTimeout = kAutoSleepDefault;
            }
            if (_signalWarning) {
                KFCFWConfiguration::setWiFiConnectLedMode();
                _signalWarning = false;
            }
        }

    protected:
        friend Button;

        EventQueue _queue;
        Queue::Lock _lock;
        Event::Timer _queueTimer;
        ConfigType _config;
        uint32_t _autoSleepTimeout;
        uint32_t _buttonsLocked;
        uint8_t _longPress;
        int8_t _comboButton;
        uint8_t _pressed;
        bool _autoDiscoveryRunOnce;
        bool _testMode;
        bool _signalWarning;
    };

}

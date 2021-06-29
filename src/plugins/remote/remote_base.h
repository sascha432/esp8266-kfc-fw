/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "remote_def.h"
#include "remote_event_queue.h"
#include <kfc_fw_config.h>
#include <stl_ext/array.h>

class RemoteControlPlugin;

// for more information about MQTT device triggers check
// https://www.home-assistant.io/integrations/device_trigger.mqtt/
//
// those can be used without home assistant as well
// using UDP has the advantage to reduce lag when waking up. MQTT requires ~350ms to arrive at the destination, while UDP reaches it in ~220ms.
// (assuming WiFi Quick Connect works and connects within ~200ms)


namespace RemoteControl {

    static constexpr uint32_t kButtonPinsMask = Interrupt::PinAndMask::mask_of(kButtonPins);

    static inline constexpr size_t getButtonIndex(uint8_t pin, size_t index = 0) {
        return pin == kButtonPins[index] ? index : getButtonIndex(pin, index + 1);
    }

    // button combination used for maintenance mode
    // first and last PIN vom list
    static constexpr uint8_t kButtonSystemComboPins[2] = { kButtonPins.front(), kButtonPins.back() };
    // and the combined bit mask
    static constexpr uint16_t kButtonSystemComboBitMask = _BV(getButtonIndex(kButtonSystemComboPins[0])) | _BV(getButtonIndex(kButtonSystemComboPins[1]));
    static constexpr uint16_t kGPIOSystemComboBitMask = _BV(kButtonSystemComboPins[0]) | _BV(kButtonSystemComboPins[1]);


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
            __autoSleepTimeout(0),
            _maxAwakeTimeout(0),
            _minAwakeTimeout(0),
            // _buttonsLocked(~0),
            _systemButtonComboTime(0),
            _systemButtonComboTimeout(0),
            _systemButtonComboState(ComboButtonStateType::NONE),
            _longPress(0),
            _comboButton(-1),
            _pressed(0),
            _autoDiscoveryRunOnce(true),
            _testMode(false),
            _signalWarning(false),
            _isCharging(false)
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

        void event(BaseEventType type, StateType state);

    protected:
        // reser auto sleep timer and disable warning LED
        void _resetAutoSleep() {
            _setAutoSleepTimeout();
            if (_signalWarning && _isAutoSleepBlocked() == false) {
                KFCFWConfiguration::setWiFiConnectLedMode();
                _signalWarning = false;
            }
        }

        // reset auto sleep if auto sleep is not disabled
        // forcetime == 0 resets the timeout if not disabled
        // forceTime == 1 resets it if disabled
        // forceTime > 1 sets the timeout to "forceTime" even if disabled
        // maxAwakeExtratimeSeconds == 0 does not change the timeout
        // maxAwakeExtratimeSecond > 0, _maxAwakeTimeout gets updated to n seconds after auto sleep timeout
        // maxAwakeExtratimeSeconds == ~0U, the default configuration will be used
        // maxAwakeExtratimeSeconds is only set *if* the new value exceeds the previous _maxAwakeTimeout
        void _setAutoSleepTimeout(uint32_t forceTime = 0, uint32_t maxAwakeExtratimeSeconds = 0) {
            if (isAutoSleepEnabled() || forceTime) {
                if (forceTime) {
                    __DBG_printf("__autoSleepTimeout=%u _config.auto_sleep_time=%u forceTime=%d", __autoSleepTimeout, _config.auto_sleep_time, forceTime);
                }
                if (forceTime <= 1) {
                    forceTime = millis() + (_config.auto_sleep_time * 1000U);
                }
                __autoSleepTimeout = forceTime;
                if (maxAwakeExtratimeSeconds == ~0U) {
                    maxAwakeExtratimeSeconds = _config.max_awake_time * 60U;
                }
                if (maxAwakeExtratimeSeconds) {
                    _maxAwakeTimeout = std::max<uint32_t>(_maxAwakeTimeout, __autoSleepTimeout + (maxAwakeExtratimeSeconds * 1000U));
                }
            }
        }

        inline __attribute__((__always_inline__))
        bool isAutoSleepEnabled() const {
            return __autoSleepTimeout != kAutoSleepDisabled;
        }

        inline __attribute__((__always_inline__))
        bool isAutoSleepDisabled() const {
            return __autoSleepTimeout == kAutoSleepDisabled;
        }

        // disable auto sleep
        inline __attribute__((__always_inline__))
        void _disableAutoSleepTimeout() {
            __autoSleepTimeout = kAutoSleepDisabled;
        }

        void _updateMaxAwakeTimeout() {
            _maxAwakeTimeout = _config.max_awake_time * (60U * 1000U);
        }

        bool _isAutoSleepBlocked() const {
            return config.isSafeMode() || _isSystemComboActive();
        }


public:
    enum class ComboButtonStateType {
        NONE = 0,
        PRESSED,
        RELEASED,
        RESET_MENU_TIMEOUT,
        CONFIRM_EXIT,
        CONFIRM_AUTO_SLEEP_OFF,
        CONFIRM_AUTO_SLEEP_ON,
        CONFIRM_DEEP_SLEEP,
        TIMEOUT,
        EXIT_MENU_TIMEOUT,
    };

    static constexpr uint32_t kSystemComboRebootTimeout = 3500;
    static constexpr uint32_t kSystemComboMenuTimeout = 30000;
    static constexpr uint32_t kSystemComboConfirmTimeout = 850;

    void systemButtonComboEvent(bool state, EventType type = EventType::DOWN, uint8_t button = 0, uint16_t repeatCount = 0, uint32_t eventTime = 0);

private:
    void _systemComboNextState(ComboButtonStateType state);
    void _updateSystemComboButton();
    void _updateSystemComboButtonLED();
    void _resetButtonState();

    inline bool _isSystemComboActive() const {
        return _systemButtonComboTimeout != 0;
    }


    protected:
        friend Button;
        friend RemoteControlPlugin;

        EventQueue _queue;
        Queue::Lock _lock;
        Event::Timer _queueTimer;
        ConfigType _config;
        uint32_t __autoSleepTimeout;
        uint32_t _maxAwakeTimeout;
public://TODO
        uint32_t _minAwakeTimeout;
        // uint32_t _buttonsLocked;
        uint32_t _systemButtonComboTime;
        uint32_t _systemButtonComboTimeout;
        ComboButtonStateType _systemButtonComboState;
        uint8_t _longPress;
        int8_t _comboButton;
        uint8_t _pressed;
        bool _autoDiscoveryRunOnce;
        bool _testMode;
        bool _signalWarning;
        bool _isCharging;
    };

}

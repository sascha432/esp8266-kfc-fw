/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include <MicrosTimer.h>
#include "pin.h"

#if DEBUG_PIN_MONITOR
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace PinMonitor {

    // Events in order
    //
    // DOWN
    // PRESSED (occurs only if repeated clicks are disabled)
    // REPEATED_CLICK
    // SINGLE_CLICK (REPEATED_CLICK with repeat count = 1)
    // DOUBLE_CLICK (REPEATED_CLICK with repeat count = 2)
    // HELD (repeated in intervals - if this event occurs, REPEATED_CLICK, SINGLE_CLICK, DOUBLE_CLICK and LONG_PRESSED won't)
    // LONG_PRESSED
    // UP

    enum class PushButtonEventType : uint16_t {
        NONE                            = 0,

        // events related to the button status
        // fired after the button is down and debounced
        DOWN                            = _BV(0),
        // fired after the button is up and debounced
        UP                              = _BV(1),
        RELEASED                        = UP,

        // events related to status and timing configuration, fired when the button is released
        // between EventType::DOWN and EventType:UP
        PRESSED                         = _BV(2),
        LONG_PRESSED                    = _BV(3),
        CLICK                           = PRESSED,
        LONG_CLICK                      = LONG_PRESSED,

        // events fired while the button is pressed
        // after EventType::DOWN before EventType::UP
        HOLD                            = _BV(4),
        HOLD_REPEAT                     = _BV(4),

        // the first HOLD_REPEAT is HOLD_START
        HOLD_START                      = _BV(8),
        // if HOLD_START was sent, HOLD_RELEASE will be sent before UP/RELASED
        HOLD_RELEASE                    = _BV(9),

        // after EventType::UP before EventType::DOWN
        REPEATED_CLICK                  = _BV(5),
        // if SINGLE_CLICK is subscribed, REPEATED_CLICK won't be sent for _repeatCount == 1
        SINGLE_CLICK                    = _BV(6),
        // if DOUBLE_CLICK is subscribed, REPEATED_CLICK won't be sent for _repeatCount == 2
        DOUBLE_CLICK                    = _BV(7),


        // all events without SINGLE_CLICK/DOUBLE_CLICK
        ALL                             = UP|DOWN|REPEATED_CLICK|PRESSED|LONG_PRESSED|HOLD|HOLD_START|HOLD_RELEASE,

        PRESSED_LONG_HOLD_SINGLE_DOUBLE_CLICK = PRESSED|LONG_PRESSED|HOLD|SINGLE_CLICK|DOUBLE_CLICK,


        ANY                             = 0xffff,
        MAX_BITS                        = 10
    };

    class PushButton;

#if PIN_MONITOR_BUTTON_GROUPS

    // Object to manage a group of buttons with shared properties
    class SingleClickGroup {
    public:
        static constexpr uint16_t kClickRepeatCountMax = (1 << 15) - 1;

        SingleClickGroup(uint16_t singleClickTimeout) : _timerOwner(nullptr), _timer(0), _timeout(singleClickTimeout), _repeatCount(kClickRepeatCountMax), _timerRunning(false)  {}

        void pressed() {
            if (_repeatCount == kClickRepeatCountMax) {
                _repeatCount = 0;
                __LDBG_assert(_timerRunning == false);
            }
        }

        void released(const void *owner, uint32_t millis) {
            _timer = millis;
            _timerRunning = true;
            _timerOwner = owner; // replace owner
            if (_repeatCount < kClickRepeatCountMax - 1) {
                _repeatCount++;
            }
        }

        inline bool isTimerRunning(const void *owner) const {
            return (owner == nullptr || owner == _timerOwner) ? _timerRunning : false;
        }

        inline uint16_t getTimeout() const {
            return _timeout;
        }

        inline uint16_t getRepeatCount() const {
            return _repeatCount;
        }

        inline uint32_t getDuration() const {
            return get_time_diff(_timer, millis());
        }

        void stopTimer(const void *owner) {
            if (_timerOwner == owner) {
                _repeatCount = kClickRepeatCountMax;
                _timerRunning = false;
                _timerOwner = nullptr;
            }
        }

    protected:
        const void *_timerOwner;
        uint32_t _timer;
        // clicks in a row with less than _timeout milliseconds after the clicks
        // _repeatCount 1 = one click, 2 = double click, 3 tripple click etc...
        uint16_t _timeout;
        uint16_t _repeatCount : 15;
        uint16_t _timerRunning : 1;
    };

    class SingleClickGroupPtr : public std::shared_ptr<SingleClickGroup>
    {
    public:
        using PtrType = std::shared_ptr<SingleClickGroup>;
        using PtrType::reset;

        SingleClickGroupPtr() : PtrType() {}
        SingleClickGroupPtr(PtrType ptr) : PtrType(ptr) {}
        SingleClickGroupPtr(uint16_t singleClickTimeout) : PtrType(new SingleClickGroup(singleClickTimeout)) {}
        SingleClickGroupPtr(PushButton &button);

        inline void reset(uint16_t singleClickTimeout) {
            reset(new SingleClickGroup(singleClickTimeout));
        }
    };

#endif

    class PushButtonConfig {
    public:
        using EventType = PushButtonEventType;

    public:
        // PushButtonConfig() :
        //     _subscribedEvents(EventType::NONE),
        //     _clickTime(350),
        //     _singleClickSteps(1),
        //     _longpressTime(1500),
        //     _repeatTime(2500)
        // {
        // }

        PushButtonConfig(EventType subscribedEvents = EventType::NONE, uint16_t clickTime = 250, uint16_t longpressTime = 600, uint16_t repeatTime = 100, uint16_t singleClickSteps = 15) :
            _subscribedEvents(subscribedEvents),
            _clickTime(clickTime),
            _singleClickSteps(singleClickSteps),
            _longpressTime(longpressTime),
            _repeatTime(repeatTime)
        {
        }

        bool hasEvent(EventType event) const {
            return (static_cast<uint16_t>(_subscribedEvents) & static_cast<uint16_t>(event)) != 0;
        }

    protected:
        EventType _subscribedEvents;
        // short press or click < _clickTime milliseconds
        uint16_t _clickTime;
        // 100% / shortpressSteps = level change per click
        uint16_t _singleClickSteps;
        // long press < longpressTime milliseconds
        uint16_t _longpressTime;
        // button held >= longpressTime, repeated event every _repeatTime milliseconds
        uint16_t _repeatTime;
    };

    class PushButton : public Pin, public PushButtonConfig {
    public:
        PushButton() :
            Pin(0, nullptr, StateType::UP_DOWN, PIN_MONITOR_ACTIVE_STATE),
            PushButtonConfig(),
#if PIN_MONITOR_BUTTON_GROUPS
            _singleClickGroup(),
#endif
            _startTimer(0),
            _duration(0),
            _repeatCount(0),
            _startTimerRunning(false)
        {
        }

        PushButton(uint8_t pin, const void *arg, const PushButtonConfig &config,
#if PIN_MONITOR_BUTTON_GROUPS
            SingleClickGroupPtr singleClickGroup = SingleClickGroupPtr(),
#endif
            ActiveStateType activeLow = PIN_MONITOR_ACTIVE_STATE) :

            Pin(pin, arg, StateType::UP_DOWN, activeLow),
            PushButtonConfig(config),
#if PIN_MONITOR_BUTTON_GROUPS
            _singleClickGroup(singleClickGroup),
#endif
            _startTimer(0),
            _duration(0),
            _repeatCount(0),
            _startTimerRunning(false),
            _holdRepeat(false)

        {
        }

#if PIN_MONITOR_BUTTON_GROUPS
        void setSingleClickGroup(SingleClickGroupPtr singleClickGroup) {
            _singleClickGroup = singleClickGroup;
        }
#endif

    public:
        virtual void event(EventType eventType, uint32_t now);
        virtual void loop() override;

        // number of repeats for EventType::HOLD and EventType::REPEATED_CLICK
        uint16_t getRepeatCount() const {
            return _repeatCount;
        }

#if DEBUG
        virtual void dumpConfig(Print &output) override;
#endif

        static const __FlashStringHelper *eventTypeToString(EventType eventType);

    protected:
        // event for StateType is final
        virtual void event(StateType state, uint32_t now) final;

        void _buttonReleased();
        bool _fireEvent(EventType eventType);
        void _reset();

    protected:
#if PIN_MONITOR_BUTTON_GROUPS
        friend SingleClickGroupPtr;
        // pointer to the group settings
        SingleClickGroupPtr _singleClickGroup;
#endif

        uint32_t _startTimer;
        uint32_t _duration;
        uint16_t _repeatCount: 14;
        uint16_t _startTimerRunning: 1;
        uint16_t _holdRepeat: 1;
    };

    inline bool PushButton::_fireEvent(EventType eventType)
    {
        __LDBG_printf("event_type=%u has_event=%u", eventType, hasEvent(eventType));
        if (hasEvent(eventType)) {
            event(eventType, millis());
            return true;
        }
        return false;
    }

    inline void PushButton::_reset()
    {
        _startTimer = 0;
        _duration = 0;
        _repeatCount = 0;
        _startTimerRunning = false;
    }

}

#include <debug_helper_disable.h>

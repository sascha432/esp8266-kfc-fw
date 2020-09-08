/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "pin.h"
#include <memory>

namespace PinMonitor {

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
        HELD                            = _BV(4),
        REPEAT                          = _BV(5),

        // after EventType::UP before EventType::DOWN
        REPEATED_CLICK                  = _BV(6),
        // if SINGLE_CLICK is subscribed, REPEATED_CLICK won't be sent for _repeatCount == 1
        SINGLE_CLICK                    = _BV(7),
        // if DOUBLE_CLICK is subscribed, REPEATED_CLICK won't be sent for _repeatCount == 2
        DOUBLE_CLICK                    = _BV(8),


        ANY                             = 0xffff,
        MAX_BITS                        = 8
    };

    class PushButton;

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

        SingleClickGroupPtr() : std::shared_ptr<SingleClickGroup>() {}
        SingleClickGroupPtr(PtrType ptr) : std::shared_ptr<SingleClickGroup>(ptr) {}
        SingleClickGroupPtr(uint16_t singleClickTimeout) : PtrType(new SingleClickGroup(singleClickTimeout)) {}
        SingleClickGroupPtr(PushButton &button);

        inline void reset(uint16_t singleClickTimeout) {
            reset(new SingleClickGroup(singleClickTimeout));
        }
    };

    class PushButtonConfig {
    public:
        using EventType = PushButtonEventType;

    public:
        PushButtonConfig(EventType subscribedEvents = EventType::ANY, uint16_t clickTime = 250, uint16_t longpressTime = 600, uint16_t repeatTime = 100, uint16_t singleClickSteps = 15) :
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
        PushButton(uint8_t pin, const void *arg, PushButtonConfig &&config, SingleClickGroupPtr singleClickGroup = SingleClickGroupPtr(), ActiveStateType activeLow = ActiveStateType::PRESSED_WHEN_HIGH) :
            Pin(pin, arg, StateType::UP_DOWN, activeLow),
            PushButtonConfig(std::move(config)),
            _startTimer(0),
            _duration(0),
            _repeatCount(0),
            _startTimerRunning(false),
            _singleClickGroup(singleClickGroup)
        {
#if DEBUG_PIN_MONITOR
            setName(String((uint32_t)arg, 16) + ':' + String((uint32_t)this, 16));
#endif
        }

    public:
        virtual void event(EventType eventType, TimeType now);
        virtual void loop() override;

#if DEBUG
        virtual void dumpConfig(Print &output) override;
#endif

    protected:
        virtual void event(StateType state, TimeType now) final;

        void _buttonReleased();

        inline bool _fireEvent(EventType eventType) {
            if (hasEvent(eventType)) {
                event(eventType, millis());
                return true;
            }
            return false;
        }

        const __FlashStringHelper *eventTypeToString(EventType eventType);

    protected:
        friend SingleClickGroupPtr;

        uint32_t _startTimer;
        uint32_t _duration;
        uint16_t _repeatCount: 15;
        uint16_t _startTimerRunning: 1;
        // pointer to the group settings
        SingleClickGroupPtr _singleClickGroup;

#if DEBUG_PIN_MONITOR
    protected:
        void setName(const String &name) {
            _name = String(F("name=<")) + name + '>';
        }

        const char *name() const {
            return _name.c_str();
        }

    private:
        String _name;
#endif
    };

}

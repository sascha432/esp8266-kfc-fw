/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#include "pin.h"

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


        ANY                             = 0xffff
    };

    class PushButtonConfig {
    public:
        using EventType = PushButtonEventType;

    public:
        PushButtonConfig(EventType subscribedEvents = EventType::ANY, uint16_t shortpressTime = 250, uint16_t longpressTime = 600, uint16_t repeatTime = 150, uint16_t shortpressNoRepeatTime = 800) :
            _subscribedEvents(subscribedEvents),
            _shortpressTime(shortpressTime),
            _longpressTime(longpressTime),
            _repeatTime(repeatTime),
            _shortpressNoRepeatTime(shortpressNoRepeatTime)
        {
        }

        bool hasEvent(EventType event) const {
            return (static_cast<uint16_t>(_subscribedEvents) & static_cast<uint16_t>(event)) != 0;
        }

    protected:

        EventType _subscribedEvents;
        // short press or click < _shortpressTime milliseconds
        uint16_t _shortpressTime;
        // long press < longpressTime milliseconds
        uint16_t _longpressTime;
        // button held >= longpressTime, repeated event every _repeatTime milliseconds
        uint16_t _repeatTime;
        // clicks in a row with less than _shortpressNoRepeatTime milliseconds after the clicks
        // _clickRepeatCount 1 = one click, 2 = double click, 3 tripple click etc...
        uint16_t _shortpressNoRepeatTime;
    };

    class PushButton : public Pin, public PushButtonConfig {
    public:
            static constexpr uint16_t kClickRepeatCountMax = (1 << 14) - 1;
    public:

        PushButton(uint8_t pin, const void *arg, PushButtonConfig &&config, ActiveStateType activeLow = ActiveStateType::PRESSED_WHEN_HIGH) :
            Pin(pin, arg, StateType::UP_DOWN, activeLow),
            PushButtonConfig(std::move(config)),
            _startTimer(0),
            _clickRepeatTimer(0),
            _duration(0),
            _repeatCount(0),
            _clickRepeatCount(kClickRepeatCountMax),
            _startTimerRunning(false),
            _clickRepeatTimerRunning(false)
        {
#if DEBUG_PIN_MONITOR
            setName(String((uint32_t)arg, 16) + ':' + String((uint32_t)this, 16));
#endif
        }

    public:
        virtual void event(EventType eventType, TimeType now);
        virtual void loop() override;

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
        uint32_t _startTimer;
        uint32_t _clickRepeatTimer;
        uint32_t _duration;
        uint16_t _repeatCount;
        uint16_t _clickRepeatCount : 14;
        uint16_t _startTimerRunning : 1;
        uint16_t _clickRepeatTimerRunning : 1;

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

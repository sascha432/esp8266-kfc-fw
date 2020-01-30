/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_WEATHER_STATION_HAS_TOUCHPAD

#include <Arduino_compat.h>
#include <Adafruit_MPR121.h>
#include <PrintString.h>
#include "EnumBitset.h"

class Mpr121Touchpad;

typedef_enum_bitset(Mpr121TouchpadEventType, uint8_t,
    TOUCH       = 0x0001,
    RELEASED    = 0x0002,
    MOVE        = 0x0004,
    TAP         = 0x0008,
    PRESS       = 0x0010,
    HOLD        = 0x0020,
    SWIPE       = 0x0040,
    DRAG        = 0x0080
);

typedef_enum_bitset(Mpr121TouchpadGesturesType, uint8_t,
    UP =        0x01,
    DOWN =      0x02,
    LEFT =      0x04,
    RIGHT =     0x08,
    TAP =       0x10,
    PRESS =     0x20,
    DOUBLE =    0x40,
    MULTI =     0x80
);

class Mpr121Touchpad {
public:
    const uint16_t MULTIPLE_MIN_TIME = 10;
    const uint16_t MULTIPLE_MAX_TIME = 1000;
    const uint16_t REPEAT_TIME = 250;
    const uint16_t PRESS_TIME = 750;

    typedef Mpr121TouchpadEventType EventType;
    typedef Mpr121TouchpadGesturesType GesturesType;
    typedef int8_t Position;

    class Coordinates {
    public:
        Position x;
        Position y;

        Coordinates() {
            clear();
        }
        Coordinates(const Coordinates &coords) : x(coords.x), y(coords.y) {
        }
        void clear() {
            x = 0;
            y = 0;
        }
        operator bool() const {
            return (x != 0) || (y != 0);
        }
        Coordinates operator+(const Coordinates &coords) const {
            auto tmp = *this;
            tmp.x += coords.x;
            tmp.y += coords.y;
            return tmp;
        }
        Coordinates operator-(const Coordinates &coords) const {
            auto tmp = *this;
            tmp.x -= coords.x;
            tmp.y -= coords.y;
            return tmp;
        }
    };

    class Event {
    public:
        Event(Mpr121Touchpad &pad);
        EventType getType() const;
        uint32_t getDuration() const;
        bool isSwipeAny() const;
        bool isSwipeUp() const;
        bool isSwipeDown() const;
        bool isSwipeLeft() const;
        bool isSwipeRight() const;
        bool isTap() const;
        bool isDoubleTap() const;
        bool isPress() const;
        bool isDoublePress() const;
        uint8_t getTapCount() const;
        GesturesType getGestures() const;
        Coordinates getStart() const;
        Coordinates getEnd() const;
        Coordinates getSwipeDistance() const;
        void setBubble(bool bubble);

    private:
        void touched();
        void released();
        void move(const Coordinates &prev);
        void gestures();
        void timer();
        void read();
        bool within(uint32_t time, uint16_t min, uint16_t max) const {
            auto diff = get_time_diff(time, millis());
            return diff >= min && diff <= max;
        }

        String __toString() {
            PrintString str;
            if (_type) {
                str.printf_P(PSTR("t=%s dxy=%d,%d:%d,%d sxy=%d:%d start=%d:%d-%d:%d cnt=%u g=%s"),
                    _type.toString().c_str(),
                    _sumN.x, _sumP.x, _sumN.y, _sumP.y,
                    (_sumP + _sumN).x, (_sumP + _sumN).y,
                    _start.x, _start.y,
                    _position.x, _position.y,
                    _counter, getGestures().toString().c_str()
                );
                // str.printf_P(PSTR("tap=%u press=%u counter=%u swipe left/right=%u/%u swipe up/down=%u/%u"),
                //     isTap(),
                //     isPress(),
                //     _counter,
                //     isSwipeLeft(),
                //     isSwipeRight(),
                //     isSwipeUp(),
                //     isSwipeDown()
                // );
            }
            return str;
        }

    private:
        friend Mpr121Touchpad;

        Mpr121Touchpad &_pad;

        EventType _type;
        Coordinates _position;
        Coordinates _start;
        Coordinates _sumP;
        Coordinates _sumN;
        Coordinates _swipeDirection;
        uint32_t _touchedTime;
        uint32_t _releasedTime;
        uint32_t _heldTime;
        uint32_t _lastTap;
        uint32_t _lastPress;
        uint8_t _counter;
        bool _press;
        bool _bubble;
    };

    typedef std::function<void(const Event &event)> Callback_t;

    class CallbackEvent {
    public:
        CallbackEvent(uint32_t _id, EventType _events, Callback_t _callback) : id(_id), events(_events), callback(_callback) {
        }
        uint32_t id;
        EventType events;
        Callback_t callback;
    };

    typedef std::vector<CallbackEvent> CallbackEventVector;

    Mpr121Touchpad();
    ~Mpr121Touchpad();

    bool begin(uint8_t address, uint8_t irqPin, TwoWire *wire = &Wire);
    void end();

    // returns true until get() has been called to retrieve the updated coordinates
    bool isEventPending() const;

    void processEvents();

    // retrieve coordinates
    void get(Coordinates &coords);
    const Coordinates &get() const;

    void addCallback(EventType events, uint32_t id, Callback_t callback);
    void removeCallback(EventType events, uint32_t id);

    Adafruit_MPR121 &getMPR121() {
        return _mpr121;
    }

private:
    void _loop();
    void _get();
    void _fireEvent();

private:
    Adafruit_MPR121 _mpr121;
    uint8_t _address;
    uint8_t _irqPin;

    CallbackEventVector _callbacks;
    Event _event;
};

#endif

/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_WEATHER_STATION_HAS_TOUCHPAD

#include <Arduino_compat.h>
#include <Adafruit_MPR121.h>
#include <EventScheduler.h>
#include <PrintString.h>
#include <EventScheduler.h>
#include <FixedCircularBuffer.h>
#include "plugins/http2serial/http2serial.h"

#ifndef DEBUG_TOUCHPAD
#define DEBUG_TOUCHPAD                      0
#endif

class Mpr121Touchpad;
class WeatherStationPlugin;

class Mpr121Timer : public OSTimer {
public:
    virtual void run() override;
};

class Mpr121Touchpad {
public:
    using EventBaseType = uint8_t;
    using GesturesBaseType = uint8_t;

    enum EventType : EventBaseType {
        NONE        = 0,
        TOUCH       = 0x01,
        RELEASED    = 0x02,
        MOVE        = 0x04,
        TAP         = 0x08,
        PRESS       = 0x10,
        HOLD        = 0x20,
        SWIPE       = 0x40,
        DRAG        = 0x80,
        ANY         = 0xff
    };

    enum class GesturesType : GesturesBaseType {
        NONE =      0,
        UP =        0x01,
        DOWN =      0x02,
        LEFT =      0x04,
        RIGHT =     0x08,
        TAP =       0x10,
        PRESS =     0x20,
        DOUBLE =    0x40,
        MULTI =     0x80
    };

    using Position = int8_t;

    static constexpr uint16_t kMultiMinTime = 10;
    static constexpr uint16_t kMultiMaxTime = 1000;
    static constexpr uint16_t kRepeatTime = 250;
    static constexpr uint16_t kPressTime = 750;

    static constexpr Position kMinX = 1;
    static constexpr Position kMinY = 1;
    static constexpr Position kMaxX = 14;
    static constexpr Position kMaxY = 8;
    static constexpr Position kPredictionZone = 1;
    static constexpr bool kInvertX = true;
    static constexpr bool kInvertY = true;

    class TouchpadEvent_t {
    public:
        TouchpadEvent_t() : touched(0), time(0) {}
        TouchpadEvent_t(uint16_t aTouched, uint32_t aTime) : touched(aTouched), time(aTime) {}

        struct __attribute__packed__ {
            uint16_t touched;
            uint32_t time;
        };
    };

    using ReadBuffer = FixedCircularBuffer<TouchpadEvent_t, 64> ;

    class Coordinates {
    public:
        Position x;
        Position y;

        Coordinates() {
            clear();
        }
        Coordinates(Position x, Position y) : x(x), y(y) {
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

    class Movement {
    public:
        using EventType = Mpr121Touchpad::EventType;

        Movement(uint32_t eventId, const Coordinates &coords, uint32_t time, EventType type) : _eventId(eventId), _coords(coords), _time(time), _type(type) {
        }

        Coordinates &getCoords() {
            return _coords;
        }

        Position getX() const {
            return _coords.x;
        }

        Position getY() const {
            return _coords.y;
        }

        uint32_t getTime() const {
            return _time;
        }

        EventType getType() const {
            return _type;
        }

        void setType(EventType type) {
            _type = type;
        }

        uint32_t getEventId() const {
            return _eventId;
        }

    private:
        uint32 _eventId;
        Coordinates _coords;
        uint32_t _time;
        EventType _type;
    };

    using MovementVector = std::vector<Movement>;

    class Event {
    public:
        using EventType = Mpr121Touchpad::EventType;
        using Coordinates = Mpr121Touchpad::Coordinates;
        using MovementVector = Mpr121Touchpad::MovementVector;

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
        void addMovement();

        void broadcastData(const Movement &movement);

    private:
        friend WeatherStationPlugin;

        void touched();
        void released();
        void move(const Coordinates &prev);
        void gestures();
        void timer();
        void read(ReadBuffer::iterator &iterator);

        bool within(uint32_t time, uint16_t min, uint16_t max, uint32_t now) const {
            auto diff = get_time_diff(time, now);
            return diff >= min && diff <= max;
        }

        String __toString() {
            PrintString str;
            if (_type != EventType::NONE) {
                // PrintString movementsStr;
                // movementsStr.printf_P(PSTR("%u:"), _movements.size());
                // for(auto &movement: _movements) {
                //     movementsStr.printf_P(PSTR("(%d,%d)"), movement.getCoords().x, movement.getCoords().y);
                // }

                //str.printf_P(PSTR("x:y=% 5d % 5d - % 5d % 5d"), _position.x, _position.y, _predict.x, _predict.y);

                // str.printf_P(PSTR("t=%s sumN=%d:%d sumP=%d:%d sum=%d:%d start-end=%d:%d-%d:%d swipe=%d:%d cnt=%u g=%s"),
                //     _type.toString().c_str(),
                //     _sumN.x, _sumP.x, _sumN.y, _sumP.y,
                //     (_sumP + _sumN).x, (_sumP + _sumN).y,
                //     _start.x, _start.y,
                //     _position.x, _position.y,
                //     getSwipeDistance().x, getSwipeDistance().y,
                //     getTapCount(), getGestures().toString().c_str()
                //     // movementsStr.c_str()
                // );
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
        uint32_t _eventId;
        Coordinates _position;
        Coordinates _predict;
        Coordinates _start;
        Coordinates _sumP;
        Coordinates _sumN;
        Coordinates _swipe;
        uint32_t _touchedTime;
        uint32_t _releasedTime;
        uint32_t _heldTime;
        uint32_t _lastTap;
        uint32_t _lastPress;
        uint8_t _counter;
        bool _press;
        // bool _bubble;
        MovementVector _movements;
        TouchpadEvent_t _curEvent;
    };

    using Callback = std::function<bool(const Event &event)>;       // return true to stop bubbling events to other callbacks

    class CallbackEvent {
    public:
        CallbackEvent(uint32_t _id, EventType _events, Callback _callback) : id(_id), events(_events), callback(_callback) {
        }
        uint32_t id;
        EventType events;
        Callback callback;
    };

    using CallbackEventVector = std::vector<CallbackEvent>;

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

    void addCallback(EventType events, uint32_t id, Callback callback);
    void removeCallback(EventType events, uint32_t id);

    Adafruit_MPR121 &getMPR121() {
        return _mpr121;
    }

private:
    void _loop();
    void _get();
    void _fireEvent();
    void _timerCallback(::Event::CallbackTimerPtr timer);

private:
    friend WeatherStationPlugin;
    friend Mpr121Timer;

    Adafruit_MPR121 _mpr121;
    uint8_t _address;
    uint8_t _irqPin;

    CallbackEventVector _callbacks;
    Event _event;
    Mpr121Timer _timer;
};

#endif

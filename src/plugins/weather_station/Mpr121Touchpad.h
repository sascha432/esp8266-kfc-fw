/**
 * Author: sascha_lammers@gmx.de
 */

#pragma once

#if IOT_WEATHER_STATION_HAS_TOUCHPAD

#include <Arduino_compat.h>
#include <Adafruit_MPR121.h>
#include <PrintString.h>
#include <EventScheduler.h>
#include <FixedCircularBuffer.h>
#include "plugins/http2serial/http2serial.h"
#include "EnumBitset.h"

class Mpr121Touchpad;
class WeatherStationPlugin;

DECLARE_ENUM_BITSET(Mpr121TouchpadEventType, uint8_t,
    TOUCH       = 0x0001,
    RELEASED    = 0x0002,
    MOVE        = 0x0004,
    TAP         = 0x0008,
    PRESS       = 0x0010,
    HOLD        = 0x0020,
    SWIPE       = 0x0040,
    DRAG        = 0x0080
);

DECLARE_ENUM_BITSET(Mpr121TouchpadGesturesType, uint8_t,
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
    typedef int8_t Position;

    const uint16_t MULTIPLE_MIN_TIME = 10;
    const uint16_t MULTIPLE_MAX_TIME = 1000;
    const uint16_t REPEAT_TIME = 250;
    const uint16_t PRESS_TIME = 750;

    static const Position _MinX = 1;
    static const Position _MinY = 1;
    static const Position _MaxX = 14;
    static const Position _MaxY = 8;
    static const Position _PredictionZone = 1;
    static const bool _InvertX = true;
    static const bool _InvertY = true;

    typedef struct  {
        uint16_t touched;
        uint32_t time;
    } TouchpadEvent_t;

    typedef FixedCircularBuffer<TouchpadEvent_t, 64> ReadBuffer;

    typedef Mpr121TouchpadEventType EventType;
    typedef Mpr121TouchpadGesturesType GesturesType;

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

    typedef std::vector<Movement> MovementVector;

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
            if (_type) {
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
        bool _bubble;
        MovementVector _movements;
        TouchpadEvent_t _curEvent;
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
    void _timerCallback(EventScheduler::TimerPtr timer);

private:
    friend WeatherStationPlugin;
    friend void mpr121_timer(EventScheduler::TimerPtr timer);

    Adafruit_MPR121 _mpr121;
    uint8_t _address;
    uint8_t _irqPin;

    CallbackEventVector _callbacks;
    Event _event;
    EventScheduler::Timer _timer;
};

#endif

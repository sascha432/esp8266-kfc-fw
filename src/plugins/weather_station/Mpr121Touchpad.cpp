/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_WEATHER_STATION_HAS_TOUCHPAD

#include <Arduino_compat.h>
#include <MicrosTimer.h>
#include "Mpr121Touchpad.h"
#include "LoopFunctions.h"

#include <map>

volatile bool mpr121_irq_callback_flag;
Mpr121Touchpad *touchpad = nullptr;

static Mpr121Touchpad::ReadBuffer buffer;

void ICACHE_RAM_ATTR mpr121_irq_callback()
{
    mpr121_irq_callback_flag = true;
}

void ICACHE_RAM_ATTR mpr121_timer(EventScheduler::TimerPtr timer)
{
    if (mpr121_irq_callback_flag) {
        mpr121_irq_callback_flag = false;
        buffer.push_back(Mpr121Touchpad::TouchpadEvent_t({touchpad->_mpr121.touched(), millis()}));
    }
}

DEFINE_ENUM_BITSET(Mpr121TouchpadEventType);
DEFINE_ENUM_BITSET(Mpr121TouchpadGesturesType);

Mpr121Touchpad::Event::Event(Mpr121Touchpad &pad) : _pad(pad), _curEvent()
{
    _touchedTime = 0;
    _releasedTime = 0;
    _lastTap = 0;
    _lastPress = 0;
    _counter = 0;
}

Mpr121Touchpad::EventType Mpr121Touchpad::Event::getType() const
{
    return _type;
}

uint32_t Mpr121Touchpad::Event::getDuration() const
{
    return _releasedTime - _touchedTime;
}

bool Mpr121Touchpad::Event::isSwipeAny() const
{
    return _swipe == true;
}

bool Mpr121Touchpad::Event::isSwipeUp() const
{
    return _swipe.y > 0;
}

bool Mpr121Touchpad::Event::isSwipeDown() const
{
    return _swipe.y < 0;
}

bool Mpr121Touchpad::Event::isSwipeLeft() const
{
    return _swipe.x < 0;
}

bool Mpr121Touchpad::Event::isSwipeRight() const
{
    return _swipe.y > 0;
}

Mpr121Touchpad::GesturesType Mpr121Touchpad::Event::getGestures() const
{
    GesturesType value;
    if (_swipe == false) {
        if (isTap()) {
            value += GesturesType::BITS::TAP;
        }
        if (isPress()) {
            value += GesturesType::BITS::PRESS;
        }
        if (value) {
            if (_counter == 2) {
                value += GesturesType::BITS::DOUBLE;
            } else if (_counter > 2) {
                value += GesturesType::BITS::MULTI;
            }
        }
    }
    else {
        if (_swipe.y > 0) {
            value += GesturesType::BITS::UP;
        }
        if (_swipe.y < 0) {
            value += GesturesType::BITS::DOWN;
        }
        if (_swipe.x < 0) {
            value += GesturesType::BITS::LEFT;
        }
        if (_swipe.x > 0) {
            value += GesturesType::BITS::RIGHT;

        }
    }
    return value;
}

Mpr121Touchpad::Coordinates Mpr121Touchpad::Event::getStart() const
{
    return _start;
}

Mpr121Touchpad::Coordinates Mpr121Touchpad::Event::getEnd() const
{
    return _position;
}

Mpr121Touchpad::Coordinates Mpr121Touchpad::Event::getSwipeDistance() const
{
    return _swipe;
}

bool Mpr121Touchpad::Event::isTap() const
{
    return _swipe == false && _press == false;
}

bool Mpr121Touchpad::Event::isDoubleTap() const
{
    return isTap() && _counter == 2;
}

bool Mpr121Touchpad::Event::isPress() const
{
    return _swipe == false && _press == true;
}

bool Mpr121Touchpad::Event::isDoublePress() const
{
    return isPress() && _counter == 2;
}

uint8_t Mpr121Touchpad::Event::getTapCount() const {
    return _counter;
}

void Mpr121Touchpad::Event::setBubble(bool bubble)
{
    _bubble = bubble;
}

void Mpr121Touchpad::Event::addMovement()
{
    if (_movements.size() < 100) {
        if (_type) {
            _movements.emplace_back(0, _position, _curEvent.time, _type);
            broadcastData(_movements.back());
        }
    }
}

void Mpr121Touchpad::Event::broadcastData(const Movement &movement)
{
    auto wsSerialConsole = Http2Serial::getConsoleServer();
    if (wsSerialConsole && !wsSerialConsole->getClients().isEmpty()) {
        typedef struct {
            uint16_t packetId;
            int16_t x;
            int16_t y;
            int16_t px;
            int16_t py;
            uint32_t time;
            uint8_t type;
        } Packet_t;
        Packet_t data = {0x200, movement.getX(), movement.getY(), _predict.x, _predict.y, movement.getTime(), movement.getType().getValue()};

        for(auto client: wsSerialConsole->getClients()) {
            if (client->status() && client->_tempObject && reinterpret_cast<WsClient *>(client->_tempObject)->isAuthenticated()) {
                if (client->canSend()) {
                    client->binary(reinterpret_cast<uint8_t *>(&data), sizeof(data));
                }
            }
        }
    }
}

void Mpr121Touchpad::Event::touched()
{
    if (_lastTap) {
        if (!within(_lastTap, 50, 1000, _curEvent.time)) {
            _lastTap = 0;
            _counter = 0;
        }
    }
    if (_lastPress) {
        if (!within(_lastPress, 50, 1000, _curEvent.time)) {
            _lastPress = 0;
            _counter = 0;
        }
    }
    _type = EventType::BITS::TOUCH;
    if (get_time_diff(_releasedTime, _curEvent.time) > 500) {
        _debug_println(F("removing predicted position"));
        _predict = Coordinates(-1, -1);
    }

    _touchedTime = _curEvent.time;
    _heldTime = _touchedTime;
    _releasedTime = 0;
    _sumN.clear();
    _sumP.clear();
    _swipe.clear();
    _bubble = true;
    _press = false;
    _start = _position;
    _movements.clear();
    addMovement();
    _pad._fireEvent();
}

void Mpr121Touchpad::Event::released()
{
    _type = EventType::BITS::RELEASED;
    _releasedTime = _curEvent.time;
    gestures();
    addMovement();
    _pad._fireEvent();
    _touchedTime = 0;
    _movements.clear();
}

void Mpr121Touchpad::Event::gestures()
{
    // for(auto &movement: _movements) {
    //     auto coords = movement.getCoords();
    // }

    auto duration = getDuration();
    Position moveX = _sumP.x + _sumN.x;
    Position moveY = _sumP.y + _sumN.y;
    if (
        _sumP.x <= 5 && _sumN.x >= -5 && moveX <= 2 &&
        _sumP.y <= 5 && _sumN.y >= -5 && moveY <= 2
    )
    {
        if (_type & EventType::BITS::RELEASED) {
            if (duration <= 1000) {
                _type += EventType::BITS::TAP;
                if (_lastTap) {
                    _counter++;
                }
                _lastTap = _curEvent.time;
                _lastPress = 0;
            } else {
                _type += EventType::BITS::PRESS;
                if (_lastPress) {
                    _counter++;
                }
                _lastPress = _curEvent.time;
                _lastTap = 0;
            }
        }
        else if (duration > 250) {
            _type += EventType::BITS::HOLD;
        }
    }
    else {
        if (moveX * 3 > _sumP.x || moveX * 3 < -_sumN.x) {
            _swipe.x = moveX;
        }
        if (moveY * 3 > _sumP.x || moveY * 3 < -_sumN.y) {
            _swipe.y = -moveY;
        }
    }

    // debug_println_notempty(__toString());

    if (_type & EventType::BITS::RELEASED) {
        if (_swipe.x  || _swipe.y) {
            _type += EventType::BITS::SWIPE;
            _lastTap = 0;
            _lastPress = 0;
            _counter = 0;
        }
    }
    else if (duration > 1000 && _type & EventType::BITS::MOVE) {
        if (_swipe.x  || _swipe.y) {
            _type += EventType::BITS::DRAG;
        }
    }
    addMovement();
}

void Mpr121Touchpad::Event::timer()
{
    if (_touchedTime != 0 && get_time_diff(_heldTime, _curEvent.time) > 250) {
        gestures();
        _pad._fireEvent();
        _heldTime = _curEvent.time;
    }
}

void Mpr121Touchpad::Event::move(const Coordinates &prev)
{
    Position moveX = 0;
    Position moveY = 0;
    if (_position.x && prev.x) {
        moveX = _position.x - prev.x;
        if (moveX < 0) {
            _sumN.x += moveX;
        }
        else {
            _sumP.x += moveX;
        }
    }
    if (_position.y && prev.x) {
        moveY = _position.y - prev.y;
        if (moveY < 0) {
            _sumN.y += moveY;
        }
        else {
            _sumP.y += moveY;
        }
    }
    _type = EventType::BITS::MOVE;
    gestures();
}

void Mpr121Touchpad::Event::read(ReadBuffer::iterator &iterator)
{
    if (iterator != buffer.end()) {
        _curEvent = *iterator;

        auto touched = _curEvent.touched;
        auto touchedX = touched & ~(1 << 8);             // 0-7 for x 0=right, 7=left
        auto touchedY = (touched >> 8) & ~(1 << 4);      // 8-11 for y 11=top, 8 bottom

        // Serial.printf("t %016.16s - %08.8s %04.4s\n", String(touched, 2).c_str(), String(touchedX, 2).c_str(), String(touchedY, 2).c_str());

        #define _BV1(a, b)          ((touched & (_BV(a)|_BV(b))) == (_BV(a)))                   // a == true && b == false
        #define _BV2(a, b)          ((touched & (_BV(a)|_BV(b))) == (_BV(a)|_BV(b)))            // a == true && b == true

        static int ___min[10]={20,20,20,20,20,20,20};
        static int ___max[10]={0,0,0,0,0,0,0,0};

        // get coordinates
        Position y = -1;
        touched = touchedY << 1;
        for (uint8_t i = 0; i < 4; i++) {
            // Serial.printf("_BV1=%u=%u %u=%u i=%u y=%u\n", i + 1, 1, i, 0, i, (i * 2));
            // Serial.printf("_BV2=%u=%u %u=%u i=%u y=%u\n", i + 1, 1, i, 1, i, 1 + (i * 2));
            if (_BV1(i + 1, i)) {
                y = (_MinY - 0) + (i * 2);
            }
            if (_BV2(i + 1, i)) {
                y = (_MinY - 1) + (i * 2);
            }
        }
        if (y>0) {
        ___min[0] = std::min(___min[0], (int)y);
        ___max[0] = std::max(___max[0], (int)y);
        }
        if (_BV1(3, 4)) {
            // debug_printf("y %u %u\n", y, _MaxY);
            y = _MaxY + 1;
        }
        if (y>0) {
        ___min[1] = std::min(___min[1], (int)y);
        ___max[1] = std::max(___max[1], (int)y);
        }

        Position x = -1;
        touched = touchedX << 1;
        for (uint8_t i = 0; i < 8; i++) {
            // Serial.printf("_BV1=%u=%u %u=%u i=%u x=%u\n", i + 1, 1, i, 0, i, 1 + (i * 2));
            // Serial.printf("_BV2=%u=%u %u=%u i=%u x=%u\n", i + 1, 1, i, 1, i, 2 + (i * 2));
            if (_BV1(i + 1, i)) {
                x = (_MinX - 1) + (i * 2);
            }
            if (_BV2(i + 1, i)) {
                x = (_MinX - 2) + (i * 2);
            }
        }
        if (x>0) {
        ___min[2] = std::min(___min[2], (int)x);
        ___max[2] = std::max(___max[2], (int)x);
        }
        if (_BV1(7, 8)) {
            // debug_printf("x %u %u\n", x, _MaxX);
            x = _MaxX;
        }
        if (x>0) {
        ___min[3] = std::min(___min[3], (int)x);
        ___max[3] = std::max(___max[3], (int)x);
        }

#if 1
        static int8_t *_coords = nullptr;
        // std::map<String, int> __map;

        // __map[PrintString("% 8.8s", touchedX)] = x;
        // __map[PrintString("% 4.4s", touchedY)] = y;

        if (!_coords) {
            _coords = (int8_t *)calloc(_MaxX+4, _MaxY+4);
        }
// +plgi=weather
        if (x > 0 && y >0) {
            _coords[x + _MaxX * y]++;
            static time_t xtime = 0;
            if(millis() > xtime) {
                MySerial.println("--------------------------------");
                for(int j = 0; j < _MaxY + 3;j++) {
                    MySerial.printf("% 2d: ", j);
                    for(int i = 0; i < _MaxX + 3; i++) {
                        MySerial.printf("% 3d ", (int)_coords[j * _MaxX + i]);
                    }
                    MySerial.println();
                }
                MySerial.println("--------------------------------");
                for(int i = 0; i <4;i++) {
                    MySerial.printf("min/max %u: %d %d\n", i, ___min[i], ___max[i]);
                }
                // for(auto &m: __map) {
                //     str.printf("%s = %u\n", m.first.c_str(), m.second);
                // }

                xtime = millis() + 2000;
            }
        }
#endif
        // invert x/y
        if (x >= _MinX && _InvertX) {
            x = _MaxX + _MinX - x;
        }
        if (y >= _MinY && _InvertY) {
            y = _MaxY + _MinY - y;
        }

        if (y < _MinY) {
            if (_position.y) {
                if (_position.y >= _MaxY - _PredictionZone) {
                    _predict.y = _MaxY;
                }
                else if (_position.y <= _MinY + _PredictionZone) {
                    _predict.y = _MinY;
                }
            }
            _position.y = 0;
        }
        else {
            _position.y = y;
            _predict.y = y;
        }

        if (x < _MinX) {
            if (_position.x) {
                if (_position.x >= _MaxX - _PredictionZone) {
                    _predict.x = _MaxX;
                }
                else if (_position.x <= _MinX + _PredictionZone) {
                    _predict.x = _MinX;
                }
            }
            _position.x = 0;
        }
        else {
            _position.x = x;
            _predict.x = x;
        }
    }
}

Mpr121Touchpad::Mpr121Touchpad() : _irqPin(0), _event(*this)
{
}

Mpr121Touchpad::~Mpr121Touchpad()
{
    end();
}

bool Mpr121Touchpad::begin(uint8_t address, uint8_t irqPin, TwoWire *wire)
{
    if (_irqPin) {
        return false;
    }
    _address = address;
    if (!_mpr121.begin(address, wire)) {
        return false;
    }

    touchpad = this;

    _irqPin = irqPin;
    mpr121_irq_callback_flag = false;
    _event._type = EventType::NONE;
    pinMode(_irqPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(_irqPin), mpr121_irq_callback, CHANGE);

    _mpr121.setThresholds(4, 4);

    // hardware timer to read the data from the sensor into "buffer"
    // min. time is 5ms
    _timer.add(5, true, mpr121_timer, EventScheduler::PRIO_HIGH);

    LoopFunctions::add([this]() {
        _loop();
    }, this);
    return true;
}

void Mpr121Touchpad::end()
{
    LoopFunctions::remove(this);

    if (_irqPin) {
        detachInterrupt(digitalPinToInterrupt(_irqPin));
        _irqPin = 0;
    }
    _timer.remove();
    touchpad = nullptr;
}

bool Mpr121Touchpad::isEventPending() const
{
    return buffer.begin() != buffer.end();
}

void Mpr121Touchpad::get(Coordinates &coords)
{
    processEvents();
    coords = _event._position;
}

const Mpr121Touchpad::Coordinates &Mpr121Touchpad::get() const
{
    return _event._position;
}

void Mpr121Touchpad::addCallback(EventType events, uint32_t id, Callback_t callback)
{
    _callbacks.emplace_back(id, events, callback);
}

void Mpr121Touchpad::removeCallback(EventType events, uint32_t id)
{
    _callbacks.erase(std::remove_if(_callbacks.begin(), _callbacks.end(), [id, events](CallbackEvent &callback) {
        if (callback.id == id) {
            callback.events -= events;
            if (!callback.events) {
                return true;
            }
        }
        return false;
    }));
}

void Mpr121Touchpad::processEvents()
{
    ReadBuffer::iterator iterator = buffer.begin();
    int n = 0;
    if (iterator != buffer.end()) {
        // auto start = millis();
        // static int event_counter=0;
        // uint32_t lagg  = 0;
        do {
            auto previous = _event._position;
            // lagg = std::max(lagg, (uint32_t)millis() - (*iterator).time);

            _event.read(iterator);
            if (previous == false && _event._position == true) {
                _event.touched();
            }
            else if (_event._position == false && previous == true) {
                _event.released();
            }
            else if (!_event._releasedTime) {
                _event.move(previous);
            }
            n++;
        } while(++iterator != buffer.end());

        // event_counter += n;
        // _debug_printf_P(PSTR("read %u (%u) events, dur %lu, size %u, max. lag %d\n"), n, event_counter, millis() - start, buffer.size(), lagg);

        noInterrupts();
        buffer.shrink(iterator, buffer.end()); // <1µs
        interrupts();
    }
}

void Mpr121Touchpad::_loop()
{
    processEvents();
    _event.timer();
}

void Mpr121Touchpad::_fireEvent()
{
    debug_println_notempty(_event.__toString());
    if (_event._type && _event._bubble) {
        for(const auto &callback: _callbacks) {
            if (callback.events == _event.getType()) {
                debug_printf_P(PSTR("callback=%p\n"), &callback.callback);
                callback.callback(_event);
            }
        }
        _event.addMovement();
        _event._type = EventType::NONE;
    }
}


#endif

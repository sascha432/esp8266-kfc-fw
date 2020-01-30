/**
 * Author: sascha_lammers@gmx.de
 */

#if IOT_WEATHER_STATION_HAS_TOUCHPAD

#include <Arduino_compat.h>
#include <MicrosTimer.h>
#include "Mpr121Touchpad.h"
#include "LoopFunctions.h"

volatile bool mpr121_irq_callback_flag;

void ICACHE_RAM_ATTR mpr121_irq_callback()
{
    mpr121_irq_callback_flag = true;
}

define_enum_bitset(Mpr121TouchpadEventType);
define_enum_bitset(Mpr121TouchpadGesturesType);

Mpr121Touchpad::Event::Event(Mpr121Touchpad &pad) : _pad(pad)
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
    return _swipeDirection == true;
}

bool Mpr121Touchpad::Event::isSwipeUp() const
{
    return _swipeDirection.y > 0;
}

bool Mpr121Touchpad::Event::isSwipeDown() const
{
    return _swipeDirection.y < 0;
}

bool Mpr121Touchpad::Event::isSwipeLeft() const
{
    return _swipeDirection.x < 0;
}

bool Mpr121Touchpad::Event::isSwipeRight() const
{
    return _swipeDirection.y > 0;
}

Mpr121Touchpad::GesturesType Mpr121Touchpad::Event::getGestures() const
{
    GesturesType value;
    if (_swipeDirection == false) {
        if (isTap()) {
            value += GesturesType::BIT::TAP;
        }
        if (isPress()) {
            value += GesturesType::BIT::PRESS;
        }
        if (value) {
            if (_counter == 2) {
                value += GesturesType::BIT::DOUBLE;
            } else if (_counter > 2) {
                value += GesturesType::BIT::MULTI;
            }
        }
    }
    else {
        if (_swipeDirection.y > 0) {
            value += GesturesType::BIT::UP;
        }
        if (_swipeDirection.y < 0) {
            value += GesturesType::BIT::DOWN;
        }
        if (_swipeDirection.x < 0) {
            value += GesturesType::BIT::LEFT;
        }
        if (_swipeDirection.y > 0) {
            value += GesturesType::BIT::RIGHT;

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
    return _sumP + _sumN;
}

bool Mpr121Touchpad::Event::isTap() const
{
    return _swipeDirection == false && _press == false;
}

bool Mpr121Touchpad::Event::isDoubleTap() const
{
    return isTap() && _counter == 2;
}

bool Mpr121Touchpad::Event::isPress() const
{
    return _swipeDirection == false && _press == true;
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


void Mpr121Touchpad::Event::touched()
{
    if (_lastTap) {
        if (!within(_lastTap, 50, 1000)) {
            _lastTap = 0;
            _counter = 0;
        }
    }
    if (_lastPress) {
        if (!within(_lastPress, 50, 1000)) {
            _lastPress = 0;
            _counter = 0;
        }
    }
    _type = EventType::BIT::TOUCH;
    _touchedTime = millis();
    _releasedTime = 0;
    _heldTime = millis();
    _sumN.clear();
    _sumP.clear();
    _swipeDirection.clear();
    _bubble = true;
    _press = false;
    _start = _position;
    _pad._fireEvent();
}

void Mpr121Touchpad::Event::released()
{
    _type = EventType::BIT::RELEASED;
    _releasedTime = millis();
    gestures();
    _pad._fireEvent();
    _touchedTime = 0;
}

void Mpr121Touchpad::Event::gestures()
{
    auto duration = getDuration();
    Position moveX = _sumP.x + _sumN.x;
    Position moveY = _sumP.y + _sumN.y;
    if (
        _sumP.x <= 5 && _sumN.x >= -5 && moveX <= 2 &&
        _sumP.y <= 5 && _sumN.y >= -5 && moveY <= 2
    )
    {
        if (_type & EventType::BIT::RELEASED) {
            if (duration <= 1000) {
                _type += EventType::BIT::TAP;
                if (_lastTap) {
                    _counter++;
                }
                _lastTap = millis();
                _lastPress = 0;
            } else {
                _type += EventType::BIT::PRESS;
                if (_lastPress) {
                    _counter++;
                }
                _lastPress = millis();
                _lastTap = 0;
            }
        }
        else if (duration > 250) {
            _type += EventType::BIT::HOLD;
        }
    }
    else {
        if (moveX * 2 > _sumP.x || moveX * 2 < -_sumN.x) {
            _swipeDirection.x = moveX > 0 ? 1 : -1;
        }
        if (moveY * 2 > _sumP.x || moveY * 2 < -_sumN.y) {
            _swipeDirection.y = moveY < 0 ? 1 : -1;
        }
    }

    // debug_println_notempty(__toString());

    if (_type & EventType::BIT::RELEASED) {
        if (_swipeDirection.x  || _swipeDirection.y) {
            _type += EventType::BIT::SWIPE;
            _lastTap = 0;
            _lastPress = 0;
            _counter = 0;
        }
    }
    else if (duration > 1000 && _type & EventType::BIT::MOVE) {
        if (_swipeDirection.x  || _swipeDirection.y) {
            _type += EventType::BIT::DRAG;
        }
    }
}

void Mpr121Touchpad::Event::timer()
{
    if (_touchedTime != 0 && get_time_diff(_heldTime, millis()) > 250) {
        gestures();
        _pad._fireEvent();
        _heldTime = millis();
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
    _type = EventType::BIT::MOVE;
    gestures();
}

void Mpr121Touchpad::Event::read()
{
    if (mpr121_irq_callback_flag) {
        mpr121_irq_callback_flag = false;

        // _mpr121.begin(_address);
        auto touched = _pad._mpr121.touched();

        if ((touched & (_BV(11)|_BV(10))) == (_BV(11)|_BV(10))) {
            _position.y = 2;
        }
        else if (touched & _BV(11)) {
            _position.y = 1;
        }
        else if ((touched & (_BV(10)|_BV(9))) == (_BV(10)|_BV(9))) {
            _position.y = 4;
        }
        else if (touched & _BV(10)) {
            _position.y = 3;
        }
        else if ((touched & (_BV(9)|_BV(8))) == (_BV(9)|_BV(8))) {
            _position.y = 6;
        }
        else if (touched & (_BV(9))) {
            _position.y = 5;
        }
        else if (touched & (_BV(8))) {
            _position.y = 7;
        }
        else {
            _position.y = 0;
        }

        if ((touched & (_BV(7)|_BV(6))) == (_BV(7)|_BV(6))) {
            _position.x = 2;
        }
        else if (touched & _BV(7)) {
            _position.x = 1;
        }
        else if ((touched & (_BV(6)|_BV(5))) == (_BV(6)|_BV(5))) {
            _position.x = 4;
        }
        else if (touched & _BV(6)) {
            _position.x = 3;
        }
        else if ((touched & (_BV(5)|_BV(4))) == (_BV(5)|_BV(4))) {
            _position.x = 6;
        }
        else if (touched & _BV(5)) {
            _position.x = 5;
        }
        else if ((touched & (_BV(4)|_BV(3))) == (_BV(4)|_BV(3))) {
            _position.x = 8;
        }
        else if (touched & _BV(4)) {
            _position.x = 7;
        }
        else if ((touched & (_BV(3)|_BV(2))) == (_BV(3)|_BV(2))) {
            _position.x = 10;
        }
        else if (touched & _BV(3)) {
            _position.x = 9;
        }
        else if ((touched & (_BV(2)|_BV(1))) == (_BV(2)|_BV(1))) {
            _position.x = 12;
        }
        else if (touched & _BV(2)) {
            _position.x = 11;
        }
        else if (touched & _BV(1)) {
            _position.x = 13;
        }
        else {
            _position.x = 0;
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

    _irqPin = irqPin;
    mpr121_irq_callback_flag = false;
    _event._type = EventType::NONE;
    pinMode(_irqPin, INPUT);
    attachInterrupt(digitalPinToInterrupt(_irqPin), mpr121_irq_callback, CHANGE);
    _mpr121.setThresholds(4, 4);

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
}

bool Mpr121Touchpad::isEventPending() const
{
    return mpr121_irq_callback_flag;
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
    _callbacks.erase(std::find_if(_callbacks.begin(), _callbacks.end(), [id, events](CallbackEvent &callback) {
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
    if (isEventPending()) {
        auto previous = _event._position;
        _event.read();
        if (previous == false && _event._position == true) {
            _event.touched();
        }
        else if (_event._position == false && previous == true) {
            _event.released();
        }
        else if (!_event._releasedTime) {
            _event.move(previous);
        }
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
            if (callback.events & _event.getType()) {
                debug_printf_P(PSTR("callback=%p\n"), &callback.callback);
                callback.callback(_event);
            }
        }
        _event._type = EventType::NONE;
    }
}


#endif

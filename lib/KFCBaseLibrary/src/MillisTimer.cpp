/**
  Author: sascha_lammers@gmx.de
*/

#include "MillisTimer.h"

void MillisTimer::set(long delay) {
    ulong _millis = millis();
    _time = _millis + delay;
    _overflow = _millis > _time;
    if (_overflow) {
        _time += 0x7fffffff;
    }
    _active = true;
    _delay = delay;
}

// get and active do not change the state, reached() or disable() must be called
long MillisTimer::get() const {
    if (!_active) {
        return -1;
    }
    if (_overflow) {
        ulong _millis = millis() + 0x7fffffff;
        return _time - _millis;
    } else {
        if (millis() > _time) {
            return 0;
        } else {
            return _time - millis();
        }
    }
}

// get and active do not change the state, reached() or disable() must be called
bool MillisTimer::isActive() const {
    return _active;
}

void MillisTimer::disable() {
    _active = false;
}

bool MillisTimer::reached(bool reset) {
    bool result;
    if (!_active)  {
        return false;
    }
    if (_overflow) {
        ulong _millis = millis();
        _millis += 0x7fffffff;
        result = _millis > _time;
    } else {
        result = millis() > _time;
    }
    if (result) {
        if (reset) {
            set(_delay);
        } else {
            _active = false;
        }
    }
    return result;
}

void MillisTimer::restart() {
    set(_delay);
}

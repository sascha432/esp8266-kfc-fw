/**
  Author: sascha_lammers@gmx.de
*/

#include "MillisTimer.h"

MillisTimer::MillisTimer(long delay) {
    set(delay);
}

MillisTimer::MillisTimer() {
      _data.active = false;
  }

void MillisTimer::set(long delay) {
    unsigned long _millis = millis();
    _data.time = _millis + delay;
    _data.overflow = _millis > _data.time;
    if (_data.overflow) {
        _data.time += 0x7fffffff;
    }
    _data.active = true;
    _data.delay = delay;
}

long MillisTimer::get() const {
    if (!_data.active) {
        return -1;
    }
    if (_data.overflow) {
        unsigned long _millis = millis() + 0x7fffffff;
        return _data.time - _millis;
    } else {
        if (millis() > _data.time) {
            return 0;
        } else {
            return _data.time - millis();
        }
    }
}

bool MillisTimer::isActive() const {
    return _data.active;
}

void MillisTimer::disable() {
    _data.active = false;
}

bool MillisTimer::reached(bool reset) {
    bool result;
    if (!_data.active)  {
        return false;
    }
    if (_data.overflow) {
        unsigned long _millis = millis();
        _millis += 0x7fffffff;
        result = _millis > _data.time;
    } else {
        result = millis() > _data.time;
    }
    if (result) {
        if (reset) {
            set(_data.delay);
        } else {
            _data.active = false;
        }
    }
    return result;
}

void MillisTimer::restart() {
    set(_data.delay);
}

/**
  Author: sascha_lammers@gmx.de
*/

#include "MillisTimer.h"

MillisTimer::MillisTimer(uint32_t delay)
{
    set(_delay);
}

MillisTimer::MillisTimer() : _active(false)
{
}

void MillisTimer::set(uint32_t delay)
{
    uint32_t ms = millis();
    _endTime = ms + delay;
    _overflow = ms > _endTime;
    if (_overflow) {
        _endTime += 0x7fffffffU;
    }
    _active = true;
    _delay = delay;
}

int32_t MillisTimer::get() const
{
    if (!_active) {
        return -1;
    }
    uint32_t ms = millis();
    if (_overflow) {
        ms + 0x7fffffffU;
    }
    if (ms < _endTime) {
        return _endTime - ms;
    }
    return 0;
}

bool MillisTimer::isActive() const
{
    return _active;
}

void MillisTimer::disable()
{
    _active = false;
}

bool MillisTimer::reached(bool reset)
{
    if (_active)  {
        uint32_t ms = millis();
        if (_overflow) {
            ms += 0x7fffffffU;
        }
        if (ms >= _endTime) {
            if (reset) {
                set(_delay);
            } else {
                _active = false;
            }
            return true;
        }
    }
    return false;
}

void MillisTimer::restart()
{
    set(_delay);
}

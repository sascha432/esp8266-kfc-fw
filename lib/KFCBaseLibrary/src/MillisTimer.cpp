/**
  Author: sascha_lammers@gmx.de
*/

#include "MillisTimer.h"

MillisTimer::MillisTimer(uint32_t delay)
{
    set(_delay);
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

uint32_t MillisTimer::getTimeLeft() const
{
    uint32_t ms = millis();
    if (_overflow) {
        ms += 0x7fffffffU;
    }
    if (ms < _endTime) {
        return _endTime - ms;
    }
    return 0;
}

int32_t MillisTimer::get() const
{
    if (!_active) {
        return -1;
    }
    return getTimeLeft();
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

/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#include <push_pack.h>

// provides a timer based on millis that doesn't suffer from unsigned long overflow and doesn't use 64bit counters

class MillisTimer {
public:
    MillisTimer();
    // max. 1 billion milliseconds
    MillisTimer(uint32_t delay);

    void set(uint32_t delay);
    int32_t get() const; // does not change the state, reached() or disable() must be called
    void disable();
    bool isActive() const; // does not change the state, reached() or disable() must be called
    bool reached(bool reset = false);
    void restart();

    uint32_t getTimeLeft() const;

    inline uint32_t getDelay() const {
        return _delay;
    }

private:
    union __attribute__packed__ {
        struct __attribute__packed__ {
            uint32_t _endTime;
            uint32_t _delay : 30;
            uint32_t _active : 1;
            uint32_t _overflow : 1;
        };
        uint32_t _raw[2];
    };
};

static_assert(sizeof(MillisTimer) == sizeof(uint32_t) * 2, "something went wrong");

inline MillisTimer::MillisTimer() : _raw{}
{
}

inline MillisTimer::MillisTimer(uint32_t delay)
{
    set(delay);
}

inline bool MillisTimer::isActive() const
{
    return _active;
}

inline void MillisTimer::disable()
{
    _active = false;
}

inline void MillisTimer::set(uint32_t delay)
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

inline uint32_t MillisTimer::getTimeLeft() const
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

inline int32_t MillisTimer::get() const
{
    if (!_active) {
        return -1;
    }
    return getTimeLeft();
}

inline bool MillisTimer::reached(bool reset)
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

inline void MillisTimer::restart()
{
    set(_delay);
}


#include <pop_pack.h>

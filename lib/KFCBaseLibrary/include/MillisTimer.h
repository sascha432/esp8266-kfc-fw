/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#include <push_pack.h>

// provides a timer based on millis that doesn't suffer from unsigned long overflow and doesn't use 64bit counters

class MillisTimer {
public:
    // max. 1 billion milliseconds
    MillisTimer(uint32_t delay);
    MillisTimer();

    void set(uint32_t delay);
    int32_t get() const; // does not change the state, reached() or disable() must be called
    void disable();
    bool isActive() const; // does not change the state, reached() or disable() must be called
    bool reached(bool reset = false);
    void restart();
    uint32_t getDelay() const {
        return _delay;
    }

private:
    struct __attribute__packed__ {
        uint32_t _endTime;
        uint32_t _delay: 30;
        uint32_t _active: 1;
        uint32_t _overflow: 1;
    };
};

static_assert(sizeof(MillisTimer) == sizeof(uint64_t), "something went wrong");

#include <pop_pack.h>

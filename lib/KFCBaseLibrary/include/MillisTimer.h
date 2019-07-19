/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#include <push_pack.h>

// provides a timer based on millis that doesn't suffer from unsigned long overflow and doesn't use 64bit counters
class MillisTimer {
public:
    typedef struct __attribute__packed__ {
        unsigned long time;
        unsigned long delay;
        uint8_t active: 1;
        uint8_t overflow: 1;
    } timer_t;

    MillisTimer(long delay);
    MillisTimer();

    void set(long delay);
    long get() const; // does not change the state, reached() or disable() must be called
    void disable();
    bool isActive() const; // does not change the state, reached() or disable() must be called
    bool reached(bool reset = false);
    void restart();

private:
    timer_t _data;
};

#include <pop_pack.h>
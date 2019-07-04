/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

// provides a timer based on millis that doesnt suffer from unsigned long overflow and doesnt use 64bit counters
class MillisTimer {
public:
    MillisTimer(long delay) {
        set(delay);
    }
    MillisTimer() {
        _active = false;
    }

    void set(long delay);
    long get() const;
    void disable();
    bool isActive() const;
    bool reached(bool reset = false);
    void restart();

private:
    bool _active;
    bool _overflow;
    ulong _time;
    long _delay;
};

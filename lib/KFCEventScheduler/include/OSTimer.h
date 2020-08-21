/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_OSTIMER
#define DEBUG_OSTIMER             0
#endif

#include <Arduino_compat.h>
#if defined(ESP8266)
#include <osapi.h>
#endif

extern "C" void ICACHE_RAM_ATTR _ostimer_callback(void *arg);
extern "C" void _ostimer_detach(ETSTimer *etsTimer, void *timerArg);

class OSTimer {
public:
    OSTimer() : _etsTimer({}) {}

    virtual ~OSTimer() {
        detach();
    }

    virtual void run() = 0;

    void startTimer(int32_t delay, bool repeat);
    virtual void detach();

    bool isRunning() const {
      return (_etsTimer.timer_arg == this);
    }

protected:
    ETSTimer _etsTimer;
};

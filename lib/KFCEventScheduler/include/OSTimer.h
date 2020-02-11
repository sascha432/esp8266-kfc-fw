/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_OSTIMER
#define DEBUG_OSTIMER 0
#endif

#include <Arduino_compat.h>
#if defined(ESP8266)
#include <osapi.h>
#endif

class OSTimer {
public:
    OSTimer();
    virtual ~OSTimer();

    virtual ICACHE_RAM_ATTR void run() = 0;

    void startTimer(uint32_t delay, bool repeat);
    virtual void detach();

private:
    ETSTimer _etsTimer;
};

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
#if defined(ESP32)
// os_timer = ets_timer wrapper
#endif

class OSTimer {
public:
    OSTimer();
    virtual ~OSTimer();

    virtual void run() {
        // if the object is dynamically allocated and not needed anymore, "delete this" can be called before returning
        // a repeated timer automatically stops after that
    }

    void startTimer(uint32_t delay, bool repeat);
    virtual void detach();

    static void _callback(void *arg);
    static void _callbackOnce(void *arg);

private:
    os_timer_t *_timer;
};

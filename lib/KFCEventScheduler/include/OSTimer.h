/**
  Author: sascha_lammers@gmx.de
*/


#pragma once

#include <Arduino_compat.h>
#if defined(ESP8266)
#include <osapi.h>
#endif
#if defined(ESP32)
#error TODO implement ets_timer
#endif

class OSTimer {
public:
    OSTimer() {
        _timer = nullptr;
    }
    virtual ~OSTimer() {
        detach();
    }

    virtual void run() {
        // if the object is dynamically allocated and not needed anymore, delete this can be called before returning
        // a repeated timer automatically stops after that
    }

    void startTimer(uint32_t delay, bool repeat);

    void detach() {
        if (_timer) {
            os_timer_disarm(_timer);
            delete _timer;
            _timer = nullptr;
        }
    }

    static void _callback(void *arg) {
        OSTimer *timer = reinterpret_cast<OSTimer *>(arg);
        timer->run();
    }

    static void _callbackOnce(void *arg) {
        OSTimer *timer = reinterpret_cast<OSTimer *>(arg);
        timer->run();
        timer->detach();
    }


private:
    os_timer_t *_timer;
};


/**
  Author: sascha_lammers@gmx.de
*/

#include "EventTimer.h"
#include "OSTimer.h"

#ifndef DEBUG_OSTIMER
#define DEBUG_OSTIMER 0
#endif
#if DEBUG_OSTIMER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

OSTimer::OSTimer() {
    _timer = nullptr;
}

OSTimer::~OSTimer() {
    detach();
}

void OSTimer::startTimer(uint32_t delay, bool repeat) {
    if (_timer) {
        os_timer_disarm(_timer);
    } else {
        os_timer_create(&_timer, reinterpret_cast<os_timer_func_t_ptr>(repeat ? _callback : _callbackOnce), reinterpret_cast<void *>(this));
        os_timer_arm(_timer, delay, repeat);
        if (delay < MIN_DELAY) {
            _debug_printf_P(PSTR("ERROR! delay %.3f < %d is not supported\n"), delay / 1000.0, MIN_DELAY);
        }
        if (delay > MAX_DELAY) {
            _debug_printf_P(PSTR("ERROR! delay %.3f > %d is not supported\n"), delay / 1000.0, MAX_DELAY);
        }
    }
}

void OSTimer::detach() {
    if (_timer) {
        os_timer_disarm(_timer);
        os_timer_delete(_timer);
        _timer = nullptr;
    }
}

void OSTimer::_callback(void *arg) {
    OSTimer *timer = reinterpret_cast<OSTimer *>(arg);
    timer->run();
}

void OSTimer::_callbackOnce(void *arg) {
    OSTimer *timer = reinterpret_cast<OSTimer *>(arg);
    timer->run();
    timer->detach();
}

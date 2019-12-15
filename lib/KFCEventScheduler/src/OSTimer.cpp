
/**
  Author: sascha_lammers@gmx.de
*/

#include "EventTimer.h"
#include "OSTimer.h"

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
    if (delay < EventTimer::minDelay) {
        _debug_printf_P(PSTR("ERROR: delay %u < %u is not supported\n"), delay, EventTimer::minDelay);
        delay = EventTimer::minDelay;
    }
    else if (delay > EventTimer::maxDelay) {
        _debug_printf_P(PSTR("ERROR: delay %u > %u is not supported\n"), delay, EventTimer::maxDelay);
        delay = EventTimer::maxDelay;
    }
    if (_timer) {
        os_timer_disarm(_timer);
    }
    else {
        os_timer_create(_timer, reinterpret_cast<os_timer_func_t_ptr>(repeat ? _callback : _callbackOnce), reinterpret_cast<void *>(this));
    }
    os_timer_arm(_timer, delay, repeat);
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

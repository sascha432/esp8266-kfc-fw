/**
  Author: sascha_lammers@gmx.de
*/

#include "EventTimer.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

EventTimer::EventTimer(EventScheduler::Callback loopCallback, EventScheduler::Callback removeCallback, int64_t delay, int repeat, EventScheduler::Priority_t priority) {

    if (delay < 0) {
        debug_printf_P(PSTR("EventTimer::EventTimer(): delay < 0\n"));
        panic();
    }
#if DEBUG_INCLUDE_SOURCE_INFO
    _start = 0;
#endif

    _loopCallback = loopCallback;
    _removeCallback = removeCallback;
    _callbackScheduled = false;
    _delay = delay;
    _repeat = repeat;
    _callCounter = 0;
    _priority = priority;

    _timer = _debug_new os_timer_t();
    _installTimer();
}

EventTimer::~EventTimer() {
    _debug_printf_P(PSTR("EventTimer::~EventTimer() os_timer %p\n"), _timer);
    detach();
    if (_removeCallback) {
        _debug_printf_P(PSTR("EventTimer::~EventTimer(): removeCallback(%p)\n"), this);
        _removeCallback(this);
    }
    if (Scheduler.hasTimer(this))  {
        _debug_printf_P(PSTR("EventTimer::~EventTimer() Calling ::removeTimer(%p) to remove destroyed object from scheduler\n"), this);
        Scheduler.removeTimer(this);
    }
}

void EventTimer::_installTimer() {
    _debug_printf_P(PSTR("EventTimer::_installTimer(): Installing os_timer %p, delay %.3f, repeat %d\n"), _timer, _delay / 1000.0, _repeat);

    if (_delay > MAX_DELAY) {
        _remainingDelay = _delay - MAX_DELAY;
    } else {
        _remainingDelay = 0;
    }
    os_timer_setfn(_timer, reinterpret_cast<os_timer_func_t *>(EventScheduler::_timerCallback), reinterpret_cast<void *>(this));
    os_timer_arm(_timer, max(MIN_DELAY, (int32_t)(_remainingDelay ? MAX_DELAY : _delay)), (_remainingDelay || _repeat) ? 1 : 0);
#if DEBUG_EVENT_SCHEDULER
    if (_start) {
        _debug_printf_P(PSTR("EventTimer::_installTimer(): %p, real delay %lu / %lu\n"), this, millis() - _start, (unsigned long)_delay);
    }
    _start = millis();
#endif
}

bool EventTimer::active() const {
    return _timer;
}

int EventTimer::getRepeat() const {
    return _repeat;
}

int64_t EventTimer::getDelay() const {
    return _delay;
}

int EventTimer::getCallCounter() const {
    return _callCounter;
}

void EventTimer::setPriority(EventScheduler::Priority_t priority) {
    _priority = priority;
}

void EventTimer::changeOptions(int delay, int repeat, EventScheduler::Priority_t priority) {
    int diff = 0;
    if (repeat != -2) {
        if (_repeat != repeat) {
            _repeat = repeat;
            diff++;
        }
    }
    if (priority != -1) {
        if (_priority != priority) {
            _priority = priority;
            diff++;
        }
    }
    if (delay != -1 && _delay != delay) {
        _delay = delay;
        diff++;
    }
    if (_timer) {
        if (diff == 0) { // options didn't change, use attached timer
            return;
        }
        os_timer_disarm(_timer);
    } else {
        _timer = _debug_new os_timer_t();
    }
    _installTimer();
}

void EventTimer::_updateInterval(int delay, bool repeat) {
    os_timer_disarm(_timer);
    os_timer_arm(_timer, max(MIN_DELAY, delay), repeat ? 1 : 0);
}

bool EventTimer::_maxRepeat() const {
    if (_repeat == 0) {
        return true;
    }
    if (_repeat == EventScheduler::UNLIMTIED) {
        return false;
    }
    return _callCounter >= _repeat;
}

String EventTimer::_getTimerSource() {
#if DEBUG_INCLUDE_SOURCE_INFO
    String out = __source;
    out +=  ':';
    out += String(__line);
    out += '@';
    out += __function;
    if (_timer) {
        char buf[16];
        snprintf_P(buf, sizeof(buf), PSTR("%p"), _timer);
        out += F(" os_timer ");
        out += buf;
    }
    return out;
#else
    return F("");
#endif
}


void EventTimer::detach() {
    if (_timer) {
        _debug_printf_P(PSTR("EventTimer::detach(): %p\n"), _timer);
        os_timer_disarm(_timer);
        delete _timer;
        _timer = nullptr;
    }
}

void EventTimer::invokeCallback() {
    if (Scheduler.hasTimer(this)) { // make sure the timer is still running
        _loopCallback(this);
    }
}

void EventTimer::_setScheduleCallback(bool enable) {
    #if 0
    if (enabled) {
        Scheduler._callbacks.push_back(ActiveCallbackTimer({timer, false}));
    } else {
        //remove from scheduler if exists?
    }
    #endif
    _callbackScheduled = enable;
}

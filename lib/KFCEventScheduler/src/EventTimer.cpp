/**
  Author: sascha_lammers@gmx.de
*/

#include "EventTimer.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

EventTimer::EventTimer(EventScheduler::TimerPtr *timerPtr, EventScheduler::Callback loopCallback, int64_t delay, int repeat, EventScheduler::Priority_t priority) {

    if (delay < 0) {
        __debugbreak_and_panic_printf_P(PSTR("delay < 0\n"));
    }

    _loopCallback = loopCallback;
    _callbackScheduled = false;
    _delay = delay;
    _repeat = repeat;
    _callCounter = 0;
    _priority = priority;
    _timerPtr = timerPtr;
    if (_timerPtr) {
        *_timerPtr = this;
    }

    os_timer_create(_timer, reinterpret_cast<os_timer_func_t_ptr>(EventScheduler::_timerCallback), reinterpret_cast<void *>(this));
    _installTimer();
}

EventTimer::~EventTimer() {
    _debug_printf_P(PSTR("os_timer=%p\n"), _timer);
    detach();
    if (Scheduler.hasTimer(this))  {
        _debug_printf_P(PSTR("calling ::removeTimer(%p) to remove destroyed object from scheduler\n"), this);
        Scheduler.removeTimer(this);
    }
}

void EventTimer::_installTimer() {
    _debug_printf_P(PSTR("installing os_timer %p, delay %.3f, repeat %d\n"), _timer, _delay / 1000.0, _repeat);

    if (_delay > maxDelay) {
        _remainingDelay = _delay - maxDelay;
    } else {
        _remainingDelay = 0;
    }

    os_timer_arm(_timer, std::max(minDelay, (int32_t)(_remainingDelay ? maxDelay : _delay)), (_remainingDelay || _repeat) ? 1 : 0);
}

void EventTimer::setPriority(EventScheduler::Priority_t priority) {
    _priority = priority;
}

void EventTimer::changeOptions(int delay, int repeat, EventScheduler::Priority_t priority) {
    uint8_t diff = 0;
    if (repeat != EventScheduler::NO_CHANGE) {
        if (_repeat != repeat) {
            _repeat = repeat;
            diff++;
        }
    }
    if (priority != EventScheduler::PRIO_NONE) {
        if (_priority != priority) {
            _priority = priority;
            diff++;
        }
    }
    if (delay != ChangeOptionsDelayEnum_t::DELAY_NO_CHANGE && _delay != delay) {
        _delay = delay;
        diff++;
    }
    if (_timer) {
        if (diff == 0) { // options didn't change, use attached timer
            return;
        }
        os_timer_disarm(_timer);
    } else {
        os_timer_disarm(_timer);
        os_timer_delete(_timer);
        os_timer_create(_timer, reinterpret_cast<os_timer_func_t_ptr>(EventScheduler::_timerCallback), reinterpret_cast<void *>(this));
    }
    _installTimer();
}

void ICACHE_RAM_ATTR EventTimer::_updateInterval(int delay, bool repeat) {
    os_timer_disarm(_timer);
    os_timer_arm(_timer, std::max(minDelay, delay), repeat ? 1 : 0);
}

bool ICACHE_RAM_ATTR EventTimer::_maxRepeat() const {
    if (_repeat == 0) {
        return true;
    }
    if (_repeat == EventScheduler::UNLIMTIED) {
        return false;
    }
    return _callCounter >= _repeat;
}

void ICACHE_RAM_ATTR EventTimer::detach() {
    if (_timer) {
        _debug_printf_P(PSTR("os_timer=%p\n"), _timer);
        os_timer_disarm(_timer);
        os_timer_delete(_timer);
        _timer = nullptr;
    }
}

void EventTimer::invokeCallback() {
    if (Scheduler.hasTimer(this)) { // make sure the timer is still running
        _loopCallback(this);
    }
}

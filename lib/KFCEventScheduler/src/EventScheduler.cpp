/**
  Author: sascha_lammers@gmx.de
*/

#include "EventScheduler.h"
#include "EventTimer.h"
#include "LoopFunctions.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if !defined(DISABLE_EVENT_SCHEDULER)
EventScheduler Scheduler;
#endif

void EventScheduler::begin() {
    LoopFunctions::add(loop);
}

void EventScheduler::end() {
    LoopFunctions::remove(loop);
    noInterrupts();
    TimerVector list;
    list.swap(_timers);
    for(auto timer: list) {
        delete timer;
    }
    interrupts();
}

/**
 *
 * class EventScheduler
 *
 *  - Wake up device
 *  - Support for more than 2 hour interval
 *  - Priorities to run inside the timer interrupt or in the main loop function
 *
 */

// EventScheduler::TimerPtr EventScheduler::_addTimer(int64_t delay, int repeat, Callback callback, Priority_t priority) {
//     TimerPtr timer;
//     _timers.push_back(timer = _debug_new EventTimer(callback, delay, repeat, priority));
// #if DEBUG
//     timer->__source = _debug_filename;
//     timer->__line = _debug_lineno;
//     timer->__function = _debug_function;
// #endif
//     _debug_printf_P(PSTR("add timer %p (%s), delay %.3f, repeat %d, priority %d\n"), timer, timer->_getTimerSource().c_str(), delay / 1000.0, repeat, priority);
//     installLoopFunc();
//     Scheduler._list();
//     return _timers.back();
// }

void EventScheduler::addTimer(TimerPtr *timerPtr, int64_t delay, int repeat, Callback callback, Priority_t priority) {
#if DEBUG_EVENT_SCHEDULER
    _timers.push_back(_debug_new EventTimer(timerPtr, callback, delay, repeat, priority));
    _debug_printf_P(PSTR("EventScheduler::addTimer(%p): delay %.3f, repeat %d, priority %d\n"), _timers.back(), delay / 1000.0, repeat, priority);
    Scheduler._list();
#else
    _timers.push_back(_debug_new EventTimer(timerPtr, callback, delay, repeat, priority));
#endif
}

bool ICACHE_RAM_ATTR EventScheduler::hasTimer(TimerPtr timer) {
    if (timer) {
        for(auto _timer: _timers) {
            if (_timer == timer) {
                return true;
            }
        }
    }
    return false;
    // auto it = std::find_if(_timers.begin(), _timers.end(), [timer](const TimerPtr _timer) {
    //     return _timer == timer;
    // });
    // return it != _timers.end();
}

bool EventScheduler::removeTimer(TimerPtr *timer)
{
    _debug_printf_P(PSTR("EventScheduler::removeTimer(); timer pointer=%p, timer=%p\n"), timer, timer ? *timer : nullptr);
    auto result = false;
    if (*timer) {
        result = removeTimer(*timer);
        *timer = nullptr;
    }
    return result;
}

bool EventScheduler::removeTimer(TimerPtr timer)
{
    if (!timer) {
        _debug_printf_P(PSTR("EventScheduler::_removeTimer(): timer=null\n"));
        return false;
    }
    auto result = hasTimer(timer);
    _debug_printf_P(PSTR("EventScheduler::_removeTimer(): timer=%p, linked timer ptr=%p, result=%d\n"), timer, (timer ? timer->_timerPtr : nullptr), result);
    if (!result) {
#if DEBUG_EVENT_SCHEDULER
        __debugbreak_and_panic_printf_P(PSTR("EventScheduler::removeTimer(): timer %p is not active or has been deleted already\n"), timer);
#endif
        return false;
    }

    _removeTimer(timer);
    return true;
}

void ICACHE_RAM_ATTR EventScheduler::_removeTimer(TimerPtr timer)
{
    _debug_printf_P(PSTR("EventScheduler::_removeTimer(): timer=%p, linked timer ptr=%p\n"), timer, (timer ? timer->_timerPtr : nullptr));

    timer->detach();

    if (timer->_timerPtr) {
        if (*timer->_timerPtr != timer) {
            _debug_printf_P(PSTR("EventScheduler::_removeTimer(): timer pointer mismatch %p<>%p\n"), timer, *timer->_timerPtr);
        }
        *timer->_timerPtr = nullptr;
    }
    else {
        _debug_printf_P(PSTR("EventScheduler::_removeTimer(): timer pointer for %p=null\n"), timer);
    }

    // remove from list of timers
    _timers.erase(std::remove_if(_timers.begin(), _timers.end(), [timer](const TimerPtr _timer) {
        return _timer == timer;
    }), _timers.end());

    _debug_printf_P(PSTR("EventScheduler::removeTimer(): Deleting timer %p\n"), timer);
    delete timer;

#if DEBUG_EVENT_SCHEDULER
    Scheduler._list();
#endif
}

void ICACHE_RAM_ATTR EventScheduler::_timerCallback(void *arg) {
    TimerPtr timer = reinterpret_cast<TimerPtr>(arg);
    if (timer->_remainingDelay > EventTimer::maxDelay) {
        timer->_remainingDelay -= EventTimer::maxDelay;
        _debug_printf_P(PSTR("EventScheduler::_timerCallback(%p): timer %p is greater max delay %d, remaining delay %.3f\n"), arg, timer, EventTimer::maxDelay, timer->_remainingDelay / 1000.0);
    }
    else if (timer->_remainingDelay) {
        _debug_printf_P(PSTR("EventScheduler::_timerCallback(%p): timer %p is below max delay %d, ajusting next callback to %.3f ms\n"), arg, timer, EventTimer::maxDelay, timer->_remainingDelay / 1000.0);
        timer->_updateInterval(timer->_remainingDelay, false);
        timer->_remainingDelay = 0;
    }
    else {
         // immediately reschedule if the delay exceeds the max. interval
        if (timer->_delay > EventTimer::maxDelay && !timer->_maxRepeat()) {
            _debug_printf_P(PSTR("EventScheduler::_timerCallback(%p): timer %p is in 'repeat mode'. Rescheduling, next remaining time update in %d ms\n"), arg, timer, EventTimer::maxDelay)
            timer->_remainingDelay = timer->_delay - EventTimer::maxDelay;
            timer->_updateInterval(EventTimer::maxDelay, true);
        }
        if (timer->_priority == EventScheduler::PRIO_HIGH) {
            // call high priortiy timer directly
            Scheduler.callTimer(timer);
        }
        else {
            timer->_callbackScheduled = true;
        }
    }
}

void ICACHE_RAM_ATTR EventScheduler::callTimer(TimerPtr timer) {
    _debug_printf_P(PSTR("EventScheduler::callTimer(%p): priority %d\n"), timer, timer->_priority);
    timer->_loopCallback(timer);
    if (hasTimer(timer)) {
        timer->_callCounter++;
        if (timer->_maxRepeat()) {
            _debug_printf_P(PSTR("EventScheduler::callTimer(%p): removing EventTimer, max repeat reached\n"), timer);
            _removeTimer(timer);
        }
        else if (!timer->active()) {
            _debug_printf_P(PSTR("EventScheduler::callTimer(%p): removing EventTimer, not active\n"), timer);
            _removeTimer(timer);
        }
    }
}


void EventScheduler::loop() {
    Scheduler._loop();
}

void EventScheduler::_loop() {
    if (!_timers.empty()) {
        int runtime = 0;
        auto start = millis();

        // do not use iterators here, the vector might be modified in the timer callback
        for(size_t i = 0; i < _timers.size(); i++) {
            auto timer = _timers[i];
            if (timer->_callbackScheduled) {
                timer->_callbackScheduled = false;
                callTimer(timer);
                if ((runtime = millis() - start) > _runtimeLimit) {
                    _debug_printf_P(PSTR("void EventScheduler::loopFunc(): timer %p, runtime limit %d/%d reached, exiting loop\n"), timer, runtime, _runtimeLimit);
                    break;
                }
            }
        }

#if DEBUG_EVENT_SCHEDULER
        int left = 0;
        for(auto timer: _timers) {
            if (timer->_callbackScheduled) {
                left++;
            }
        }

        if (left) {
            _debug_printf_P(PSTR("void EventScheduler::loopFunc(): %d callbacks still need to be scheduled after this loop\n"), left);
        }
#endif
    }
}

#if DEBUG_EVENT_SCHEDULER

void EventScheduler::_list() {
    if (_timers.empty()) {
        _debug_printf_P(PSTR("No timers left\n"));
    } else {
        int scheduled = 0;
        for(auto timer: _timers) {
            _debug_printf_P(PSTR("%p: delay %.3f repeat %d/%d, scheduled %d\n"), timer, timer->_delay / 1000.0, timer->_callCounter, timer->_repeat, timer->_callbackScheduled);
            if (timer->_callbackScheduled) {
                scheduled++;
            }
        }
        _debug_printf_P(PSTR("Timers %d, scheduled %d\n"), _timers.size(), scheduled);
    }
}

#endif


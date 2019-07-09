/**
  Author: sascha_lammers@gmx.de
*/

#include "EventScheduler.h"
#include "EventTimer.h"
#include "LoopFunctions.h"

EventScheduler Scheduler;

/**
 *
 * class EventScheduler
 *
 *  - Wake up device
 *  - Support for more than 2 hour interval
 *  - Priorities to run inside the timer interrupt or in the main loop function
 *
 */

// EventScheduler::EventTimerPtr EventScheduler::_addTimer(int64_t delay, int repeat, Callback callback, Priority_t priority) {
//     EventTimerPtr timer;
//     _timers.push_back(timer = new EventTimer(callback, delay, repeat, priority));
// #if DEBUG
//     timer->__source = _debug_filename;
//     timer->__line = _debug_lineno;
//     timer->__function = _debug_function;
// #endif
//     if_debug_printf_P(PSTR("add timer %p (%s), delay %.3f, repeat %d, priority %d\n"), timer, timer->_getTimerSource().c_str(), delay / 1000.0, repeat, priority);
//     installLoopFunc();
//     Scheduler._list();
//     return _timers.back();
// }

EventScheduler::EventTimerPtr EventScheduler::addTimer(int64_t delay, int repeat, Callback callback, Callback removeCallback, Priority_t priority) {
#if DEBUG_EVENT_SCHEDULER
    EventTimerPtr timer;
    _timers.push_back(timer = new EventTimer(callback, removeCallback, delay, repeat, priority));
    if_debug_printf_P(PSTR("add timer %p (%s), delay %.3f, repeat %d, priority %d\n"), timer, timer->_getTimerSource().c_str(), delay / 1000.0, repeat, priority);
#else
    _timers.push_back(new EventTimer(callback, removeCallback, delay, repeat, priority));
#endif
    installLoopFunc();
    Scheduler._list();
    return _timers.back();
}

bool EventScheduler::hasTimer(EventTimerPtr timer) {
    if (!timer) {
        return false;
    }
    const auto &it = std::find_if(_timers.begin(), _timers.end(), [&timer](const EventTimerPtr _timer) {
        return _timer == timer;
    });
    return it != _timers.end();
}

void EventScheduler::removeTimer(EventTimerPtr timer, bool deleteObject) {
  debug_printf_P(PSTR("EventScheduler::removeTimer(%p, %d)\n"), timer, deleteObject);
    if (!timer) {
        return;
    }
#if DEBUG_EVENT_SCHEDULER
    if (!hasTimer(timer)) {
        debug_printf_P(PSTR("EventScheduler::removeTimer(): timer %p is not active or has been deleted already\n"), timer);
        panic();
    }
#endif

    timer->detach();

    // remove from list of timers
    _timers.erase(std::remove_if(_timers.begin(), _timers.end(), [&timer](const EventTimerPtr _timer) {
        return _timer == timer;
    }), _timers.end());

    #if 0
    // mark as removed (active schedule)
    for(auto &it: _callbacks) {
        if (it.timer == timer) {
            it.removed = true;
        }
    }
    // _callbacks.erase(std::remove_if(_callbacks.begin(), _callbacks.end(), [&timer](const EventTimerPtr _timer) {
    //     return _timer == timer;
    // }), _callbacks.end());

    // remove main() loop function if there isn't any timers to process
    if (_timers.empty() && _callbacks.empty()) {
        removeLoopFunc();
    }
    #else
    if (_timers.empty()) {
        removeLoopFunc();
    }
    #endif

    if (deleteObject) {
        debug_printf_P(PSTR("EventScheduler::removeTimer(): Deleting timer %p\n"), timer);
        delete timer;
    }

    Scheduler._list();
}

void EventScheduler::_timerCallback(void *arg) {
    EventTimerPtr timer = reinterpret_cast<EventTimerPtr>(arg);
    if (timer->_remainingDelay > MAX_DELAY) {
        timer->_remainingDelay -= MAX_DELAY;
        debug_printf_P(PSTR("EventScheduler::_timerCallback(%p): timer %p is greater max delay %d, remaining delay %.3f\n"), arg, timer, MAX_DELAY, timer->_remainingDelay / 1000.0);
    } else if (timer->_remainingDelay) {
        debug_printf_P(PSTR("EventScheduler::_timerCallback(%p): timer %p is below max delay %d, ajusting next callback to %.3f ms\n"), arg, timer, MAX_DELAY, timer->_remainingDelay / 1000.0);
        timer->_updateInterval(timer->_remainingDelay, false);
        timer->_remainingDelay = 0;
    } else {
         // immediately reschedule if the delay exceeds the max. interval
        if (timer->_delay > MAX_DELAY && !timer->_maxRepeat()) {
            debug_printf_P(PSTR("EventScheduler::_timerCallback(%p): timer %p is in 'repeat mode'. Rescheduling, next remaining time update in %d ms\n"), arg, timer, timer->_getTimerSource().c_str(), MAX_DELAY)
            timer->_remainingDelay = timer->_delay - MAX_DELAY;
            timer->_updateInterval(MAX_DELAY, true);
        }
        if (timer->_priority == EventScheduler::PRIO_HIGH) {
            Scheduler.callTimer(timer);
        } else {
            #if 0
            Scheduler._callbacks.push_back(ActiveCallbackTimer({timer, false}));
            #endif
            timer->_setScheduleCallback(true);
        }
        Scheduler.installLoopFunc();
    }
}

void EventScheduler::callTimer(EventTimerPtr timer) {
    debug_printf_P(PSTR("EventScheduler::callTimer(%p): priority %d\n"), timer, timer->_priority);
#if DEBUG_EVENT_SCHEDULER
    // debug_printf_P(PSTR("real delay %lu / %lu\n"), millis() - timer->_start, (unsigned long)timer->getDelay());
    timer->_start = millis();
#endif
    timer->_loopCallback(timer);
    if (hasTimer(timer)) {
        timer->_callCounter++;
        if (timer->_maxRepeat()) {
            debug_printf_P(PSTR("EventScheduler::callTimer(%p): detaching os_timer %p max repeat reached\n"), timer, timer->_timer);
            timer->detach();
        }
        if (!timer->active()) {
            debug_printf_P(PSTR("EventScheduler::callTimer(%p): remove EventTimer, os_timer is not active\n"));
            removeTimer(timer);
        }
    } else {
        if (timer) {
          debug_printf_P(PSTR("EventScheduler::callTimer(%p): WARNING! timer has been removed already\n"), timer);
        }
    }
}

void EventScheduler::loopFunc() {
    #if 1
    if (!_timers.empty()) {
    #else
    if (!_callbacks.empty()) {
    #endif
        int runtime = 0;
        ulong start = millis();

        #if 1

        // do not use iterators here, the vector might be modified in the timer callback
        for(size_t i = 0; i < _timers.size(); i++) {
            if (_timers[i]->_callbackScheduled) {
                _timers[i]->_callbackScheduled = false;
                callTimer(_timers[i]);
                if ((runtime = millis() - start) > _runtimeLimit) {
                    debug_printf_P(PSTR("void EventScheduler::loopFunc(): runtime limit %d/%d reached, exiting loop\n"), runtime, _runtimeLimit);
                    break;
                }
            }
        }

        int left = 0;
        for(auto &timer: _timers) {
            if (timer->_callbackScheduled) {
                left++;
            }
        }

        if (!left) {
            removeLoopFunc();
        } else {

            debug_printf_P(PSTR("void EventScheduler::loopFunc(): %d callbacks still need to be scheduled after this loop\n"), left);
        }

        #else
        auto it = _callbacks.begin();
        do {
            if (!it->removed) {
                callTimer(it->timer);
                it->removed = true;
            }
            if ((runtime = millis() - start) > _runtimeLimit) {
                if_debug_printf("runtime limit %d/%d reached, exiting loop\n", runtime, _runtimeLimit);
                break;
            }
        } while (++it != _callbacks.end());

        // remove marked entries from _callbacks since there might be some that haven't been executed yet or added while the loop was running
        _callbacks.erase(std::remove_if(_callbacks.begin(), _callbacks.end(), [](const ActiveCallbackTimer &cb) {
            return cb.removed;
        }), _callbacks.end());

        if (_callbacks.empty()) {
            removeLoopFunc();
        }
        #endif
    }
}

void EventScheduler::_list() {
#if DEBUG_EVENT_SCHEDULER
    if (_timers.empty()) {
        if_debug_printf_P(PSTR("No timers left\n"));
    } else {
        #if 0
        if_debug_printf_P(PSTR("Timers %d, active %d:\n"), _timers.size(), _callbacks.size());
        for(auto &timer: _timers) {
            //if_debug_printf_P(PSTR("%p (%s): delay %.3f repeat %d/%d\n"), timer, timer->_getTimerSource().c_str(), timer->_delay / 1000.0, timer->_callCounter, timer->_repeat);
            if_debug_printf_P(PSTR("%p: delay %.3f repeat %d/%d\n"), timer, timer->_delay / 1000.0, timer->_callCounter, timer->_repeat);
        }
        #else
        int scheduled = 0;
        for(auto &timer: _timers) {
            #if DEBUG_INCLUDE_SOURCE_INFO
                if_debug_printf_P(PSTR("%p(%s): delay %.3f repeat %d/%d, scheduled %d\n"),
                    timer, timer->_getTimerSource().c_str(), timer->_delay / 1000.0, timer->_callCounter, timer->_repeat, timer->_callbackScheduled);
            #else
                if_debug_printf_P(PSTR("%p: delay %.3f repeat %d/%d, scheduled %d\n"), timer, timer->_delay / 1000.0, timer->_callCounter, timer->_repeat, timer->_callbackScheduled);
            #endif
            if (timer->_callbackScheduled) {
                scheduled++;
            }
        }
        if_debug_printf_P(PSTR("Timers %d, scheduled %d\n"), _timers.size(), scheduled);
        #endif
    }
#endif
}

static void scheduler_loop_function() {
    Scheduler.loopFunc();
}

void EventScheduler::installLoopFunc() {
    if (!_loopFunctionInstalled) {
        // if_debug_printf_P(PSTR("Loop function installed\n"));
        LoopFunctions::add(scheduler_loop_function);
        _loopFunctionInstalled = true;
    }
}

void EventScheduler::removeLoopFunc() {
    if (_loopFunctionInstalled) {
        // if_debug_printf_P(PSTR("Removing Loop function\n"));
        // LoopFunctions::remove(scheduler_loop_function);
        // _loopFunctionInstalled = false;
    }
}

void EventScheduler::terminate() {
    LoopFunctions::remove(scheduler_loop_function);
    _loopFunctionInstalled = false;
    cli();
    for(auto timer: _timers) {
        timer->detach();
    }
    sei();
}

// void wifi_list_callbacks() {
// #if DEBUG_EVENT_SCHEDULER
//     if_debug_println("+++listing loop callbacks");
//     for(const auto &cb : loop_functions) {
//         #if DEBUG_INCLUDE_SOURCE_INFO
//             if_debug_printf_P(PSTR("callbacks %p events %d source " DEBUG_SOURCE_FORMAT "\n"), cb.callback, cb.__source.c_str(), cb.__line, cb.__function.c_str());
//         #else
//             if_debug_printf_P(PSTR("callbacks %p events %d\n"), cb.callback);
//         #endif
//     }
//     if_debug_println("+++listing fifi event callbacks");
//     for(const auto &cb : wifi_event_callbacks) {
//         #if DEBUG_INCLUDE_SOURCE_INFO
//             if_debug_printf_P(PSTR("callbacks %p events %d, source " DEBUG_SOURCE_FORMAT "\n"), cb.callback, cb.events, cb.__source.c_str(), cb.__line, cb.__function.c_str());
//         #else
//             if_debug_printf_P(PSTR("callbacks %p events %d\n"), cb.callback, cb.events);
//         #endif
//     }
// #endif
// }

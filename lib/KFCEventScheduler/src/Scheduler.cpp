/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "LoopFunctions.h"
#include "Event.h"
#include "Scheduler.h"
#include "CallbackTimer.h"
#include "ManagedTimer.h"
#include "Timer.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

#if !DISABLE_GLOBAL_EVENT_SCHEDULER
Event::Scheduler __Scheduler;
#endif

using namespace Event;

/**
 *
 * class EventScheduler
 *
 * - support for intervals of months in millisecond precision
 * - no loop() function that wastes CPU cycles except a single boolean check
 * - all code is executed in the main loop and no IRAM is used except 137 byte for the interrupt handler
 */
Scheduler::Scheduler() : _hasEvent(false), _runtimeLimit(250) {
}

void Scheduler::add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority)
{
    _add(intervalMillis, repeat, callback, priority);
}

void Scheduler::add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
{
    _add(interval.count(), repeat, callback, priority);
}

CallbackTimer *Scheduler::_add(int64_t delay, RepeatType repeat, Callback callback, PriorityType priority)
{
    // TimerPtr ptr(new CallbackTimer(callback, delay, repeat, priority));
    // auto iter = std::upper_bound(_timers.begin(), _timers.end(), ptr, [](const TimerPtr &a, const TimerPtr &b) {
    //     return a && b && (b->_priority < a->_priority);
    // });
    // auto iterator = _timers.insert(iter, std::move(ptr));
    // auto &timerPtr = *iterator;

    auto timerPtr = new CallbackTimer(callback, delay, repeat, priority);
    _timers.push_back(timerPtr);

    __LDBG_IF(
        timerPtr->_file = DebugContext::__pos._file;
        timerPtr->_line = DebugContext::__pos._line;
        __LDBG_printf("dly=%.0f maxrep=%d prio=%u callback=%p timer=%p %s:%u", delay / 1.0, repeat._repeat, priority, lambda_target(callback), timerPtr, __S(DebugContext::__pos._file), DebugContext::__pos._line);
        DebugContext::__pos = DebugContext();
    );

    // __list();
    timerPtr->_initTimer();
    return timerPtr;
}

void Scheduler::end()
{
    __LDBG_print("terminating scheduler");
    // disarm all timers to ensure __TimerCallback is not called while deleting them
    for(const auto &timer: _timers) {
        timer->_disarm();
    }
    // move vector before deleting all CallbackTimer objects
    auto tmp = std::move(_timers);
    std::swap(_timers, tmp);
    for(auto timer: tmp) {
        delete timer;
    }
    _hasEvent = (int8_t)PriorityType::NONE;
}

void Scheduler::remove(CallbackTimerPtr timer)
{
    if (timer) {
        _removeTimer(timer);
    }
}

TimerVectorIterator Scheduler::_getTimer(CallbackTimerPtr timer)
{
    return std::find(_timers.begin(), _timers.end(), timer);
}

bool Scheduler::_hasTimer(CallbackTimerPtr timer) const
{
    return (timer == nullptr) ? false : (std::find(_timers.begin(), _timers.end(), timer) != _timers.end());
}

bool Scheduler::_removeTimer(CallbackTimerPtr timer)
{
    if (timer) {
        auto iterator = _getTimer(timer);
        __LDBG_assert(iterator != _timers.end());
        if (iterator != _timers.end()) {

            __LDBG_printf("timer=%p iterator=%p managed=%p iterator_managed=%p %s:%u", timer, *iterator, timer->_timer, (*iterator)->_timer, __S(timer->_file), timer->_line);

            // if (timer->_timer) {
            //     timer->_timer->_managedCallbackTimer.clear();
            // }

            // mark as deleted in vector
            *iterator = nullptr;

            // disarm and delete
            timer->_disarm();
            delete timer;

            return true;
        }
    }
    return false;
}

void Scheduler::_cleanup()
{
    _timers.erase(std::remove(_timers.begin(), _timers.end(), nullptr));
    _timers.shrink_to_fit();
}

void Scheduler::run(PriorityType runAbovePriority)
{
    __Scheduler._run(runAbovePriority);
}

void Scheduler::_run(PriorityType runAbovePriority)
{
    if (_hasEvent > (int8_t)runAbovePriority) {
        uint32_t start = millis();
        uint32_t timeout = start + _runtimeLimit;
        bool updateHasEvents = false;       // check if we have any events left
        bool cleanup = false;               // cleanup empty pointers

        // store size to skip all new timers that are added inside the callback
        auto size = _timers.size();
        for(size_t i = 0; i < size; i++) {
            // the pointers do not change position but may become nullptr
            auto timer = _timers[i];
            if (timer) {
                auto timerPriority = timer->_priority;
                if (timerPriority > runAbovePriority) {
                    if (timer->_callbackScheduled) {
                        timer->_callbackScheduled = false;
                        updateHasEvents = true; // update event flag
                        __LDBG_IF(
                            uint32_t _start = micros();
                            bool check = timerPriority > PriorityType::NORMAL;
                        );
                        timer->_invokeCallback(timer);
                        __LDBG_IF(
                            uint32_t _dur = micros() - _start;
                            if (check && _dur > 5000) {
                                __LDBG_printf("timer with priority above NORMAL time=%u limit=15000", _dur);
                            }
                        );
                        // check after _invokeCallback() and only if priority is NORMAL or less
                        // priority above NORMAL will be executed regardless any timeout
                        if (timerPriority <= PriorityType::NORMAL && millis() >= timeout) {
                            __LDBG_printf("timer=%p time=%d limit=%d", timer, millis() - start, _runtimeLimit);
                            cleanup = false; // skip cleanup and update event flag
                            updateHasEvents = true;
                            break;
                        }
                    }
                }
            }
            else {
                cleanup = true;
            }
        }

#if DEBUG_EVENT_SCHEDULER
        int left = 0;
        int deleted = 0;
        if (runAbovePriority == PriorityType::NONE) {
            for(const auto timer: _timers) {
                if (!timer) {
                    deleted++;
                }
                else if (timer->_callbackScheduled) {
                    left++;
                }
            }
        }
#endif

        if (size != _timers.size()) {
            std::sort(_timers.begin(), _timers.end(), [](const CallbackTimerPtr a, const CallbackTimerPtr b) {
                return a && b && (b->_priority < a->_priority);
            });
        }
        else if (cleanup && runAbovePriority == PriorityType::NONE) {
            _cleanup(); // remove empty pointers
        }

        if (updateHasEvents) {
            // reset event flag if nothing is scheduled
            noInterrupts();
            _hasEvent = (int8_t)PriorityType::NONE;
            for(const auto &timer: _timers) {
                if (timer && timer->_callbackScheduled && (int8_t)timer->_priority > _hasEvent) {
                    _hasEvent = (int8_t)timer->_priority;
                }
            }
            interrupts();
        }
#if DEBUG_EVENT_SCHEDULER
        if (runAbovePriority == PriorityType::LOWEST) {
            if (left || deleted || _hasEvent != (int8_t)PriorityType::NONE) {
                __LDBG_printf("skipped=%d deleted=%d has_event=%d", left, deleted, _hasEvent);
            }
        }
#endif
    }
}

void ICACHE_RAM_ATTR Scheduler::__TimerCallback(void *arg)
{
    auto timer = reinterpret_cast<CallbackTimerPtr>(arg);
    if (timer->_remainingDelay >= 1) {
        uint32_t delay = (--timer->_remainingDelay) ?
            kMaxDelay // repeat until delay <= max delay
            :
            (timer->_delay % kMaxDelay);
        ets_timer_arm_new(&timer->_etsTimer, delay, false, true);
    }
    else {
        timer->_callbackScheduled = true;
        if ((int8_t)timer->_priority > __Scheduler._hasEvent) {
            __Scheduler._hasEvent = (int8_t)timer->_priority;
        }
    }
}

#if DEBUG_EVENT_SCHEDULER

void Scheduler::__list(bool debug)
{
    Print &output = debug ? DEBUG_OUTPUT : Serial;

    if (debug) {
        __DBG_print("listing timers:");
    }

    if (_timers.empty()) {
        output.println(F("no timers attached"));
    } else {
        int scheduled = 0;
        int deleted = 0;
        for(const auto timer: _timers) {
            if (timer) {
                output.printf_P(PSTR("ETSTimer=%p func=%p arg=%p managed=%p dly=%.0f (%.3fs) repeat=%d prio=%d scheduled=%d %s:%u\n"),
                    &timer->_etsTimer, timer->_etsTimer.timer_func, timer, timer->_timer, timer->_delay / 1.0, timer->_delay / 1000.0, timer->_repeat._repeat, timer->_priority, timer->_callbackScheduled, __S(timer->_file), timer->_line
                );
                if (timer->_callbackScheduled) {
                    scheduled++;
                }
            }
            else {
                deleted++;
            }
        }
        output.printf_P(PSTR("timers=%d scheduled=%d deleted=%d sizeof(EventTimer)=%u\n"), _timers.size(), scheduled, deleted, sizeof(CallbackTimer));
    }
}

#else

void Scheduler::__list(bool debug) {}

#endif


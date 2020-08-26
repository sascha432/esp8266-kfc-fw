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
#include <debug_helper_enable_mem.h>
#define DEBUG_EVENT_SCHEDULER_ASSERT        1
#else
#include <debug_helper_disable.h>
// enable extra asserts
#define DEBUG_EVENT_SCHEDULER_ASSERT        1
#endif

#if DEBUG_EVENT_SCHEDULER_ASSERT
#include <MicrosTimer.h>
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
 * - no IRAM is used
 * - all code is executed in the main loop
 * - runtime limit per loop()
 * - high priority timers have no runlimit
 * - priority TIMER executed directly
 */
Scheduler::Scheduler() :
    _size(0),
    _hasEvent(false),
    _addedFlag(false),
    _removedFlag(false)
#if DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR == 0
    , _runtimeLimit(kMaxRuntimeLimit)
#endif
{
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

    auto timerPtr = __LDBG_new(CallbackTimer, callback, delay, repeat, priority);
    _timers.push_back(timerPtr);
    _addedFlag = true;

#if DEBUG_EVENT_SCHEDULER_ASSERT
    __DBG_assert_printf(delay >= kMinDelay, "delay delay=%u limit=%u", (uint32_t)delay, kMinDelay);
    if (priority == PriorityType::TIMER) {
        __DBG_assert_printf(repeat._repeat == RepeatType::kPreset || repeat._repeat == RepeatType::kUnlimited || repeat._repeat == RepeatType::kNoRepeat, "invalid repeat value %u", repeat._repeat);
        __DBG_assert_printf(delay < kMaxDelay, "delay delay=%u limit=%u", (uint32_t)delay, kMaxDelay);
    }
#endif
#if DEBUG_EVENT_SCHEDULER
    timerPtr->_file = DebugContext::__pos._file;
    timerPtr->_line = DebugContext::__pos._line;
    __LDBG_printf("dly=%.0f maxrep=%d prio=%u callback=%p timer=%p %s:%u", delay / 1.0, repeat._repeat, priority, lambda_target(callback), timerPtr, __S(DebugContext::__pos._file), DebugContext::__pos._line);
    DebugContext::__pos = DebugContext();
#endif

    timerPtr->_initTimer();
    return timerPtr;
}

void Scheduler::end()
{
    __LDBG_print("terminating scheduler");
    // disarm all timers to ensure __TimerCallback is not called while deleting them
    for(const auto &timer: _timers) {
        if (timer) {
            timer->_disarm();
            if (timer->_timer) {
                timer->_timer->_managedTimer.clear();
            }
        }
    }
    // move vector before deleting all CallbackTimer objects
    auto tmp = std::move(_timers);
    std::swap(_timers, tmp);
    for(auto timer: tmp) {
        if (timer) {
            delete timer;
        }
    }
    _hasEvent = (int8_t)PriorityType::NONE;
    _addedFlag = false;
    _removedFlag = false;
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
        assert(iterator != _timers.end());
        if (iterator != _timers.end()) {
            __LDBG_printf("timer=%p iterator=%p managed=%p %s:%u", timer, *iterator, timer->_timer, __S(timer->_file), timer->_line);
            // disarm and delete
            timer->_disarm();
            // mark as deleted in vector
            *iterator = nullptr;
            _removedFlag = true;
            __LDBG_delete(timer);
            return true;
        }
    }
    return false;
}

#if _MSC_VER

static void __dump(Event::TimerVector &timers)
{
    Serial.printf("--- %u\n", timers.size());
    int i = 0;
    for (auto &timer : timers) {
        if (timer) {
            Serial.printf("%03d %u\n", i, timer->_priority);
        }
        else {
            Serial.printf("%03d null\n", i);
        }
        i++;
    }
}

#endif

void Scheduler::_cleanup()
{
    _timers.erase(std::remove(_timers.begin(), _timers.end(), nullptr), _timers.end());
    _timers.shrink_to_fit();
    _size = _timers.size();
    _removedFlag = false;
}

void Event::Scheduler::_sort()
{
    // if _size changed, new timers have been added
    __LDBG_printf("size=%u timers=%u", _size, _timers.size());
    //__dump(_timers);
    // sort by priority and move all nullptr to the end for removal
    std::sort(_timers.begin(), _timers.end(), [](const CallbackTimerPtr a, const CallbackTimerPtr b) {
        return (a && b) ? (b->_priority < a->_priority) : (b < a);
    });
    // remove all null pointers with std::find()
    _timers.erase(std::find(_timers.begin(), _timers.end(), nullptr), _timers.end());
    _timers.shrink_to_fit();
    _size = _timers.size();
    //__dump(_timers);
    _addedFlag = false;
    _removedFlag = false;
}

void Scheduler::run(PriorityType runAbovePriority)
{
    __Scheduler._run(runAbovePriority);
}

void Scheduler::run()
{
    __Scheduler._run();
}

void Scheduler::_run(PriorityType runAbovePriority)
{
    if (_hasEvent > (int8_t)runAbovePriority) {
        // store size to skip all new timers that are added inside the callback
        for(size_t i = 0; i < _size; i++) {
            // the pointers do not change position but may become nullptr
            auto timer = _timers[i];
            if (timer) {
#if DEBUG_EVENT_SCHEDULER_ASSERT
                // store values, timer might become a dangling pointer
                auto timerPriority = timer->_priority;
#if DEBUG_EVENT_SCHEDULER
                auto timerFile = timer->_file;
                auto timerLine = timer->_line;
#endif
#endif
                if (timer->_priority > runAbovePriority && timer->_callbackScheduled) {
                    timer->_callbackScheduled = false;
#if DEBUG_EVENT_SCHEDULER_ASSERT
                    uint32_t _diff = (timer->_priority > PriorityType::NORMAL);
                    uint32_t _start = _diff ? micros() : 0;
#endif
                    timer->_invokeCallback(timer);
#if DEBUG_EVENT_SCHEDULER_ASSERT
                    if (_diff && (_diff = get_time_diff(_start, micros())) > kMaxRuntimePrioAboveNormal) {
#if DEBUG_EVENT_SCHEDULER
                        __DBG_printf("timer=%p priority=%u time=%u limit=%u %s:%u", timer, timerPriority, _diff, kMaxRuntimePrioAboveNormal, __S(timerFile), timerLine);
#else
                        __DBG_printf("timer=%p priority=%u time=%u limit=%u", timer, timerPriority, _diff, kMaxRuntimePrioAboveNormal);
#endif
                    }
#endif
                }
            }
        }
    }
}

void Scheduler::_run()
{
    if (_hasEvent != (int8_t)PriorityType::NONE) {
        uint32_t start = millis();
        uint32_t timeout = start + _runtimeLimit;
        // cleanup empty pointers
        // store size to skip all new timers that are added inside the callback
        for (size_t i = 0; i < _size; i++) {
            // the pointers do not change position but may become nullptr
            auto timer = _timers[i];
            if (timer) {
                auto timerPriority = timer->_priority;
                if (timer->_callbackScheduled) {
                    timer->_callbackScheduled = false;
#if DEBUG_EVENT_SCHEDULER_ASSERT
                    uint32_t _diff = (timerPriority > PriorityType::NORMAL);
                    uint32_t _start = _diff ? micros() : 0;
#endif
                    timer->_invokeCallback(timer);
#if DEBUG_EVENT_SCHEDULER_ASSERT
                    if (_diff && (_diff = get_time_diff(_start, micros())) > kMaxRuntimePrioAboveNormal) {
                        __DBG_printf("timer=%p priority=%u time=%u limit=%u %s:%u", timer, timerPriority, _diff, kMaxRuntimePrioAboveNormal, __LDBG_IF(__S(timer->_file), timer->_line) __LDBG_N_IF(PSTR(""), 0));
                    }
#endif
                    // check after _invokeCallback() and only if priority is NORMAL or less
                    // priority above NORMAL will be executed regardless any timeout
                    if (timerPriority <= PriorityType::NORMAL && millis() >= timeout) {
                        __LDBG_printf("timer=%p time=%d limit=%d", timer, millis() - start, _runtimeLimit);
                        break;
                    }
                }
            }
        }

#if DEBUG_EVENT_SCHEDULER
        int left = 0;
        int deleted = 0;
        for (const auto timer : _timers) {
            if (!timer) {
                deleted++;
            }
            else if (timer->_callbackScheduled) {
                left++;
            }
        }
#endif

        // reset event flag if nothing is scheduled
        noInterrupts();
        _hasEvent = (int8_t)PriorityType::NONE;
        for (const auto &timer : _timers) {
            if (timer && timer->_callbackScheduled && (int8_t)timer->_priority > _hasEvent) {
                _hasEvent = (int8_t)timer->_priority;
            }
        }
#if DEBUG_EVENT_SCHEDULER
        if (left || deleted || _hasEvent != (int8_t)PriorityType::NONE) {
            __LDBG_printf("skipped=%d deleted=%d has_event=%d", left, deleted, _hasEvent);
        }
#endif
    }
    if (_addedFlag) {
        _sort();
    }
    else if (_removedFlag) {
        _cleanup();
    }
}

void Scheduler::__TimerCallback(void *arg)
{
    auto timer = reinterpret_cast<CallbackTimerPtr>(arg);
    if (timer->_priority == PriorityType::TIMER) {
#if DEBUG_EVENT_SCHEDULER_ASSERT
        uint32_t start = micros();
#endif
        timer->_callback(timer);
#if DEBUG_EVENT_SCHEDULER_ASSERT
        auto diff = get_time_diff(start, micros());
        if (diff > kMaxRuntimePrioTimer) {
            __DBG_printf("timer=%p priority=%u time=%u limit=%u %s:%u", timer, timer->_priority, diff, kMaxRuntimePrioTimer, __LDBG_IF(__S(timer->_file), timer->_line) __LDBG_N_IF(PSTR(""), 0));
        }
#endif
        if (!timer->isArmed()) {
            __Scheduler._removeTimer(timer);
        }
    }
    else if (timer->_remainingDelay >= 1) {
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
    }
    else {
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


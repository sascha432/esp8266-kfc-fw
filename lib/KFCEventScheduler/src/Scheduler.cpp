/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <PrintString.h>
#include "LoopFunctions.h"
#include "Event.h"
#include "Scheduler.h"
#include "CallbackTimer.h"
#include "ManagedTimer.h"
#include "Timer.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
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
 * - no loop() function that wastes CPU cycles except a single comparison
 * - no IRAM is used
 * - all code is executed in the main loop()
 * - runtime limit per loop()
 * - high priority timers have no runtime limit
 * - priority TIMER is executed directly as callback or user task, not as ISR
 * - min. interval 5 milliseconds or 100 microseconds for PriorityType::TIMER
 */
Scheduler::Scheduler() :
    _size(0),
    _hasEvent(PriorityType::NONE),
    _addedFlag(false),
    _removedFlag(false),
    _checkTimers(false)
#if DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR == 0
    , _runtimeLimit(kMaxRuntimeLimit)
#endif
{
}

Scheduler::~Scheduler()
{
    end();
}

CallbackTimer *Scheduler::_add(int64_t delay, RepeatType repeat, Callback callback, PriorityType priority)
{
    auto timerPtr = new CallbackTimer(callback, delay, repeat, priority);
    _timers.push_back(timerPtr);
    _addedFlag = true;

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
    _size = 0;
    _hasEvent = PriorityType::NONE;
    _addedFlag = false;
    _removedFlag = false;
    _checkTimers = false;
}

bool Scheduler::_hasTimer(CallbackTimerPtr timer) const
{
    return (timer == nullptr) ? false : (std::find(_timers.begin(), _timers.end(), timer) != _timers.end());
}

bool Scheduler::_removeTimer(CallbackTimerPtr timer)
{
    if (timer) {
        auto iterator = std::find(_timers.begin(), _timers.end(), timer);
        EVENT_SCHEDULER_ASSERT(iterator != _timers.end());
        if (iterator != _timers.end()) {
            __LDBG_printf("timer=%p iterator=%p managed=%p %s:%u", timer, *iterator, timer->_timer, __S(timer->_file), timer->_line);
            // disarm and delete
            timer->_disarm();
            // mark as deleted in vector
            *iterator = nullptr;
            _removedFlag = true;
            _checkTimers = true;
            delete timer;
            return true;
        }
        else {
            // __LDBG_printf(_VT100(bold_red) "timer=%p NOT FOUND" _VT100(reset), timer);
            __DBG_assert_printf(false, "timer=%p NOT FOUND", timer);
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
    // if _fp_size changed, new timers have been added
    __LDBG_printf("size=%u timers=%u", _size, _timers.size());
    //__dump(_timers);
    // sort by priority and move all nullptr to the end for removal
    portMuxLock mLock(_mux);
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
    if (_hasEvent > runAbovePriority) {
        for(int i = 0; i < _size; i++) {
            auto timer = _timers[i];
            if (timer) {
                if (timer->_priority > runAbovePriority && timer->_callbackScheduled) {
                    timer->_callbackScheduled = false;
                    timer->_invokeCallback(timer);
                }
            }
        }
    }
}

void Scheduler::_run()
{
    // low priority run
    // any events scheduled?
    if (_hasEvent != PriorityType::NONE) {
        uint32_t start = millis();
        uint32_t timeout = start + _runtimeLimit; // time limit for PriorityType::NORMAL and below
        for (int i = 0; i < _size; i++) {
            auto timer = _timers[i];
            if (timer) {
                if (timer->_priority <= PriorityType::NORMAL && millis() > timeout) {
                    __LDBG_printf(_VT100(bold_red) "runtime limit=%u exceeded=%u" _VT100(reset), _runtimeLimit, millis() - start);
                    break;
                }
                if (timer->_callbackScheduled) {
                    _mux.enterISR();
                    timer->_callbackScheduled = false;
                    _mux.exitISR();
                    timer->_invokeCallback(timer);
                }
            }
        }

        // reset event flag
        {
            InterruptLock lock;
            portMuxLock mLock(_mux);
            _hasEvent = PriorityType::NONE;
            for (const auto &timer : _timers) {
                if (timer && timer->_callbackScheduled && timer->_priority > _hasEvent) {
                    _hasEvent = timer->_priority;
                }
            }
        }

    }

    // sort new timers and remove null pointers
    if (_addedFlag) {
        _sort();
    }
    // remove null pointers
    else if (_removedFlag) {
        _cleanup();
    }
}

void Scheduler::__TimerCallback(void *arg)
{
    auto timer = reinterpret_cast<CallbackTimerPtr>(arg);
    if (timer->_priority == PriorityType::TIMER) {
        // PriorityType::TIMER invoke now
        __Scheduler._invokeCallback(timer, kMaxRuntimePrioTimer);
    }
    else if (timer->_remainingDelay >= 1) {
        // continue with remaining delay
        uint32_t delay = (--timer->_remainingDelay) ? kMaxDelay : (timer->_delay % kMaxDelay);
        ets_timer_arm_new(&timer->_etsTimer, delay, false, true);
    }
    else {
        // schedule for execution in main loop
        timer->_callbackScheduled = true;
        if (timer->_priority > __Scheduler._hasEvent) {
            __Scheduler._hasEvent = timer->_priority;
        }
    }
}

void Scheduler::_invokeCallback(CallbackTimerPtr timer, uint32_t runtimeLimit)
{
    // any delete sets this flag to true
    _checkTimers = false;

    String fpos = timer->__getFilePos();
    uint32_t start = runtimeLimit ? micros() : 0;
    timer->_callback(timer);
    uint32_t diff = runtimeLimit ? get_time_diff(start, micros()) : 0;

    if (diff > runtimeLimit) {
        __LDBG_printf(_VT100(bold_red) "timer=%p time=%u limit=%u exceeded%s" _VT100(reset), timer, diff, runtimeLimit, fpos.c_str());
    }

    // verify _checkTimers is set. if false the timer must still exist
    EVENT_SCHEDULER_ASSERT(_hasTimer(timer) || _checkTimers);

    // if _checkTimers is set, we need to check if the timer was removed
    if (_checkTimers && !_hasTimer(timer)) {
        __LDBG_printf(_VT100(bold_green) "timer=%p removed%s" _VT100(reset), timer, fpos.c_str());
        return;
    }

    if (timer->isArmed() == false) { // check if timer is still armed
        __LDBG_printf("timer=%p armed=%u cb=%p %s:%u", timer, timer->isArmed(), lambda_target(timer->_callback), __S(timer->_file), timer->_line);
        // remove disarmed timer
        _removeTimer(timer);
    }
    else if (timer->_repeat._doRepeat() == false) { // check if the timer has any repetitions left
        __LDBG_printf("timer=%p armed=%u cb=%p %s:%u", timer, timer->isArmed(), lambda_target(timer->_callback), __S(timer->_file), timer->_line);
        _removeTimer(timer);
    }
    else if (timer->_maxDelayExceeded) {
        // if max delay is exceeded we need to manually reschedule
        __LDBG_printf("timer=%p armed=%u cb=%p %s:%u", timer, timer->isArmed(), lambda_target(timer->_callback), __S(timer->_file), timer->_line);
        timer->_disarm();
        timer->_rearm();
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


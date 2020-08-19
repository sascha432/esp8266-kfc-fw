/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "LoopFunctions.h"
#include "Event.h"
#include "Scheduler.h"
#include "CallbackTimer.h"

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
 *  - Wake up device
 *  - Support for intervals of months in millisecond precision
 *  - All code is executed in the main loop and no IRAM is used
 *
 */

TimerPtr &Scheduler::add(int64_t delay, RepeatType repeat, Callback callback, PriorityType priority)
{
    _timers.emplace_back(new CallbackTimer(callback, delay, repeat, priority));
    auto &timerPtr = _timers.back();
    __LDBG_IF(
        timerPtr->_file = DebugContext::__pos._file;
        timerPtr->_line = DebugContext::__pos._line;
        __LDBG_printf("dly=%.0f maxrep=%d prio=%u callback=%p file=%s:%u timer=%p", delay / 1.0, repeat._repeat, priority, lambda_target(callback), __S(DebugContext::__pos._file), DebugContext::__pos._line, _timers.back().get());
        DebugContext::__pos = DebugContext();
    );

    // __list();
    timerPtr->initTimer();
    return timerPtr;
}

TimerPtr &Scheduler::add(milliseconds delay, RepeatType repeat, Callback callback, PriorityType priority)
{
    return add(delay.count(), repeat, callback, priority);
}

void Scheduler::begin()
{
    LoopFunctions::add(loop);
}

void Scheduler::end()
{
    LoopFunctions::remove(loop);
    noInterrupts();
    _timers.clear();
    interrupts();
}

void Scheduler::remove(TimerPtr &timer)
{
    if (timer) {
        _removeTimer(timer.get());
    }
}

TimerVectorIterator Scheduler::_getTimer(CallbackTimer *timer)
{
    return std::find_if(_timers.begin(), _timers.end(), std::compare_unique_ptr(timer));
}

bool Scheduler::_hasTimer(CallbackTimer *timer) const
{
    return (timer == nullptr) ? false : (std::find_if(_timers.begin(), _timers.end(), std::compare_unique_ptr(timer)) != _timers.end());
}


bool Scheduler::_removeTimer(CallbackTimer *timer)
{
    if (timer) {
        auto iterator = _getTimer(timer);
        __LDBG_assert(iterator != _timers.end());
        if (iterator != _timers.end()) {
            __LDBG_printf("erase timer=%p iterator=%p", timer, iterator->get());
            _timers.erase(iterator);
            _timers.shrink_to_fit();
            return true;
        }
    }
    return false;
}


void Scheduler::_cleanup()
{
    _timers.erase(std::remove(_timers.begin(), _timers.end(), nullptr));
    _timers.shrink_to_fit();
    // __list();
}

void Scheduler::loop()
{
    __Scheduler._loop();
}

void Scheduler::_loop()
{
    if (!_timers.empty()) {
        uint32_t start = millis();
        uint32_t timeout = start + _runtimeLimit;

        bool cleanup = false;
        for(auto &timer: _timers) {
            if (timer) {
                if (timer->_callbackScheduled) {
                    timer->_callbackScheduled = false;
                    timer->_invokeCallback(timer);
                    if (millis() >= timeout) {
                        __LDBG_printf("timer=%p time=%d limit=%d", timer.get(), millis() - start, _runtimeLimit);
                        break;
                    }
                }
            }
            else {
                cleanup = true;
            }
        }

        if (cleanup) {
            _cleanup();
        }

        __LDBG_IF(
            int left = 0;
            int deleted = 0;
            for(const auto &timer: _timers) {
                if (!timer) {
                    deleted++;
                }
                else if (timer->_callbackScheduled) {
                    left++;
                }
            }

            if (left) {
                __LDBG_printf("left=%d del=%d", left, deleted);
            }
        );
    }
}

void ICACHE_RAM_ATTR Scheduler::__TimerCallback(void *arg)
{
    auto timer = reinterpret_cast<CallbackTimer *>(arg);
    if (timer->_remainingDelay) {
        timer->_rearm();
    }
    else {
        timer->_callbackScheduled = true;
    }
}

__LDBG_S_IF(

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
        for(const auto &timer: _timers) {
            if (timer.get()) {
                output.printf_P(PSTR("timer=%p dly=%.0f (%.3fs) repeat=%d scheduled=%d file=%s:%u\n"),
                    timer.get(), timer->_delay / 1.0, timer->_delay / 1000.0, timer->_repeat._repeat, timer->_callbackScheduled, timer->_file, timer->_line
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

, // no debug

void Scheduler::__list(bool debug) {}

);

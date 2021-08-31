/**
  Author: sascha_lammers@gmx.de
*/

#include "CallbackTimer.h"
#include "Event.h"
#include "EventScheduler.h"
#include "Scheduler.h"
#include <Arduino_compat.h>
#include <stl_ext/utility.h>

#if DEBUG_EVENT_SCHEDULER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

using namespace Event;

CallbackTimer::CallbackTimer(const char *name, Callback callback, int64_t delay, RepeatType repeat, PriorityType priority) :
    _etsTimer(OSTIMER_NAME_VAR(name)),
    _callback(callback),
    _timer(nullptr),
    _delay(std::max<int64_t>(kMinDelay, delay)),
    #if SCHEDULER_HAVE_REMAINING_DELAY
        _remainingDelay(0),
    #endif
    _repeat(repeat),
    _priority(priority),
    #if SCHEDULER_HAVE_REMAINING_DELAY
        _maxDelayExceeded(false),
    #endif
    _callbackScheduled(false)
{
    EVENT_SCHEDULER_ASSERT(delay >= kMinDelay);
}

CallbackTimer::~CallbackTimer()
{
    __LDBG_printf("_timer=%p has_timer=%u(%p)", _timer, __Scheduler._hasTimer(this), this);

    if (_timer) {
        __LDBG_printf("removing managed timer=%p", _timer);
        _timer->_managedTimer.clear();
    }

    // the Timer object must be nullptr
    EVENT_SCHEDULER_ASSERT(_timer == nullptr);
    // the CallbackTimer must be removed from the list
    EVENT_SCHEDULER_ASSERT(__Scheduler._hasTimer(this) == false);
    // ets_timer must be disarmed
    EVENT_SCHEDULER_ASSERT(isArmed() == false);

    __LDBG_printf("ets_timer=%p running=%p armed=%d %s:%u", &_etsTimer, _etsTimer.isRunning(), isArmed(), __S(_file), _line);

    PORT_MUX_LOCK_BLOCK(_mux) {
        _etsTimer.done();
    }
}

void CallbackTimer::_rearm()
{
    bool repeat;

    EVENT_SCHEDULER_ASSERT(_callbackScheduled == false);
    EVENT_SCHEDULER_ASSERT(isArmed() == false);

    #if SCHEDULER_HAVE_REMAINING_DELAY

        EVENT_SCHEDULER_ASSERT(_remainingDelay == 0);

        uint32_t delay;
        // split interval in multiple parts if max delay is exceeded
        if (_delay > kMaxDelay) {
            uint32_t lastDelay = _delay % kMaxDelay;
            // adjust the _delay here to avoid checking in the ISR
            // delay must be between kMinDelay and (kMaxDelay - 1)
            if (lastDelay == 0) {
                _delay--;
            }
            else if (lastDelay < kMinDelay) {
                _delay += kMinDelay - lastDelay;
            }
            _remainingDelay = static_cast<decltype(_remainingDelay)>((_delay / kMaxDelay) + 1); // 1 to n times kMaxDelay
            // store state to avoid comparing uint64_t inside isr
            _maxDelayExceeded = true;
            delay = kMaxDelay;
            repeat = false; // we manually repeat
            __LDBG_printf("delay=%.0f repeat=%u * %d + %u", _delay / 1.0, (_remainingDelay - 1), kMaxDelay, (uint32_t)(_delay % kMaxDelay));
        }
        else {
            // delay that can be handled by ets timer
            delay = std::max(kMinDelay, static_cast<uint32_t>(_delay));
            _maxDelayExceeded = false;
            repeat = _repeat._hasRepeat();
            __LDBG_printf("delay=%d repeat=%u", delay, repeat);
        }
    #else
        int64_t delay = std::clamp<int64_t>(_delay, kMinDelay, kMaxDelay);
        repeat = _repeat._hasRepeat();
        __LDBG_printf("delay=%.0f repeat=%u", (float)delay, repeat);
    #endif

    PORT_MUX_LOCK_BLOCK(_mux) {
        _etsTimer.create(Scheduler::__TimerCallback, this);
        _etsTimer.arm(delay, repeat, true);
    }
}

#if DEBUG_EVENT_SCHEDULER

#include <PrintString.h>

String CallbackTimer::__getFilePos()
{
    return PrintString(F(" %s:%u"), __S(_file), _line);
}

#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

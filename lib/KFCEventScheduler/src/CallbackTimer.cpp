/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "Event.h"
#include "CallbackTimer.h"
#include "EventScheduler.h"
#include "Scheduler.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Event;

CallbackTimer::CallbackTimer(Callback callback, int64_t delay, RepeatType repeat, PriorityType priority) :
    _etsTimer({}),
    _callback(callback),
    _timer(nullptr),
    _delay(std::max_signed(kMinDelay, delay)),
    _repeat(repeat),
    _priority(priority),
    _remainingDelay(0),
    _callbackScheduled(false),
    _maxDelayExceeded(false)
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

    __LDBG_printf("ets_timer=%p func=%p armed=%d %s:%u", &_etsTimer, _etsTimer.timer_func, isArmed(), __S(_file), _line);

    ets_timer_done(&_etsTimer);
}

void CallbackTimer::rearm(int64_t delay, RepeatType repeat, Callback callback)
{
    _disarm();
    EVENT_SCHEDULER_ASSERT(delay >= kMinDelay);
    _delay = std::max_signed(kMinDelay, delay);
    if (repeat._repeat != RepeatType::kPreset) {
        _repeat._repeat = repeat._repeat;
    }
    EVENT_SCHEDULER_ASSERT(_repeat._repeat != RepeatType::kPreset);
    if (callback) {
        _callback = callback;
    }
    __LDBG_printf("rearm=%.0f timer=%p repeat=%d cb=%p %s:%u", delay / 1.0, this, _repeat._repeat, lambda_target(_callback), __S(_file), _line);
    _rearm();
}

void CallbackTimer::rearm(milliseconds interval, RepeatType repeat, Callback callback)
{
    rearm(interval.count(), repeat, callback);
}

void CallbackTimer::disarm()
{
    __LDBG_printf("disarm timer=%p %s:%u", this, __S(_file), _line);
    _disarm();
}

void CallbackTimer::_rearm()
{
    uint32_t delay;
    bool repeat;

    EVENT_SCHEDULER_ASSERT(_remainingDelay == 0);
    EVENT_SCHEDULER_ASSERT(_callbackScheduled == false);
    EVENT_SCHEDULER_ASSERT(isArmed() == false);

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
        _remainingDelay = (_delay / kMaxDelay) + 1; // 1 to n times kMaxDelay
        // store state to avoid comparing uint64_t inside isr
        _maxDelayExceeded = true;
        delay = kMaxDelay;
        repeat = false; // we manually repeat
        __LDBG_printf("delay=%.0f repeat=%u * %d + %u", _delay / 1.0, (_remainingDelay - 1), kMaxDelay, (uint32_t)(_delay % kMaxDelay));
    }
    else {
        // delay that can be handled by ets timer
        delay = std::max(kMinDelay, (uint32_t)_delay);
        _maxDelayExceeded = false;
        repeat = _repeat._hasRepeat();
        __LDBG_printf("delay=%d repeat=%u", delay, repeat);
    }

    ets_timer_setfn(&_etsTimer, Scheduler::__TimerCallback, this);
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void CallbackTimer::_disarm()
{
    __LDBG_printf("timer=%p armed=%u cb=%p %s:%u", this, isArmed(), lambda_target(_callback), __S(_file), _line);
    if (isArmed()) {
        ets_timer_disarm(&_etsTimer);
        _remainingDelay = 0;
        _callbackScheduled = false;
        _maxDelayExceeded = false;
        // set timer_func to nullptr as indication that the timer is disarmed
        _etsTimer.timer_func = nullptr;
    }
}

void CallbackTimer::_invokeCallback(CallbackTimerPtr timer)
{
    __Scheduler._invokeCallback(timer, _runtimeLimit(timer->_priority));
}

uint32_t CallbackTimer::_runtimeLimit(PriorityType priority) const
{
    if (priority == PriorityType::TIMER) {
        return kMaxRuntimePrioTimer;
    }
    else if (priority > PriorityType::NORMAL) {
        return kMaxRuntimePrioAboveNormal;
    }
    return 0;
}

#if DEBUG_EVENT_SCHEDULER

#include <PrintString.h>

String CallbackTimer::__getFilePos()
{
    return PrintString(F(" %s:%u"), __S(_file), _line);
}

#endif

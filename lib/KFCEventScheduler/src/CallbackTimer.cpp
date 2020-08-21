/**
  Author: sascha_lammers@gmx.de
*/

#include "Event.h"
#include "CallbackTimer.h"
#include "EventScheduler.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Event;

CallbackTimer::CallbackTimer(Callback callback, int64_t delay, RepeatType repeat, PriorityType priority) :
    _etsTimer({}),
    _callback(callback),
    _delay(std::max_signed(kMinDelay, delay)),
    _repeat(repeat),
    _priority(priority),
    _remainingDelay(0),
    _callbackScheduled(false)
{
    assert(delay >= kMinDelay);
}

CallbackTimer::~CallbackTimer()
{
    __LDBG_printf("~timer=%p", this);
    assert(__Scheduler._hasTimer(this) == false); // the TimerPtr must be nullptr at this point
    __LDBG_printf("ets_timer=%p func=%p armed=%d %s:%u", &_etsTimer, _etsTimer.timer_func, isArmed(), __S(_file), _line);
    assert(_etsTimer.timer_func != Scheduler::__TimerCallback); // check if the timer was disarmed before delete was called
    ets_timer_done(&_etsTimer);
}

void CallbackTimer::_initTimer()
{
    _rearm();
}

bool CallbackTimer::isArmed() const
{
    return _etsTimer.timer_func == Scheduler::__TimerCallback;
}

int64_t CallbackTimer::getInterval() const
{
    return _delay;
}

uint32_t CallbackTimer::getShortInterval() const
{
    __DBG_assert(_delay <= std::numeric_limits<uint32_t>::max());
    return (uint32_t)_delay;
}

void CallbackTimer::setInterval(milliseconds interval)
{
    rearm(interval);
}

bool CallbackTimer::updateInterval(milliseconds interval)
{
    if (interval.count() != _delay) {
        rearm(interval);
        return true;
    }
    return false;
}

void CallbackTimer::rearm(int64_t delay, RepeatType repeat, Callback callback)
{
    _disarm();
    _delay = std::max_signed(kMinDelay, delay);
    _repeat |= repeat;
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

void CallbackTimer::_rearm()
{
    uint32_t delay;
    bool repeat;

    assert(_remainingDelay == 0);
    assert(_callbackScheduled == false);
    assert(isArmed() == false);

    // split interval in multiple parts if max delay is exceeded
    if (_delay > kMaxDelay) {
        uint32_t lastDelay = _delay % kMaxDelay;
        // adjust the _delay here to avoid checking in the ISR
        // delay must be between kMinDelay and (kMaxDelay - 1)
        if (lastDelay == 0) {
            _delay--;
        } else if (lastDelay < kMinDelay) {
            _delay += kMinDelay - lastDelay;
        }
        _remainingDelay = (_delay / kMaxDelay) + 1; // 1 to n times kMaxDelay
        delay = kMaxDelay;
        repeat = false; // we manually repeat
        __LDBG_printf("delay=%.0f repeat=%u * %d + %u", _delay / 1.0, (_remainingDelay - 1), kMaxDelay, (uint32_t)(_delay % kMaxDelay));
    }
    else { // default delay
        delay = std::max(kMinDelay, (uint32_t)_delay);
        repeat = _repeat._hasRepeat();
        __LDBG_printf("delay=%d", delay);
    }

    ets_timer_setfn(&_etsTimer, Scheduler::__TimerCallback, this);
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void CallbackTimer::_disarm()
{
    __LDBG_printf("arg=%p this=%p fn=%p armed=%u cb=%p %s:%u", _etsTimer.timer_arg, this, _etsTimer.timer_func, isArmed(), lambda_target(_callback), __S(_file), _line);
    if (_etsTimer.timer_func) {
        ets_timer_disarm(&_etsTimer);
        _callbackScheduled = false;
        _remainingDelay = 0;
        _etsTimer.timer_func = nullptr;
    }
}

void CallbackTimer::_invokeCallback(TimerPtr &timer)
{
    auto callbackTimer = timer.get();
    _callback(timer);

    // check if pointer was reset inside callback
    if (!timer) {
        __LDBG_printf("reset called for timer=%p", callbackTimer);
#if DEBUG_EVENT_SCHEDULER
        // avoid debug alert failing
        if (!__Scheduler._hasTimer(callbackTimer)) {
            return;
        }
#endif
        __Scheduler._removeTimer(callbackTimer);
    }
    // check if timer was removed inside callback
    else if (__Scheduler._hasTimer(callbackTimer)) {
        if (callbackTimer->isArmed() == false) { // check if timer is still armed (calling stop() disarms the timer)
            __LDBG_printf("arg=%p this=%p fn=%p armed=%u cb=%p %s:%u", _etsTimer.timer_arg, this, _etsTimer.timer_func, isArmed(), lambda_target(_callback), __S(_file), _line);
            // reset can be safely
            timer.reset();
        }
        else if (_repeat._doRepeat() == false) { // check if the timer has any repititons left
            __LDBG_printf("arg=%p this=%p fn=%p armed=%u cb=%p %s:%u", _etsTimer.timer_arg, this, _etsTimer.timer_func, isArmed(), lambda_target(_callback), __S(_file), _line);
            // disarm and reset
            _disarm();
            timer.reset();
        }
        else if (_delay > kMaxDelay) {
            // if max delay is exceeded we need to manually reschedule
            __LDBG_printf("arg=%p this=%p fn=%p armed=%u cb=%p %s:%u", _etsTimer.timer_arg, this, _etsTimer.timer_func, isArmed(), lambda_target(_callback), __S(_file), _line);
            _disarm();
            _rearm();
        }
        else {
            // auto repeat is active
            // TODO check if repeat is set
            //_etsTimer.timer_expire?
        }
    }
    else {
        __LDBG_printf("removing dangling pointer timer=%p timerPtr=%p", callbackTimer, timer.get());
        timer.release();
        __LDBG_printf("timer=%u timer=%p", (bool)timer, timer.get());
    }
}

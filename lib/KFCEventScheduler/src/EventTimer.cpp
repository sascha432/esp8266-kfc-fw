/**
  Author: sascha_lammers@gmx.de
*/

#include "EventTimer.h"
#include "EventScheduler.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

EventTimer::EventTimer(EventScheduler::Callback loopCallback, int64_t delay, EventScheduler::RepeatType repeat, EventScheduler::Priority_t priority) : _etsTimer()
{
    if (delay < MIN_DELAY) {
        __debugbreak_and_panic_printf_P(PSTR("delay %lu < %u\n"), (ulong)delay, MIN_DELAY);
    }
    _loopCallback = loopCallback;
    _callbackScheduled = false;
    _delay = delay;
    _remainingDelay = 0;
    _repeat = repeat;
    _priority = priority;
    _disarmed = true;
}

EventTimer::~EventTimer()
{
    auto hasTimer = Scheduler.hasTimer(this);
    if (hasTimer || _etsTimer.timer_func || !_disarmed)  {
        debug_printf_P(PSTR("timer=%p timer_func=%p callback=%p hasTimer=%u disarmed=%d object deleted while active\n"), this, _etsTimer.timer_func, resolve_lambda(lambda_target(_loopCallback)), hasTimer, _disarmed);
        // __debugbreak_and_panic_printf_P(PSTR("timer=%p timer_func=%p callback=%p hasTimer=%u disarmed=%d object deleted while active\n"), this, _etsTimer.timer_func, resolve_lambda(lambda_target(_loopCallback)), hasTimer, _disarmed);
    }
    // detach();
    ets_timer_done(&_etsTimer);
}

void ICACHE_RAM_ATTR EventTimer::setCallback(EventScheduler::Callback loopCallback)
{
    _loopCallback = loopCallback;
}

EventScheduler::Callback EventTimer::getCallback()
{
    return _loopCallback;
}

void ICACHE_RAM_ATTR EventTimer::setPriority(EventScheduler::Priority_t priority)
{
    _priority = priority;
}

void ICACHE_RAM_ATTR EventTimer::rearm(int64_t delay, EventScheduler::RepeatUpdateType repeat)
{
    _debug_printf_P(PSTR("rearm=%.0f repeat=%d\n"), delay / 1.0, repeat._maxRepeat);
    _delay = delay;
    _remainingDelay = 0;
    if (repeat.isUpdateRequired()) {
        _repeat = repeat;
    }
    _rearmEtsTimer();
}

void ICACHE_RAM_ATTR EventTimer::_rearmEtsTimer()
{
    if (_etsTimer.timer_func) {
        ets_timer_disarm(&_etsTimer);
        _etsTimer.timer_func = nullptr;
        _disarmed = true;
    }
    if (_remainingDelay == 0 && _delay > MAX_DELAY) { // init, rearm
        _remainingDelay = _delay;
    }

    int32_t delay;
    bool repeat = false;
    if (_remainingDelay > MAX_DELAY) { // delay too long, chop up
        _debug_printf_P(PSTR("timer=%p delay %.0f remaining %.0f delay %ums\n"), this, _delay / 1.0, _remainingDelay / 1.0, MAX_DELAY);
        _remainingDelay -= MAX_DELAY;
        delay = MAX_DELAY;
    }
    else if (_remainingDelay) { // wait for remainig delay
        delay = std::max(MIN_DELAY, (int32_t)_remainingDelay);
        _remainingDelay = 0;
        _debug_printf_P(PSTR("timer=%p delay %.0fs remaining 0s delay %ums\n"), this, _delay / 1.0, delay);
    }
    else { // default delay
        delay = std::max(MIN_DELAY, (int32_t)_delay);
        repeat = _repeat.hasRepeat();
    }
    _debug_printf_P(PSTR("rearm timer_arg=%p callback=%p\n"), _etsTimer.timer_arg, resolve_lambda(lambda_target(_loopCallback)));
    _callbackScheduled = false;
    ets_timer_setfn(&_etsTimer, reinterpret_cast<ETSTimerFunc *>(EventScheduler::_timerCallback), reinterpret_cast<void *>(this));
    _disarmed = false;
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void ICACHE_RAM_ATTR EventTimer::detach()
{
    _debug_printf_P(PSTR("timer=%p timer_func=%p callback=%p\n"), this, _etsTimer.timer_func, resolve_lambda(lambda_target(_loopCallback)));
    if (_etsTimer.timer_func) {
        ets_timer_disarm(&_etsTimer);
        _disarmed = true;
        _etsTimer.timer_func = nullptr;
    }
}

static void ICACHE_RAM_ATTR _no_deleter(void *) {
}

void ICACHE_RAM_ATTR EventTimer::_invokeCallback()
{
    EventScheduler::TimerPtr timerPtr(this, _no_deleter);
    _debug_printf_P(PSTR("timer=%p callback=%p priority %d\n"), this, resolve_lambda(lambda_target(_loopCallback)), _priority);
    _loopCallback(timerPtr);
    _repeat._counter++;
    if (_repeat.hasRepeat() && _etsTimer.timer_func != nullptr) {
        if (_delay > EventTimer::MAX_DELAY) { // if max delay is exceeded we need to reschedule
            _rearmEtsTimer();
        }
    } else {
        _debug_printf_P(PSTR("timer=%p timer_func=%p callback=%p removing EventTimer, repeat %d/%d\n"), this, _etsTimer.timer_func, resolve_lambda(lambda_target(_loopCallback)), _repeat._counter, _repeat._maxRepeat);
        Scheduler._removeTimer(this);
    }
}

void ICACHE_RAM_ATTR EventTimer::_remove()
{
    _debug_printf_P(PSTR("timer=%p callback=%p removing EventTimer, repeat %d/%d\n"), this, resolve_lambda(lambda_target(_loopCallback)), _repeat._counter, _repeat._maxRepeat);
    Scheduler._removeTimer(this);
}

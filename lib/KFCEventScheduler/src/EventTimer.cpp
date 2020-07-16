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

EventTimer::EventTimer(EventScheduler::Callback loopCallback, int64_t delay, EventScheduler::RepeatType repeat, EventScheduler::Priority_t priority) :
    _etsTimer({}),
    _loopCallback(loopCallback),
    _delay(delay),
    _remainingDelay(0),
    _priority(priority),
    _repeat(repeat),
    _callbackScheduled(false),
    _disarmed(true)
{
    if (delay < kMinDelay) {
        __debugbreak_and_panic_printf_P(PSTR("delay %lu < %u\n"), (ulong)delay, kMinDelay);
    }
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

void ICACHE_RAM_ATTR EventTimer::rearm(int64_t delay, EventScheduler::RepeatUpdateType repeat)
{
    _debug_printf_P(PSTR("rearm=%.0f rep=%d\n"), delay / 1.0, repeat._maxRepeat);
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
    if (_remainingDelay == 0 && _delay > kMaxDelay) { // init, rearm
        _remainingDelay = _delay;
    }

    int32_t delay;
    bool repeat = false;
    if (_remainingDelay > kMaxDelay) { // delay too long, chop up
        _debug_printf_P(PSTR("t=%p d=%.0f r=%.0f d=%ums\n"), this, _delay / 1.0, _remainingDelay / 1.0, MaxDelay);
        _remainingDelay -= kMaxDelay;
        delay = kMaxDelay;
    }
    else if (_remainingDelay) { // wait for remainig delay
        delay = std::max(kMinDelay, (int32_t)_remainingDelay);
        _remainingDelay = 0;
        _debug_printf_P(PSTR("t=%p d=%.0fs re0s d=%ums\n"), this, _delay / 1.0, delay);
    }
    else { // default delay
        delay = std::max(kMinDelay, (int32_t)_delay);
        repeat = _repeat.hasRepeat();
    }
    _debug_printf_P(PSTR("rearm ta=%p cb=%p\n"), _etsTimer.timer_arg, resolve_lambda(lambda_target(_loopCallback)));
    _callbackScheduled = false;
    ets_timer_setfn(&_etsTimer, reinterpret_cast<ETSTimerFunc *>(EventScheduler::_timerCallback), reinterpret_cast<void *>(this));
    _disarmed = false;
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void ICACHE_RAM_ATTR EventTimer::detach()
{
    _debug_printf_P(PSTR("t=%p tf=%p cb=%p\n"), this, _etsTimer.timer_func, resolve_lambda(lambda_target(_loopCallback)));
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
    _debug_printf_P(PSTR("t=%p cb=%p p=%d\n"), this, resolve_lambda(lambda_target(_loopCallback)), _priority);
    _loopCallback(timerPtr);
    _repeat._counter++;
    if (_repeat.hasRepeat() && _etsTimer.timer_func != nullptr) {
        if (_delay > EventTimer::kMaxDelay) { // if max delay is exceeded we need to reschedule
            _rearmEtsTimer();
        }
    } else {
        _debug_printf_P(PSTR("t=%p tf=%p cb=%p, rep=%d/%d\n"), this, _etsTimer.timer_func, resolve_lambda(lambda_target(_loopCallback)), _repeat._counter, _repeat._maxRepeat);
        Scheduler._removeTimer(this);
    }
}

void ICACHE_RAM_ATTR EventTimer::_remove()
{
    _debug_printf_P(PSTR("t=%p cb=%p, rep=%d/%d\n"), this, resolve_lambda(lambda_target(_loopCallback)), _repeat._counter, _repeat._maxRepeat);
    Scheduler._removeTimer(this);
}

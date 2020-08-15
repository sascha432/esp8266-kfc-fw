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
        __SLDBG_panic("delay %lu < %u", (ulong)delay, kMinDelay);
    }
}

EventTimer::~EventTimer()
{
    auto hasTimer = Scheduler.hasTimer(this);
    if (hasTimer || _etsTimer.timer_func || !_disarmed)  {
        __SLDBG_printf("timer=%p timer_func=%p callback=%p hasTimer=%u disarmed=%d object deleted while active", this, _etsTimer.timer_func, lambda_target(_loopCallback), hasTimer, _disarmed);
        // __DBG_panic("timer=%p timer_func=%p callback=%p hasTimer=%u disarmed=%d object deleted while active", this, _etsTimer.timer_func, lambda_target(_loopCallback), hasTimer, _disarmed);
    }
    // detach();
    ets_timer_done(&_etsTimer);
}

void ICACHE_RAM_ATTR EventTimer::rearm(int64_t delay, EventScheduler::RepeatUpdateType repeat)
{
    __SLDBG_printf("rearm=%.0f rep=%d", delay / 1.0, repeat._maxRepeat);
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
        __SLDBG_printf("disrearm arg=%p func=%p", _etsTimer.timer_arg, _etsTimer.timer_func);
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
        __SLDBG_printf("t=%p d=%.0f r=%.0f d=%ums", this, _delay / 1.0, _remainingDelay / 1.0, kMaxDelay);
        _remainingDelay -= kMaxDelay;
        delay = kMaxDelay;
    }
    else if (_remainingDelay) { // wait for remainig delay
        delay = std::max(kMinDelay, (int32_t)_remainingDelay);
        _remainingDelay = 0;
        __SLDBG_printf("t=%p d=%.0fs re0s d=%ums\n", this, _delay / 1.0, delay);
    }
    else { // default delay
        delay = std::max(kMinDelay, (int32_t)_delay);
        repeat = _repeat.hasRepeat();
    }
    __SLDBG_printf("rearm ta=%p cb=%p", _etsTimer.timer_arg, lambda_target(_loopCallback));
    _callbackScheduled = false;
    ets_timer_setfn(&_etsTimer, reinterpret_cast<ETSTimerFunc *>(EventScheduler::_timerCallback), reinterpret_cast<void *>(this));
    _disarmed = false;
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void ICACHE_RAM_ATTR EventTimer::detach()
{
    __SLDBG_printf("t=%p tf=%p cb=%p", this, _etsTimer.timer_func, lambda_target(_loopCallback));
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
    __SLDBG_printf("t=%p cb=%p p=%d", this, lambda_target(_loopCallback), _priority);
    _loopCallback(timerPtr);
    _repeat._counter++;
    if (_repeat.hasRepeat() && _etsTimer.timer_func != nullptr) {
        if (_delay > EventTimer::kMaxDelay) { // if max delay is exceeded we need to reschedule
            _rearmEtsTimer();
        }
    } else {
        __SLDBG_printf("t=%p tf=%p cb=%p, rep=%d/%d", this, _etsTimer.timer_func, lambda_target(_loopCallback), _repeat._counter, _repeat._maxRepeat);
        Scheduler._removeTimer(this);
    }
}

void ICACHE_RAM_ATTR EventTimer::_remove()
{
    __SLDBG_printf("t=%p cb=%p, rep=%d/%d", this, lambda_target(_loopCallback), _repeat._counter, _repeat._maxRepeat);
    Scheduler._removeTimer(this);
}

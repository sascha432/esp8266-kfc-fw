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
    _callbackScheduled(false),
    _disarmed(true)
{
    assert(delay >= kMinDelay);
}

CallbackTimer::~CallbackTimer()
{
    assert(__Scheduler._hasTimer(this) == false);
    if (_etsTimer.timer_func) {
        _disarm();
    }
    ets_timer_done(&_etsTimer);
}

void CallbackTimer::initTimer()
{
    _rearm();
}

void CallbackTimer::rearm(int64_t delay, RepeatType repeat, Callback callback, PriorityType priority)
{
    _disarm();
    _delay = std::max_signed(kMinDelay, delay);
    _remainingDelay = 0;
    _repeat |= repeat;
    if (priority != PriorityType::NONE) {
        _priority = priority;
    }
    if (callback) {
        _callback = callback;
    }
    __LDBG_printf("rearm=%.0f rep=%d cb=%p prio=%d", delay / 1.0, _repeat._repeat, lambda_target(_callback), _priority);
    assert(delay >= kMinDelay);
    _rearm();
}

void CallbackTimer::rearm(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
{
    rearm(interval.count(), repeat, callback, priority);
}

void CallbackTimer::_rearm()
{
    int32_t delay;
    bool repeat = false;

    // if _remainingDelay is 0, it is the first rearm call or _remainingDelay has become 0 again,
    // which means a repeated delay and we start all over
    if (_remainingDelay == 0 && _delay > kMaxDelay) {
        _remainingDelay = (_delay / kMaxDelay) + 1;
    }

    if (_remainingDelay > 1) { // delay too long, chop up
        _remainingDelay--;
        __LDBG_printf("dly=%.0f rdly=%.0f delay=%u", _delay / 1.0, (_delay - (_remainingDelay * kMaxDelay)) / 1.0, kMaxDelay);
        delay = kMaxDelay;
    }
    else if (_remainingDelay == 1) { // wait for remainig delay
        delay = std::max(kMinDelay, (int32_t)(_delay % kMaxDelay));
        _remainingDelay = 0;
        __LDBG_printf("dly=%.0f rdly=0 delay=%d", _delay / 1.0, delay);
    }
    else { // default delay
        delay = std::max(kMinDelay, (int32_t)_delay);
        repeat = _repeat._hasRepeat();
        __LDBG_printf("delay=%d", delay);
    }

    assert(_etsTimer.timer_func == nullptr);
    assert(_disarmed == true);

    __LDBG_printf("rearm this=%p cb=%p repeat=%u", this, lambda_target(_callback), repeat);
    _callbackScheduled = false;
    ets_timer_setfn(&_etsTimer, Scheduler::__TimerCallback, reinterpret_cast<void *>(this));
    _disarmed = false;
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void CallbackTimer::stop()
{
    __LDBG_printf("arg=%p this=%p fn=%p disarmed=%u cb=%p", _etsTimer.timer_arg, this, _etsTimer.timer_func, _disarmed,lambda_target(_callback));
    _disarm();
}

void CallbackTimer::_disarm()
{
    __LDBG_printf("arg=%p this=%p fn=%p disarmed=%u cb=%p", _etsTimer.timer_arg, this, _etsTimer.timer_func, _disarmed, lambda_target(_callback));
    if (_etsTimer.timer_func) {
        ets_timer_disarm(&_etsTimer);
        _disarmed = true;
        _etsTimer.timer_func = nullptr;
    }
}

void CallbackTimer::_invokeCallback(TimerPtr &timer)
{
#if 0
        // show recurring timers every 10 seconds only
        bool show = true;
        if (_delay < 10000) {
            if (_repeat.hasRepeat()) {
                show = _repeat._counter % (10000 / _delay) == 0;
            }
        }
        if (show) {
            //  using any sort of print here causes the IRAM to fill up (540byte are free)
            //::printf(PSTR("this=%p arg=%p cb=%p repeat=%d max=%d\n"), this, _etsTimer.timer_func, lambda_target(_callback), _repeat._counter, _repeat._maxRepeat);
        }
#endif
    _callback(timer);

    if (timer) {
        if (_etsTimer.timer_func != nullptr && _repeat._doRepeat()) { // if timer_func is null it has been detached inside the callback
            if (_delay > kMaxDelay) {
                // if max delay is exceeded we need to reschedule
                _disarm();
                _rearm();
            }
        } else {
            timer.reset();
        }
    }
}

// void CallbackTimer::_remove()
// {
//     __LDBG_printf("this=%p arg=%p cb=%p repeat=%d max=%d", this, _etsTimer.timer_func, lambda_target(_callback), _repeat._counter, _repeat._maxRepeat);
//     Scheduler._removeTimer(this);
// }

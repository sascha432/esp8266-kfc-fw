/**
  Author: sascha_lammers@gmx.de
*/

#include "Event.h"
#include "Timer.h"
#include "ManagedTimer.h"
#include "Scheduler.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Event;

Timer::Timer() : _managedTimer()
{
}

Timer::Timer(CallbackTimerPtr callbackTimer) : _managedTimer(callbackTimer, this)
{
}

Timer &Timer::operator=(CallbackTimerPtr callbackTimer)
{
    _managedTimer = ManangedCallbackTimer(callbackTimer, this);
    return *this;
}

Timer::~Timer()
{
    remove();
}

void Timer::add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority)
{
    if (_isActive()) {
        _managedTimer->rearm(intervalMillis, repeat, callback);
    }
    else {
        auto callbackTimer = __Scheduler._add(intervalMillis, repeat, callback, priority);
        _managedTimer = ManangedCallbackTimer(callbackTimer, this);
    }
}

void Timer::add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
{
    add(interval.count(), repeat, callback, priority);
}

bool Timer::remove()
{
    __LDBG_printf("_managedTimer=%u", (bool)_managedTimer);
    if (_managedTimer) {
        return _managedTimer.remove();
        // return __Scheduler._removeTimer(_managedTimer.get());
    }
    return false;
}

bool Timer::_isActive() const
{
    return (bool)_managedTimer;
}

Timer::operator bool() const
{
    return _isActive();
}

CallbackTimerPtr Timer::operator->() const noexcept
{
    return _managedTimer.get();
}

CallbackTimerPtr Timer::operator*() const noexcept
{
    return _managedTimer.get();
}

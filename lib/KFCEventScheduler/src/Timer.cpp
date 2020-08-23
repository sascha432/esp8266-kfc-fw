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

Timer::Timer() : _managedCallbackTimer()
{
}

Timer::Timer(CallbackTimerPtr &&callbackTimer) : _managedCallbackTimer(std::move(callbackTimer), this)
{
}

Timer &Timer::operator=(CallbackTimerPtr &&callbackTimer)
{
    _managedCallbackTimer = ManangedCallbackTimer(std::move(callbackTimer), this);
    return *this;
}

// Timer::Timer(Timer &&move) : _managedCallbackTimer(std::move(move))
// {
// //     __DBG_assert(_timer->_timer == nullptr || _timer->_timer == &move);
// //     _timer->_timer = this;
//  }

Timer::~Timer()
{
    remove();
}

void Timer::add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority)
{
    if (_isActive()) {
        _managedCallbackTimer->rearm(intervalMillis, repeat, callback);
    }
    else {
        auto callbackTimer = __Scheduler._add(intervalMillis, repeat, callback, priority);
        _managedCallbackTimer = ManangedCallbackTimer(callbackTimer, this);
    }
}

void Timer::add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
{
    add(interval.count(), repeat, callback, priority);
}

bool Timer::remove()
{
    if (_managedCallbackTimer) {
        return __Scheduler._removeTimer(_managedCallbackTimer.get());
    }
    return false;
}

bool Timer::_isActive() const
{
    return (bool)_managedCallbackTimer;
}

Timer::operator bool() const
{
    return _isActive();
}

CallbackTimerPtr Timer::operator->() const noexcept
{
    return _managedCallbackTimer.get();
}

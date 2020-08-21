/**
  Author: sascha_lammers@gmx.de
*/

#include "Event.h"
#include "Timer.h"
#include "Scheduler.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Event;

Timer::Timer() : _timer(nullptr)
{
}

Timer::Timer(CallbackTimer *timer) : _timer(timer)
{
}

Timer::Timer(Timer &&move) : _timer(std::exchange(move._timer, nullptr))
{
}

Timer::~Timer()
{
    __Scheduler._removeTimer(_timer);
}

void Timer::add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority)
{
    if (_isActive()) {
        _timer->rearm(intervalMillis, repeat, callback);
    }
    else {
        _timer = __Scheduler._add(intervalMillis, repeat, callback, priority).get();
    }
}

void Timer::add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
{
    add(interval.count(), repeat, callback, priority);
}

bool Timer::remove()
{
    if (_timer) {
        auto result = __Scheduler._removeTimer(_timer);
        __LDBG_printf("remove_timer=%u timer=%p", result, _timer);
        _timer = nullptr;
        return result;
    }
    return false;
}

bool Timer::_isActive() const
{
    return _timer && __Scheduler._hasTimer(_timer);
}

bool Timer::_isActive()
{
    if (_timer) {
        if (__Scheduler._hasTimer(_timer)) {
            return true;
        }
        // pointer was not found, release it
        _timer = nullptr;
    }
    return false;
}

Timer::operator bool() const
{
    return _isActive();
}

Timer::operator bool()
{
    return _isActive();
}

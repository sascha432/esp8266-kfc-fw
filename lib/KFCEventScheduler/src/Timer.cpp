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

Timer::Timer() : TimerPtr(nullptr)
{
}

Timer::Timer(CallbackTimer *timer) : TimerPtr(timer)
{

}

// Timer::Timer(Timer &&timer) : TimerPtr(std::move(timer))
// {
// }

Timer::~Timer()
{
    remove();
}

void Timer::add(int64_t delayMillis, RepeatType repeat, Callback callback, PriorityType priority)
{
    auto ptr = get();
    if (ptr && __Scheduler._hasTimer(ptr)) {
        ptr->rearm(delayMillis, repeat, callback, priority);
    }
    else {
        reset(__Scheduler.add(delayMillis, repeat, callback, ((priority == PriorityType::NONE) ? PriorityType::NORMAL : priority)).get());
    }
}

void Timer::add(milliseconds delay, RepeatType repeat, Callback callback, PriorityType priority)
{
    add(delay.count(), repeat, callback, priority);
}

bool Timer::remove()
{
    auto ptr = get();
    if (ptr) {
        __LDBG_printf("remote timer=%p", ptr);
        auto result = __Scheduler._removeTimer(ptr);
        release();
        __LDBG_printf("remove=%u release=%p", result, get());
        return result;
    }
    return false;
}

bool Timer::isActive() const
{
    CallbackTimer *ptr;
    return ((ptr = get()) != nullptr) && __Scheduler._hasTimer(ptr);
}

bool Timer::isActive()
{
    auto ptr = get();
    if (ptr) {
        if (__Scheduler._hasTimer(ptr)) {
            return true;
        }
        // pointer was not found, release it
        release();
    }
    return false;
}

Timer::operator bool() const
{
    return isActive();
}

Timer::operator bool()
{
    return isActive();
}

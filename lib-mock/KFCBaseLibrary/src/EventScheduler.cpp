/**
  Author: sascha_lammers@gmx.de
*/

#include "EventScheduler.h"
#include <thread>

EventScheduler Scheduler;

EventTimer *EventScheduler::addTimer(int64_t delayMillis, RepeatType repeat, Callback callback, Priority_t priority, DeleterCallback deleter)
{
    auto timer = new EventTimer(callback, delayMillis, repeat, priority);
    _timers.push_back(deleter ? TimerPtr(timer, deleter) : TimerPtr(timer));
    timer->initTimer();
    return timer;
}

bool EventScheduler::hasTimer(EventTimer *timer) const
{
    if (timer) {
        for (const auto &_timer : _timers) {
            if (_timer.get() == timer) {
                return true;
            }
        }
    }
    return false;
}

bool EventScheduler::removeTimer(EventTimer *timer)
{
    return false;
}

void EventScheduler::Timer::add(int64_t delayMillis, RepeatType repeat, Callback callback, Priority_t priority, DeleterCallback deleter)
{
    if (_timer) {
        remove();
    }
    _timer = Scheduler.addTimer(delayMillis, repeat, callback, priority, [this, deleter](EventTimer *timer) {
        if (deleter) {
            deleter(timer);
        }
        else {
            delete timer;
        }
        _timer = nullptr;
    });
}

bool EventScheduler::Timer::remove()
{
    if (_timer) {
        _timer->_remove();
        _timer = nullptr;
        return true;
    }
    return false;
}

bool EventScheduler::Timer::active() const
{
    return _timer && _timer->active();
}

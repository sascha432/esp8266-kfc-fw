/**
  Author: sascha_lammers@gmx.de
*/

#include "EventScheduler.h"
#include "EventTimer.h"
#include "LoopFunctions.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif


#if !defined(DISABLE_EVENT_SCHEDULER)
EventScheduler Scheduler;
#endif

extern ETSTimer *timer_list;

#include "../include/RestApi/RetrieveSymbols.h"

void EventScheduler::listETSTimers(Print &output)
{
    StringVector addresses;
    //DebugDumper dumper(output);
    ETSTimer *cur = timer_list;
    while(cur) {
        for(const auto &timer: _timers) {
            if (&timer->_etsTimer == cur) {
                addresses.push_back(PrintString(F("0x%x"), lambda_target(timer->_loopCallback)));
                addresses.push_back(PrintString(F("0x%x"), (uint32_t)cur->timer_func));
                addresses.push_back(PrintString(F("0x%x"), (uint32_t)cur->timer_arg));
                break;
            }
        }
        // float period_in_s = cur->timer_period / 312500.0;
        // output.printf_P(PSTR("ETSTimer=%p func=%p arg=%p period=%u (%.3fs) exp=%u callback=%p\n"), cur, cur->timer_func, cur->timer_arg, cur->timer_period, period_in_s, cur->timer_expire, callback);
        cur = cur->timer_next;
    }

    output.println(F("Waiting for symbols..."));
    xtra_containers::remove_duplicates(addresses);
    auto rs = new RetrieveSymbols::RestApi();
    rs->setAutoDelete(true);
    rs->setAddresses(std::move(addresses));
    rs->call([this, &output](RetrieveSymbols::JsonReaderResult *result, const String &error) {
        if (result) {
            auto &items = result->getItems();
            auto getAddr = [&items](uint32_t addr) {
                auto it = std::find(items.begin(), items.end(), addr);
                if (it == items.end()) {
                    return String('-');
                }
                auto name = it->getName();
                if (name.length() > 64) {
                    name.remove(60, -1);
                    name += F("...#");
                    name += String(std::distance(items.begin(), it));
                }
                return name;
            };
            ETSTimer *cur = timer_list;
            while(cur) {
                void *callback = nullptr;
                for(const auto &timer: _timers) {
                    if (&timer->_etsTimer == cur) {
                        callback = lambda_target(timer->_loopCallback);
                        break;
                    }
                }
                float period_in_s = cur->timer_period / 312500.0;
                output.printf_P(PSTR("ETSTimer=%p func=%p[%s] arg=%p[%s] period=%u (%.3fs) exp=%u callback=%p[%s]\n"),
                    cur,
                    cur->timer_func, getAddr((uint32_t)cur->timer_func).c_str(),
                    cur->timer_arg, getAddr((uint32_t)cur->timer_arg).c_str(),
                    cur->timer_period,
                    period_in_s,
                    cur->timer_expire,
                    callback, getAddr((uint32_t)callback).c_str());
                cur = cur->timer_next;
            }
            size_t num = 0;
            for (auto item : result->getItems()) {
                output.printf_P(PSTR("#% 2u 0x%08x 0x%08x:0x%04x %s\n"), num++, item.getSrcAddress(), item.getAddress(), item.getSize(), item.getName().c_str());
            }
        }
        else {
            output.println(error);
        }
    });
}

void EventScheduler::begin()
{
    LoopFunctions::add(loop);
}

void EventScheduler::end()
{
    LoopFunctions::remove(loop);
    noInterrupts();
    TimerVector timers;
    timers.swap(_timers);
    for(const auto &timer: timers) {
        timer->detach();
    }
    interrupts();
    // _timers.clear();
}

// EventScheduler::Timer

EventScheduler::Timer::Timer() : _timer(nullptr)
{
}

EventScheduler::Timer::~Timer()
{
    remove();
}

EventScheduler::Timer::Timer(Timer &&timer)
{
    *this = std::move(timer);
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
    _debug_printf_P(PSTR("_timer %p hasTimer=%u\n"), _timer, Scheduler.hasTimer(_timer));
    if (_timer) {
        _timer->_remove();
        _timer = nullptr;
        return true;
    }
    return false;
}

EventTimer *EventScheduler::Timer::operator->() const
{
    if (!_timer || !Scheduler.hasTimer(_timer)) {
        __debugbreak_and_panic_printf_P(PSTR("_timer=%p hasTimer=%u\n"), _timer, Scheduler.hasTimer(_timer));
    }
    return _timer;
}

/**
 *
 * class EventScheduler
 *
 *  - Wake up device
 *  - Support for more than 2 hour interval
 *  - Priorities to run inside the timer interrupt or in the main loop function
 *
 */

EventTimer *EventScheduler::addTimer(int64_t delay, RepeatType repeat, Callback callback, Priority_t priority, DeleterCallback deleter)
{
    _debug_printf_P(PSTR("delay=%.0f repeat=%d prio=%u callback=%u deleter=%u\n"), delay / 1.0, repeat._maxRepeat, priority, callback ? 1 : 0, deleter ? 1 : 0);
    auto timer = new EventTimer(callback, delay, repeat, priority);
    _timers.push_back(deleter ? TimerPtr(timer, deleter) : TimerPtr(timer));
#if DEBUG_EVENT_SCHEDULER
    Scheduler._list();
#endif
    timer->initTimer();
    return timer;
}

bool ICACHE_RAM_ATTR EventScheduler::hasTimer(EventTimer *timer) const
{
    if (timer) {
        for(const auto &_timer: _timers) {
            if (_timer.get() == timer) {
                return true;
            }
        }
    }
    return false;
}

bool EventScheduler::removeTimer(EventTimer *timer)
{
    _debug_printf_P(PSTR("timer=%p result=%d\n"), timer, hasTimer(timer));
    if (!hasTimer(timer)) {
        return false;
    }
    _removeTimer(timer);
    return true;
}

void ICACHE_RAM_ATTR EventScheduler::_removeTimer(EventTimer *timer)
{
    _debug_printf_P(PSTR("timer=%p, _etsTimer.timer_func=%p\n"), timer, timer ? timer->_etsTimer.timer_func : nullptr);

    timer->detach();

#if 0
    auto iterator = std::find(_timers.begin(), _timers.end(), timer);
    TimerPtr ptr;
    if (iterator != _timers.end()) {
        ptr = *iterator;
    }
    _timers.erase(iterator, _timers.end());
#else
    for(auto iterator = _timers.begin(); iterator != _timers.end(); ++iterator) {
        if (iterator->get() == timer) {
            _timers.erase(iterator);
            break;
        }
    }
    // _timers.erase(std::remove_if(_timers.begin(), _timers.end(), timer));
#endif

#if DEBUG_EVENT_SCHEDULER
    Scheduler._list();
#endif
}

void ICACHE_RAM_ATTR EventScheduler::_timerCallback(void *arg)
{
    auto timer = reinterpret_cast<EventTimer *>(arg);
    if (timer->_remainingDelay) {
        timer->_rearmEtsTimer();
        return;
    }
    else {
         // immediately reschedule if the delay exceeds the max. interval
        if (timer->_priority == EventScheduler::PRIO_HIGH) {
            // call high priortiy timer directly
            timer->_invokeCallback();
        }
        else {
            timer->_callbackScheduled = true;
        }
    }
}

void EventScheduler::loop()
{
    Scheduler._loop();
}

void EventScheduler::_loop()
{
    if (!_timers.empty()) {
        int runtime = 0;
        auto start = millis();

        // do not use iterators here, the vector might be modified in the timer callback
        for(size_t i = 0; i < _timers.size(); i++) {
            auto timer = _timers[i].get();
            if (timer->_callbackScheduled) {
                timer->_callbackScheduled = false;
                timer->_invokeCallback();
                if ((runtime = millis() - start) > _runtimeLimit) {
                    _debug_printf_P(PSTR("timer=%p runtime limit %d/%d reached, exiting loop\n"), timer, runtime, _runtimeLimit);
                    break;
                }
            }
        }

#if DEBUG_EVENT_SCHEDULER
        int left = 0;
        for(const auto &timer: _timers) {
            if (timer->_callbackScheduled) {
                left++;
            }
        }

        if (left) {
            _debug_printf_P(PSTR("%d callbacks still need to be scheduled after this loop\n"), left);
        }
#endif
    }
}

#if DEBUG_EVENT_SCHEDULER

void EventScheduler::_list()
{
    if (_timers.empty()) {
        _debug_printf_P(PSTR("no timers left\n"));
    } else {
        int scheduled = 0;
        for(const auto &timer: _timers) {
            _debug_printf_P(PSTR("%p: delay %.3f repeat %d/%d, scheduled %d\n"), timer.get(), timer->_delay / 1000.0, timer->_callCounter, timer->_repeat, timer->_callbackScheduled);
            if (timer->_callbackScheduled) {
                scheduled++;
            }
        }
        _debug_printf_P(PSTR("timers %d, scheduled %d\n"), _timers.size(), scheduled);
    }
}

#endif


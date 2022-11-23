/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Event.h"
#include "CallbackTimer.h"
#include "ManagedTimer.h"

#if DEBUG_EVENT_SCHEDULER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

namespace Event {

    class Scheduler;
    class ManagedCallbackTimer;
    class CallbackTimer;

    class Timer {
    public:
        Timer(const Timer &timer) = delete;
        Timer(Timer &&timer) = delete;
        Timer &operator=(const Timer &timer) = delete;

        Timer();
        Timer(CallbackTimerPtr callbackTimer);
        ~Timer();

        Timer &operator=(CallbackTimerPtr callbackTimer);

        // add() is using rearm() if the timer already exists
        // to change priority, use remove() and add()
        void add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        void add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);

        // add named timer
        void add(const char *name, int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        void add(const char *name, milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);

        // throttle a call, if called a second time before delayMillis have not been passed, it is ignored
        void throttle(uint32_t delayMillis, Callback callback, PriorityType priority = PriorityType::NORMAL);

        bool remove();

        operator bool() const;
        CallbackTimerPtr operator->() const noexcept;
        CallbackTimerPtr operator*() const noexcept;

    private:
        friend CallbackTimer;
        friend ManagedCallbackTimer;
        friend Scheduler;

        bool _isActive() const;

        ManagedCallbackTimer _managedTimer;
    };

    inline Timer::Timer() : _managedTimer()
    {
    }

    inline Timer::Timer(CallbackTimerPtr callbackTimer) : _managedTimer(callbackTimer, this)
    {
    }

    inline void Timer::throttle(uint32_t delayMillis, Callback callback, PriorityType priority)
    {
        if (_isActive()) {
            auto &timer = *_managedTimer.get();
            if (timer._repeat.getRepeatsLeft() >= RepeatType::kNoRepeat) {
                // further calls are blocked for delayMillis
                return;
            }
            // execute delayed and block all calls for delayMillis
            add(Event::milliseconds(delayMillis), 2, [callback](CallbackTimerPtr timer) {
                callback(timer);
                // disarm, we just want one call
                timer->disarm();
            }, priority);
        }
        else {
            // not active, run immediately
            add(Event::milliseconds(10), RepeatType::kNoRepeat, callback, priority);
        }
    }

    inline bool Timer::remove()
    {
        __LDBG_printf("_managedTimer=%u", (bool)_managedTimer);
        if (_managedTimer) {
            return _managedTimer.remove();
            // return __Scheduler._removeTimer(_managedTimer.get());
        }
        return false;
    }

    inline Timer &Timer::operator=(CallbackTimerPtr callbackTimer)
    {
        _managedTimer = ManagedCallbackTimer(callbackTimer, this);
        return *this;
    }

    inline Timer::~Timer()
    {
        remove();
    }

    inline void Timer::add(const char *name, milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
    {
        add(name, interval.count(), repeat, callback, priority);
    }

    inline void Timer::add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority)
    {
        add(PSTR("ManagedTimer"), intervalMillis, repeat, callback, priority);
    }

    inline void Timer::add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
    {
        add(PSTR("ManagedTimer"), interval, repeat, callback, priority);
    }

    inline bool Timer::_isActive() const
    {
        return _managedTimer.operator bool();
    }

    inline Timer::operator bool() const
    {
        return _managedTimer.operator bool();
    }

    inline CallbackTimerPtr Timer::operator->() const noexcept
    {
        return _managedTimer.get();
    }

    inline CallbackTimerPtr Timer::operator*() const noexcept
    {
        return _managedTimer.get();
    }

}

#if DEBUG_EVENT_SCHEDULER
#    include <debug_helper_disable.h>
#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

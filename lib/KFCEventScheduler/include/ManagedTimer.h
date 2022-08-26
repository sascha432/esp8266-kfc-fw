/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "CallbackTimer.h"
#include "Event.h"

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

    class Timer;

    class ManagedCallbackTimer {
    public:
        using TimerPtr = Timer *;

        ManagedCallbackTimer(const ManagedCallbackTimer &) = delete;
        ManagedCallbackTimer &operator=(const ManagedCallbackTimer &) = delete;

        ManagedCallbackTimer();
        ManagedCallbackTimer(CallbackTimerPtr callbackTimer, TimerPtr timer);

        ManagedCallbackTimer &operator=(ManagedCallbackTimer &&move) noexcept;

        ManagedCallbackTimer(CallbackTimerPtr callbackTimer);

        ~ManagedCallbackTimer();

        operator bool() const;

        CallbackTimerPtr operator->() const noexcept;
        CallbackTimerPtr get() const noexcept;
        void clear();

        bool remove();

    private:
        CallbackTimerPtr _callbackTimer;
    };

    inline ManagedCallbackTimer::ManagedCallbackTimer() : _callbackTimer(nullptr)
    {
    }

    inline ManagedCallbackTimer::ManagedCallbackTimer(CallbackTimerPtr callbackTimer) : _callbackTimer(callbackTimer)
    {
    }

    inline ManagedCallbackTimer::ManagedCallbackTimer(CallbackTimerPtr callbackTimer, TimerPtr timer) : _callbackTimer(callbackTimer)
    {
        EVENT_SCHEDULER_ASSERT(callbackTimer != nullptr);
        __LDBG_printf("callback_timer=%p timer=%p _timer=%p", callbackTimer, timer, callbackTimer->_timer);
        if (callbackTimer) {
            callbackTimer->_releaseManagedTimer();
            _callbackTimer->_timer = timer;
        }
    }

    inline ManagedCallbackTimer &ManagedCallbackTimer::operator=(ManagedCallbackTimer &&move) noexcept
    {
        EVENT_SCHEDULER_ASSERT(move._callbackTimer != nullptr);
        if (_callbackTimer) {
            _callbackTimer->_releaseManagedTimer();
        }
        EVENT_SCHEDULER_ASSERT(move._callbackTimer != nullptr);
        _callbackTimer = std::exchange(move._callbackTimer, nullptr);
        return *this;
    }

    inline ManagedCallbackTimer::~ManagedCallbackTimer()
    {
        __LDBG_printf("this=%p callback_timer=%p timer=%p", this, _callbackTimer, _callbackTimer ? _callbackTimer->_timer : nullptr);
        remove();
    }

    inline ManagedCallbackTimer::operator bool() const
    {
        return _callbackTimer != nullptr;
    }

    inline CallbackTimerPtr ManagedCallbackTimer::operator->() const noexcept
    {
        return _callbackTimer;
    }

    inline CallbackTimerPtr ManagedCallbackTimer::get() const noexcept
    {
        return _callbackTimer;
    }

}

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_disable.h>
#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

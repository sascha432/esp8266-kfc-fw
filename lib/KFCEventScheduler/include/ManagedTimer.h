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

    class ManangedCallbackTimer {
    public:
        using TimerPtr = Timer *;

        ManangedCallbackTimer(const ManangedCallbackTimer &) = delete;
        ManangedCallbackTimer &operator=(const ManangedCallbackTimer &) = delete;

        ManangedCallbackTimer();
        ManangedCallbackTimer(CallbackTimerPtr callbackTimer, TimerPtr timer);

        ManangedCallbackTimer &operator=(ManangedCallbackTimer &&move) noexcept;

        ManangedCallbackTimer(CallbackTimerPtr callbackTimer);

        ~ManangedCallbackTimer();

        operator bool() const;

        CallbackTimerPtr operator->() const noexcept;
        CallbackTimerPtr get() const noexcept;
        void clear();

        bool remove();

    private:
        CallbackTimerPtr _callbackTimer;
    };

    inline ManangedCallbackTimer::ManangedCallbackTimer() : _callbackTimer(nullptr)
    {
    }

    inline ManangedCallbackTimer::ManangedCallbackTimer(CallbackTimerPtr callbackTimer) : _callbackTimer(callbackTimer)
    {
    }

    inline ManangedCallbackTimer::ManangedCallbackTimer(CallbackTimerPtr callbackTimer, TimerPtr timer) : _callbackTimer(callbackTimer)
    {
        EVENT_SCHEDULER_ASSERT(callbackTimer != nullptr);
        __LDBG_printf("callback_timer=%p timer=%p _timer=%p", callbackTimer, timer, callbackTimer->_timer);
        if (callbackTimer) {
            callbackTimer->_releaseManagedTimer();
            _callbackTimer->_timer = timer;
        }
    }

    inline ManangedCallbackTimer &ManangedCallbackTimer::operator=(ManangedCallbackTimer &&move) noexcept
    {
        EVENT_SCHEDULER_ASSERT(move._callbackTimer != nullptr);
        if (_callbackTimer) {
            _callbackTimer->_releaseManagedTimer();
        }
        EVENT_SCHEDULER_ASSERT(move._callbackTimer != nullptr);
        _callbackTimer = std::exchange(move._callbackTimer, nullptr);
        return *this;
    }

    inline ManangedCallbackTimer::~ManangedCallbackTimer()
    {
        __LDBG_printf("this=%p callback_timer=%p timer=%p", this, _callbackTimer, _callbackTimer ? _callbackTimer->_timer : nullptr);
        remove();
    }

    inline ManangedCallbackTimer::operator bool() const
    {
        return _callbackTimer != nullptr;
    }

    inline CallbackTimerPtr ManangedCallbackTimer::operator->() const noexcept
    {
        return _callbackTimer;
    }

    inline CallbackTimerPtr ManangedCallbackTimer::get() const noexcept
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

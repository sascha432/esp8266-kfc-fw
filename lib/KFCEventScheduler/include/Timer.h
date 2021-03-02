/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Event.h"
#include "CallbackTimer.h"
#include "ManagedTimer.h"

namespace Event {

    class Scheduler;
    class ManangedCallbackTimer;
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
        bool remove();

        operator bool() const;
        CallbackTimerPtr operator->() const noexcept;
        CallbackTimerPtr operator*() const noexcept;

        // Timer(Timer &&timer) : _managedTimer(std::move(timer._managedTimer)) {}
    private:
        friend CallbackTimer;
        friend ManangedCallbackTimer;
        friend Scheduler;

        bool _isActive() const;

        ManangedCallbackTimer _managedTimer;
    };

}

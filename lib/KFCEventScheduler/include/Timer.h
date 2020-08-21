/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Event.h"
#include "CallbackTimer.h"

namespace Event {

    // nullptr safe "shared_ptr" version of TimerPtr
    class Timer {
    public:
        Timer(const TimerPtr &timer) = delete;
        Timer &operator=(const TimerPtr &timer) = delete;

        Timer();
        Timer(CallbackTimer *timer);
        Timer(Timer &&timer);
        ~Timer();

        // add() is using rearm() if the timer already exists
        // to change priority, use remove() and add()
        void add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        void add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        bool remove();

        operator bool() const;
        operator bool();

    private:
        bool _isActive() const;
        bool _isActive();

        CallbackTimer *_timer;
    };

}

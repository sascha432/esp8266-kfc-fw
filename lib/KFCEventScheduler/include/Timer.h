/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Event.h"
#include "CallbackTimer.h"

namespace Event {

    class Timer : public TimerPtr {
    public:
        using TimerPtr::TimerPtr;

        Timer(CallbackTimer *timer);

        Timer(const TimerPtr &timer) = delete;
        // Timer(Timer &&timer);

        Timer();
        ~Timer();

        void add(int64_t delayMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NONE);
        void add(milliseconds delay, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NONE);
        bool remove();

        bool isActive() const;
        bool isActive();
        operator bool() const;
        operator bool();
    };

}

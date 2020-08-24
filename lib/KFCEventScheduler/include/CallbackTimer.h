/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Event.h"

namespace Event {

    class Scheduler;
    class Timer;
    class ManangedCallbackTimer;

    class CallbackTimer {
    public:

        CallbackTimer(Callback loopCallback, int64_t delay, RepeatType repeat, PriorityType priority);
        ~CallbackTimer();

        bool isArmed() const;
        int64_t getInterval() const;

        // make sure the delay does not exceed UINT32_MAX
        uint32_t getShortInterval() const;

        // set new interval and restart timer
        void setInterval(milliseconds interval);

        // update interval and restart timer only if the interval is different
        // returns true if the interval has been changed
        bool updateInterval(milliseconds interval);

        // set new interval and repeat type
        // the current timer is restarted
        void rearm(int64_t intervalMillis, RepeatType repeat = RepeatType(), Callback callback = nullptr);

        void rearm(milliseconds interval, RepeatType repeat = RepeatType(), Callback callback = nullptr);

        // disarm timer inside callback
        // once exiting the callback it is being removed
        void disarm();

    private:
        friend Timer;
        friend ManangedCallbackTimer;
        friend Scheduler;

        void _initTimer();
        void _rearm();
        void _disarm();
        void _invokeCallback(CallbackTimerPtr timer);

    public:
        ETSTimer &__getETSTimer() { return _etsTimer; }
        Callback &__getLoopCallback() { return _callback; }
        int64_t __getRemainingDelayMillis() const { return (_remainingDelay == 0) ?  0 : ((_remainingDelay == 1) ? (_delay % kMaxDelay) : ((_remainingDelay - 1) * kMaxDelay)); }

    private:
        ETSTimer _etsTimer;
        Timer *_timer;
        Callback _callback;
        int64_t _delay;
        RepeatType _repeat;
        PriorityType _priority;
        uint32_t _remainingDelay: 17;
        uint32_t _callbackScheduled: 1;
        uint32_t ____free: 14;

#if DEBUG_EVENT_SCHEDULER
    public:
        const char *_file;
        uint32_t _line;
#endif
    };

}

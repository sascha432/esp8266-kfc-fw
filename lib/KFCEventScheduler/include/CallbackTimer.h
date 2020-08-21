/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Event.h"

namespace Event {

    class Scheduler;

    class CallbackTimer {
    public:

        CallbackTimer(Callback loopCallback, int64_t delay, RepeatType repeat, PriorityType priority);
        ~CallbackTimer();

        bool isArmed() const;
        int64_t getInterval() const;

        // make sure the delay does not exceed UINT32_MAX
        uint32_t getShortInterval() const;

        // void setCallback(Callback loopCallback) {
        //     _callback = loopCallback;
        // }

        // Callback getCallback() const {
        //     return _callback;
        // }

        // set new interval and restart timer
        void setInterval(milliseconds interval);

        // update interval and restart timer only if the interval is different
        // returns true if the interval has been changed
        bool updateInterval(milliseconds interval);

        // set new interval and repeat type
        // the current timer is restarted
        void rearm(int64_t intervalMillis, RepeatType repeat = RepeatType(), Callback callback = nullptr);

        void rearm(milliseconds interval, RepeatType repeat = RepeatType(), Callback callback = nullptr);

    private:
        friend Scheduler;

        // void _setScheduleCallback(bool enable);
        void _initTimer();
        void _rearm();
        void _disarm();
        void _invokeCallback(TimerPtr &timer);
        void _remove();

    public:
        ETSTimer &__getETSTimer() { return _etsTimer; }
        Callback &__getLoopCallback() { return _callback; }
        int64_t __getRemainingDelayMillis() const { return (_remainingDelay == 0) ?  0 : ((_remainingDelay == 1) ? (_delay % kMaxDelay) : ((_remainingDelay - 1) * kMaxDelay)); }

#if DEBUG_EVENT_SCHEDULER
        const char *_file;
        uint32_t _line;
#endif

    private:
        ETSTimer _etsTimer;
        Callback _callback;
        int64_t _delay;
        RepeatType _repeat;
        PriorityType _priority;
        uint32_t _remainingDelay: 17;
        volatile uint32_t _callbackScheduled: 1;
        uint32_t ____free: 14;
    };

}

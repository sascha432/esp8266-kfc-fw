/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Event.h"

class SwitchPlugin;

namespace Event {

    class Scheduler;
    class Timer;
    class ManangedCallbackTimer;

    class CallbackTimer {
    public:

        CallbackTimer(Callback loopCallback, int64_t delay, RepeatType repeat, PriorityType priority);
    private:
        ~CallbackTimer();

    public:

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
        friend SwitchPlugin;

        void _initTimer();
        void _rearm();
        void _disarm();
        void _invokeCallback(CallbackTimerPtr timer);
        uint32_t _runtimeLimit(PriorityType priority) const;
        // release manager timer without removing the timer it manages
        void _releaseManagerTimer();

    public:
        inline int64_t __getRemainingDelayMillis() const {
            return (_remainingDelay == 0) ?  0 : ((_remainingDelay == 1) ? (_delay % kMaxDelay) : ((_remainingDelay - 1) * (int64_t)kMaxDelay));
        }

    public:
        ETSTimer _etsTimer;
        Callback _callback;
        Timer *_timer;
        int64_t _delay;
        RepeatType _repeat;
        PriorityType _priority;
        uint32_t _remainingDelay : 17;
        uint32_t _callbackScheduled : 1;
        uint32_t _maxDelayExceeded : 1;

#if DEBUG_EVENT_SCHEDULER
        uint32_t _line : 13;
        const char *_file;
        String __getFilePos();
#else
        String __getFilePos() { return emptyString; }
#endif
    };

    static constexpr auto CallbackTimerSize = sizeof(CallbackTimer);

    inline bool CallbackTimer::isArmed() const
    {
        //return (_etsTimer.timer_func == Scheduler::__TimerCallback);
        return (_etsTimer.timer_func != nullptr);
    }

    inline void CallbackTimer::_initTimer()
    {
        _rearm();
    }

    inline int64_t CallbackTimer::getInterval() const
    {
        return _delay;
    }

    inline uint32_t CallbackTimer::getShortInterval() const
    {
        EVENT_SCHEDULER_ASSERT(_delay <= std::numeric_limits<uint32_t>::max());
        return (uint32_t)_delay;
    }

    inline void CallbackTimer::setInterval(milliseconds interval)
    {
        rearm(interval);
    }

    inline bool CallbackTimer::updateInterval(milliseconds interval)
    {
        if (interval.count() != _delay) {
            rearm(interval);
            return true;
        }
        return false;
    }


}

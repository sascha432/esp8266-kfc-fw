/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Event.h"
#include "OSTimer.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

namespace Event {

    class Scheduler;
    class Timer;
    class ManangedCallbackTimer;

    class CallbackTimer {
    public:

        CallbackTimer(const char *name, Callback loopCallback, int64_t delay, RepeatType repeat, PriorityType priority);
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

        void _initTimer();
        void _rearm();
        void _disarm();
        void _invokeCallback(CallbackTimerPtr timer);
        uint32_t _runtimeLimit(PriorityType priority) const;
        // release manager timer without removing the timer it manages
        void _releaseManagerTimer();

    private:
        int64_t __getRemainingDelayMillis() const;

    public:
        ETSTimerEx _etsTimer;
        Callback _callback;
        Timer *_timer;
        int64_t _delay;
        #if SCHEDULER_HAVE_REMAINING_DELAY
            uint32_t _remainingDelay;
        #endif
        RepeatType _repeat;
        PriorityType _priority;
        #if SCHEDULER_HAVE_REMAINING_DELAY
            bool _maxDelayExceeded;
        #endif
        bool _callbackScheduled;

        #if DEBUG_EVENT_SCHEDULER
            uint32_t _line;
            const char *_file;
            String __getFilePos();
        #else
            String __getFilePos() { return emptyString; }
        #endif
    };

    static constexpr auto CallbackTimerSize = sizeof(CallbackTimer);

    inline bool CallbackTimer::isArmed() const
    {
        return _etsTimer.isRunning();
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
        return static_cast<uint32_t>(_delay);
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

    inline int64_t CallbackTimer::__getRemainingDelayMillis() const
    {
        #if SCHEDULER_HAVE_REMAINING_DELAY
            return (_remainingDelay == 0) ? 0 : ((_remainingDelay == 1) ? (_delay % kMaxDelay) : ((_remainingDelay - 1) * static_cast<int64_t>(kMaxDelay)));
        #else
            return _delay;
        #endif
    }

    inline void CallbackTimer::rearm(int64_t delay, RepeatType repeat, Callback callback)
    {
        _disarm();
        EVENT_SCHEDULER_ASSERT(delay >= kMinDelay);
        _delay = std::max<int64_t>(kMinDelay, delay);
        if (repeat._repeat != RepeatType::kPreset) {
            _repeat._repeat = repeat._repeat;
        }
        EVENT_SCHEDULER_ASSERT(_repeat._repeat != RepeatType::kPreset);
        if (callback) {
            _callback = callback;
        }
        __LDBG_printf("rearm=%.0f timer=%p repeat=%d cb=%p %s:%u", delay / 1.0, this, _repeat._repeat, lambda_target(_callback), __S(_file), _line);
        _rearm();
    }

    inline void CallbackTimer::rearm(milliseconds interval, RepeatType repeat, Callback callback)
    {
        rearm(interval.count(), repeat, callback);
    }

    inline void CallbackTimer::disarm()
    {
        __LDBG_printf("disarm timer=%p %s:%u", this, __S(_file), _line);
        _disarm();
    }

    inline void CallbackTimer::_disarm()
    {
        __LDBG_printf("timer=%p armed=%u cb=%p %s:%u", this, isArmed(), lambda_target(_callback), __S(_file), _line);
        if (isArmed()) {
            _etsTimer.disarm();
            #if SCHEDULER_HAVE_REMAINING_DELAY
                _remainingDelay = 0;
                _maxDelayExceeded = false;
            #endif
            _callbackScheduled = false;
        }
    }

}

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_disable.h>
#endif

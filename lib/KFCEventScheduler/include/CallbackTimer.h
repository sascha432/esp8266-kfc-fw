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

        void initTimer();

        bool active() const {
            return _etsTimer.timer_func != nullptr;
        }

        int64_t getDelay() const {
            return _delay;
        }

        // make sure the delay does not exceed UINT32_MAX
        uint32_t getDelayUInt32() const {
            __DBG_assert(_delay <= std::numeric_limits<uint32_t>::max());
            return (uint32_t)_delay;
        }

        void setCallback(Callback loopCallback) {
            _callback = loopCallback;
        }

        Callback getCallback() const {
            return _callback;
        }

        milliseconds getInterval() const {
            return milliseconds(_delay);
        }

        // update interval only if the new interval is different from the current one
        // and restart timer
        void setInterval(milliseconds interval) {
            rearm(interval);
        }

        // update invertal and restart timer. if the interval is the same, the timer
        // continue to run
        void updateInterval(milliseconds interval) {
            if (interval.count() != _delay) {
                rearm(interval);
            }
        }

        // set new interval and repeat type
        // the current timer is restarted
        void rearm(int64_t intervalMillis, RepeatType repeat = RepeatType(), Callback callback = nullptr, PriorityType priority = PriorityType::NONE);

        void rearm(milliseconds interval, RepeatType repeat = RepeatType(), Callback callback = nullptr, PriorityType priority = PriorityType::NONE);

        // called from Event::Timer
        // stops the timer until resumed by rearm()/setInterval()
        //
        // called inside timer callback (Event::TimerPtr)
        //
        // stops timer until resumed by rearm()...
        // when returning from the callback, stopped timers are removed
        void stop();

    private:
        friend Scheduler;

        // void _setScheduleCallback(bool enable);
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
        uint32_t _callbackScheduled: 1;
        uint32_t _disarmed: 1;
        uint32_t ____free: 13;
    };

}

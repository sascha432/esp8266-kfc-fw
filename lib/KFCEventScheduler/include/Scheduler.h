/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Event.h"

namespace Event {

    class Scheduler {
    public:
        Scheduler() : _runtimeLimit(250) {
        }

        void begin();
        void end();

        TimerPtr &add(int64_t delayMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        TimerPtr &add(milliseconds delay, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);

        // remove timer
        void remove(TimerPtr &timer);

        // returns number of scheduled timers
        size_t size() const;

    public:
        static void loop();
        static void ICACHE_RAM_ATTR __TimerCallback(void *arg);

    public:
        TimerVector &__getTimers() { return _timers; }
        void __list(bool debug = true);

    private:
        friend CallbackTimer;
        friend Timer;

        TimerVectorIterator _getTimer(CallbackTimer *timer);
        bool _hasTimer(CallbackTimer *timer) const;
        bool _removeTimer(CallbackTimer *timer);
        void _loop();
        void _cleanup();

    private:
        TimerVector _timers;
        uint32_t _runtimeLimit; // if a callback takes longer than this amount of time, the next callback will be delayed until the next loop function call
    };

}

#if !DISABLE_GLOBAL_EVENT_SCHEDULER
extern Event::Scheduler __Scheduler;
#endif

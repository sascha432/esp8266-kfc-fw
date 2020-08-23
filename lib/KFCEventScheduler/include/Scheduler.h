/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Event.h"

namespace Event {

    static constexpr uint32_t kMaxRuntimeLimit = 250; // milliseconds

    class Scheduler {
    public:
        Scheduler();

        void end();

        // there is no guarantee that a timer is called within its interval
        // if the main loop is blocked by another function or timer callback, all callbacks that are due can be delayed
        // if delayed for more than one interval, the callback is executed only once
        // timers with a higher priority get executed first and NORMAL and below might be skipped if the max. runtime (250ms)
        // exceeds a single loop. to ensure all high priority timers are executed in one loop, above NORMAL is limited to 15ms
        void add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        void add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);

        // remove timer
        void remove(CallbackTimerPtr timer);

        // returns number of scheduled timers
        size_t size() const;

    public:
        static void run(PriorityType runAbovePriority = PriorityType::NONE);
        static void ICACHE_RAM_ATTR __TimerCallback(void *arg);

    public:
        TimerVector &__getTimers() { return _timers; }
        void __list(bool debug = true);

    private:
        friend CallbackTimer;
        friend Timer;

        CallbackTimer *_add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);

        TimerVectorIterator _getTimer(CallbackTimerPtr timer);
        bool _hasTimer(CallbackTimerPtr timer) const;
        bool _removeTimer(CallbackTimerPtr timer);
        void _run(PriorityType runAbovePriority);
        void _cleanup();

    private:
        TimerVector _timers;
        volatile int8_t _hasEvent;
        uint32_t _runtimeLimit; // if a callback takes longer than this amount of time, the next callback will be delayed until the next loop function call
    };

}

#if !DISABLE_GLOBAL_EVENT_SCHEDULER
extern Event::Scheduler __Scheduler;
#endif

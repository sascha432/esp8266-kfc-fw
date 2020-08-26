/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Event.h"

namespace Event {


    static constexpr uint32_t kMaxRuntimeLimit = 250;                       // milliseconds, per main loop()
                                                                            // if a callback takes longer than this amount of time, the next callbacks will be delayed until the next loop function call
    static constexpr uint32_t kMaxRuntimePrioTimer = 5000;                  // microseconds, per callback priority TIMER
    static constexpr uint32_t kMaxRuntimePrioAboveNormal = 15000;           // microseconds, per callback priority >NORMAL

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
        static void run(PriorityType runAbovePriority);
        static void run();
        static void __TimerCallback(void *arg);

    public:
        TimerVector &__getTimers() { return _timers; }
        void __list(bool debug = true);

    private:
#if _MSC_VER
    public:
#endif
        friend CallbackTimer;
        friend Timer;

        CallbackTimer *_add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);

        TimerVectorIterator _getTimer(CallbackTimerPtr timer);
        bool _hasTimer(CallbackTimerPtr timer) const;
        bool _removeTimer(CallbackTimerPtr timer);
        void _run(PriorityType runAbovePriority);
        void _run();
        void _cleanup();
        void _sort();

    private:
        TimerVector _timers;
        size_t _size;
        volatile int8_t _hasEvent : 6;
        int8_t _addedFlag : 1;
        int8_t _removedFlag : 1;
#if DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR
        static constexpr uint32_t _runtimeLimit = kMaxRuntimeLimit;
#else
        uint32_t _runtimeLimit;
#endif
    };

}

#if !DISABLE_GLOBAL_EVENT_SCHEDULER
extern Event::Scheduler __Scheduler;
#endif

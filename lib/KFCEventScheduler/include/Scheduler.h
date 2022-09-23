/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "Event.h"

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

namespace Event {

    static constexpr uint32_t kMaxRuntimeLimit = 250;                       // milliseconds, per main loop(). hard limit
                                                                            // if a callback takes longer than this amount of time, the next callbacks will be delayed until the next loop function call
    static constexpr uint32_t kMaxRuntimePrioTimer = 5000;                  // microseconds, per callback priority TIMER. soft limit
    static constexpr uint32_t kMaxRuntimePrioAboveNormal = 15000;           // microseconds, per callback priority >NORMAL. soft limit

    class Scheduler {
    public:
        Scheduler();
        ~Scheduler();

        void end();

        // there is no guarantee that a timer is called within its interval until PriorityType is set to TIMER
        // lower priorities are executed in the main loop and can be blocked by the program or another timer
        // depending on the implementation, different priorities might be executed in different section of the program
        void add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        void add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);

        // add named timer in debug mode
        void add(const char *name, int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        void add(const char *name, milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);

        // remove timer
        void remove(CallbackTimerPtr timer);

        // returns number of scheduled timers
        size_t size() const;

    public:
        static void run(PriorityType runAbovePriority);
        static void run();
        static void __TimerCallback(CallbackTimerPtr timer);

    public:
        TimerVector &__getTimers() { return _timers; }
        void __list(bool debug = true);

    private:
#if _MSC_VER
    public:
#endif
        friend CallbackTimer;
        friend Timer;
        friend ManagedCallbackTimer;

        CallbackTimer *_add(const char *name, int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority = PriorityType::NORMAL);
        void _invokeCallback(CallbackTimerPtr timer, uint32_t runtimeLimit);

        bool _hasTimer(CallbackTimerPtr timer) const;
        bool _removeTimer(CallbackTimerPtr timer);

        // execute timers with a priority above without any time limiot
        void _run(PriorityType runAbovePriority);
        // execute high priority timers without time limit
        // once kMaxRuntimeLimit is reached, normal and below will be skipped until the next call of the function
        void _run();

        // remove null pointers
        void _cleanup();

        // remove null pointers and sort timers by priority
        void _sort();

        SemaphoreMutex &getLock();

    private:
        TimerVector _timers;
        int16_t _size;
        volatile PriorityType _hasEvent;
        bool _addedFlag;
        bool _removedFlag;
        // volatile bool _checkTimers;
        SemaphoreMutex _lock;

#if DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR
        static constexpr uint32_t _runtimeLimit = kMaxRuntimeLimit;
#else
        uint32_t _runtimeLimit;
#endif
    };

    static constexpr auto SchedulerSize = sizeof(Scheduler);

    inline void Scheduler::remove(CallbackTimerPtr timer)
    {
        _removeTimer(timer);
    }

    inline size_t Scheduler::size() const
    {
        return _timers.size() - std::count(_timers.begin(), _timers.end(), nullptr);
    }

    inline void Scheduler::add(int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority)
    {
        _add(PSTR("SchedulerTimer"), intervalMillis, repeat, callback, priority);
    }

    inline void Scheduler::add(milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
    {
        _add(PSTR("SchedulerTimer"), interval.count(), repeat, callback, priority);
    }

    inline void Scheduler::add(const char *name, int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority)
    {
        _add(name, intervalMillis, repeat, callback, priority);
    }

    inline void Scheduler::add(const char *name, milliseconds interval, RepeatType repeat, Callback callback, PriorityType priority)
    {
        _add(name, interval.count(), repeat, callback, priority);
    }

    inline SemaphoreMutex &Scheduler::getLock()
    {
        return _lock;
    }

}

#if !DISABLE_GLOBAL_EVENT_SCHEDULER
    extern Event::Scheduler __Scheduler;
#endif

#include "Scheduler.hpp"

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

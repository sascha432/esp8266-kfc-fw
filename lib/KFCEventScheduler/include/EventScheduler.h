/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

class EventTimer;
class EventScheduler;

extern EventScheduler Scheduler;

class EventScheduler {
public:
    enum Priority_t {
        PRIO_NONE = -1,
        PRIO_LOW = 0,
        PRIO_NORMAL,
        PRIO_HIGH,          // HIGH runs the timer callback directly inside the timer interrupt without any delay, but it has its limitations
    };

    enum Repeat_t {
        NO_CHANGE    = -2,
        UNLIMTIED    = -1,
        DONT         = 0,
        ONCE
    };

    struct ActiveCallbackTimer {
        EventTimer *timer;
        bool removed;
    };

    typedef EventTimer *EventTimerPtr;
    typedef std::function<void(EventTimerPtr timer)> Callback;
    typedef std::vector<ActiveCallbackTimer> CallbackVector;
    typedef std::vector<EventTimerPtr> TimerVector;
    typedef std::vector<EventTimerPtr>::iterator TimerIterator;

    EventScheduler() {
        _loopFunctionInstalled = false;
        _runtimeLimit = 250;
    }

    EventTimerPtr addTimer(int64_t delayMillis, int repeat, Callback callback, Callback removeCallback, Priority_t priority = PRIO_LOW);

    EventTimerPtr addTimer(int64_t delayMillis, int repeat, Callback callback, Priority_t priority = PRIO_LOW) {
        return addTimer(delayMillis, repeat, callback, nullptr, priority);
    }
    EventTimerPtr addTimer(int64_t delayMillis, bool repeat, Callback callback, Priority_t priority = PRIO_LOW) {
        return addTimer(delayMillis, (int)(repeat ? EventScheduler::UNLIMTIED : EventScheduler::DONT), callback, priority);
    }
    EventTimerPtr addTimer(int64_t delayMillis, bool repeat, Callback callback, Callback removeCallback, Priority_t priority = PRIO_LOW) {
        return addTimer (delayMillis, (int)(repeat ? EventScheduler::UNLIMTIED : EventScheduler::DONT), callback, removeCallback, priority);
    }
    bool hasTimer(EventTimerPtr timer);
    void removeTimer(EventTimerPtr timer, bool deleteObject = true);

    void loopFunc();
    void callTimer(EventTimerPtr timer);
    void installLoopFunc();
    void removeLoopFunc();

    // this stops the scheduler and removes all timers, but does not invoke any callbacks and does not free memory
    void terminate();

    static void _timerCallback(void *arg);

    void _list();

private:
    CallbackVector _callbacks;
    TimerVector _timers;
    bool _loopFunctionInstalled;
    int _runtimeLimit; // if a callback takes longer than this amount of time, the next callback will be delayed until the next loop function call
};

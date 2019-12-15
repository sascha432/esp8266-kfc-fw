/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_EVENT_SCHEDULER
#define DEBUG_EVENT_SCHEDULER 0
#endif

class EventTimer;
class EventScheduler;

extern EventScheduler Scheduler;

class EventScheduler {
public:
    typedef enum {
        PRIO_NONE =         -1,
        PRIO_LOW =          0,
        PRIO_NORMAL,                // below HIGH, the timer callback is executed in the main loop and might be considerably delayed
        PRIO_HIGH,                  // HIGH runs the timer callback directly inside the timer interrupt/task without any delay
    } Priority_t;

    typedef enum {
        NO_CHANGE =         -2,
        UNLIMTIED =         -1,
        DONT =              0,
        ONCE =              1,
    } Repeat_t;

    typedef EventTimer *TimerPtr;
    typedef std::function<void(TimerPtr timer)> Callback;
    typedef std::vector<TimerPtr> TimerVector;
    typedef std::vector<TimerPtr>::iterator TimerIterator;

    EventScheduler() : _runtimeLimit(250) {
    }

    void begin();
    void end();

    // repeat as int = number of repeats
    void addTimer(TimerPtr *timerPtr, int64_t delayMillis, int repeat, Callback callback, Priority_t priority = PRIO_LOW);

    // repeat as bool = unlimited or none
    void addTimer(TimerPtr *timerPtr, int64_t delayMillis, bool repeat, Callback callback, Priority_t priority = PRIO_LOW) {
        addTimer(timerPtr, delayMillis, (int)(repeat ? EventScheduler::UNLIMTIED : EventScheduler::DONT), callback, priority);
    }

    void addTimer(int64_t delayMillis, int repeat, Callback callback, Priority_t priority = PRIO_LOW) {
        addTimer(nullptr, delayMillis, repeat, callback, priority);
    }
    void addTimer(int64_t delayMillis, bool repeat, Callback callback, Priority_t priority = PRIO_LOW) {
        addTimer(nullptr, delayMillis, repeat, callback, priority);
    }

    bool hasTimer(TimerPtr timer);
    void removeTimer(TimerPtr timer);

    static void loop();
    void callTimer(TimerPtr timer);

    static void _timerCallback(void *arg);

private:
#if DEBUG_EVENT_SCHEDULER
    void _list();
#endif

    void _removeTimer(TimerPtr timer);
    void _loop();

private:
    TimerVector _timers;
    int _runtimeLimit; // if a callback takes longer than this amount of time, the next callback will be delayed until the next loop function call
};

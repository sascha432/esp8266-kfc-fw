/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "EventScheduler.h"

class EventTimer {
public:
    typedef enum {
        DELAY_NO_CHANGE =   -1,
    } ChangeOptionsDelayEnum_t;

    static const int32_t minDelay = 5;
    static const int32_t maxDelay = 0x68D7A3;

    EventTimer(EventScheduler::TimerPtr *timerPtr, EventScheduler::Callback loopCallback, int64_t delay, int repeat, EventScheduler::Priority_t priority);
    ~EventTimer();

    void _installTimer();

    inline operator bool() const __attribute__((always_inline)) {
        return !!_timer;
    }

    inline bool active() const __attribute__((always_inline)) {
        return !!_timer;
    }

    inline int getRepeat() const {
        return _repeat;
    }

    inline int64_t getDelay() const {
        return _delay;
    }

    inline int getCallCounter() const {
        return _callCounter;
    }
    inline void setCallCounter(int callCounter) {
        _callCounter = callCounter;
    }

    inline void setCallback(EventScheduler::Callback loopCallback) {
        _loopCallback = loopCallback;
    }

    void setPriority(EventScheduler::Priority_t priority = EventScheduler::PRIO_LOW);
    void changeOptions(int delay, int repeat = EventScheduler::NO_CHANGE, EventScheduler::Priority_t priority = EventScheduler::PRIO_NONE);

    inline void changeOptions(int delay, bool repeat, EventScheduler::Priority_t priority = EventScheduler::PRIO_NONE) {
        changeOptions(delay, (int)(repeat ? EventScheduler::UNLIMTIED : EventScheduler::DONT), priority);
    }

    inline void rearm(int delay, bool repeat) {
        _updateInterval(delay, repeat);
    }

    void detach();
    void invokeCallback();

private:
    friend class EventScheduler;

    void _setScheduleCallback(bool enable);
    void _updateInterval(int delay, bool repeat);
    bool _maxRepeat() const;

    os_timer_t *_timer;
    EventScheduler::Callback _loopCallback;
    EventScheduler::TimerPtr *_timerPtr;
    int64_t _delay;
    int64_t _remainingDelay;
    int32_t _repeat;
    int32_t _callCounter;
    EventScheduler::Priority_t _priority;
    bool _callbackScheduled;
};

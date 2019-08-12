/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "EventScheduler.h"

#define MIN_DELAY   5
#define MAX_DELAY   0x68D7A3

class EventTimer {
public:
    // static const int MIN_DELAY = 5;
    // static const int MAX_DELAY = 0x68D7A3;

    EventTimer(EventScheduler::Callback loopCallback, EventScheduler::Callback removeCallback, int64_t delay, int repeat, EventScheduler::Priority_t priority);
    EventTimer(EventScheduler::Callback loopCallback, int64_t delay, int repeat, EventScheduler::Priority_t priority) :
        EventTimer(loopCallback, nullptr, delay, repeat, priority)
    {
    }
    ~EventTimer();

    void _installTimer();

    bool active() const {
        return _timer;
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

    void setCallback(EventScheduler::Callback loopCallback) {
        _loopCallback = loopCallback;
    }
    void setRemoveCallback(EventScheduler::Callback removeCallback) {
        _removeCallback = removeCallback;
    }

    void setPriority(EventScheduler::Priority_t priority = EventScheduler::PRIO_LOW);
    void changeOptions(int delay, int repeat = EventScheduler::NO_CHANGE, EventScheduler::Priority_t priority = EventScheduler::PRIO_NONE);
    void changeOptions(int delay, bool repeat, EventScheduler::Priority_t priority = EventScheduler::PRIO_NONE) {
        changeOptions(delay, (int)(repeat ? EventScheduler::UNLIMTIED : EventScheduler::DONT), priority);
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
    EventScheduler::Callback _removeCallback;
    int64_t _delay;
    int64_t _remainingDelay;
    int32_t _repeat;
    int32_t _callCounter;
    EventScheduler::Priority_t _priority;
    bool _callbackScheduled;
public:
    String _getTimerSource();

#if DEBUG
    String __source;
    int __line;
    String __function;
    unsigned long _start;
#endif
};

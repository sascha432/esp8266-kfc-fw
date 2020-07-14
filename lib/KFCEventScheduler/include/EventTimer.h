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

    static const int32_t MinDelay = 5;
    static const int32_t MaxDelay = 0x68D7A3; // 6870947ms / 6870.947 seconds

    EventTimer(EventScheduler::Callback loopCallback, int64_t delay, EventScheduler::RepeatType repeat, EventScheduler::Priority_t priority);
    ~EventTimer();

    // force inline to avoid ICACHE_RAM_ATTR

    inline void initTimer() __attribute__((always_inline)) {
        _rearmEtsTimer();
    }

    inline bool active() const __attribute__((always_inline)) {
        return _etsTimer.timer_func != nullptr;
    }

    inline int64_t getDelay() const __attribute__((always_inline))  {
        return _delay;
    }

    // make sure the delay does not exceed UINT32_MAX
    inline uint32_t getDelayUInt32() const __attribute__((always_inline))  {
        return (uint32_t)_delay;
    }

    inline void setCallback(EventScheduler::Callback loopCallback) __attribute__((always_inline)) {
        _loopCallback = loopCallback;
    }

    inline EventScheduler::Callback getCallback() __attribute__((always_inline)) {
        return _loopCallback;
    }

    inline void setPriority(EventScheduler::Priority_t priority) __attribute__((always_inline)) {
        _priority = priority;
    }

    // cancels timer and updates options
    void rearm(int64_t delay, EventScheduler::RepeatUpdateType repeat = EventScheduler::RepeatUpdateType());

    // stops the timer
    void detach();

private:
    friend EventScheduler;
    friend EventScheduler::Timer;
    friend void at_mode_list_ets_timers(Print &output);

    // void _setScheduleCallback(bool enable);
    void _rearmEtsTimer();
    void _invokeCallback();
    void _remove();

    ETSTimer _etsTimer;
    EventScheduler::Callback _loopCallback;
    int64_t _delay;
    int64_t _remainingDelay;
    EventScheduler::Priority_t _priority;
    EventScheduler::RepeatType _repeat;
    bool _callbackScheduled;
    bool _disarmed;
};

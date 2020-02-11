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

    static const int32_t MIN_DELAY = 5;
    static const int32_t MAX_DELAY = 0x68D7A3; // 6870947ms / 6870.947 seconds

    EventTimer(EventScheduler::Callback loopCallback, int64_t delay, EventScheduler::RepeatType repeat, EventScheduler::Priority_t priority);
    ~EventTimer();

    operator bool() const {
        __debugbreak_and_panic_printf_P(PSTR("operator bool was used to check if a timer is active, use active()"));
        return false;
    }

    inline void initTimer() __attribute__((always_inline)) {
        _rearmEtsTimer();
    }

    inline bool active() const __attribute__((always_inline)) {
        return _etsTimer.timer_func != nullptr;
    }

    inline int64_t getDelay() const __attribute__((always_inline))  {
        return _delay;
    }

    // inline EventScheduler::RepeatType getRepeat() const {
    //     return _repeat;
    // }

    void setCallback(EventScheduler::Callback loopCallback);
    void setPriority(EventScheduler::Priority_t priority = EventScheduler::PRIO_LOW);

    // cancels timer and updates options
    void rearm(int64_t delay, EventScheduler::RepeatUpdateType repeat = EventScheduler::RepeatUpdateType());

    // stops the timer
    void detach();

private:
    friend EventScheduler;
    friend EventScheduler::Timer;

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

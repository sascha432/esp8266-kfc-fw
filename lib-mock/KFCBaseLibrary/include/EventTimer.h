/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include "EventScheduler.h"
#include <thread>

class EventTimer {
public:
    typedef enum {
        DELAY_NO_CHANGE = -1,
    } ChangeOptionsDelayEnum_t;

    static const int32_t MinDelay = 5;
    static const int32_t MaxDelay = 0x68D7A3; // 6870947ms / 6870.947 seconds

    EventTimer(EventScheduler::Callback loopCallback, int64_t delay, EventScheduler::RepeatType repeat, EventScheduler::Priority_t priority);
    ~EventTimer();

    inline void initTimer() {
        _disarmed = false;
    }

    inline bool active() const  {
        return _disarmed == false;
    }

    inline int64_t getDelay() const {
        return _delay;
    }

    // inline EventScheduler::RepeatType getRepeat() const {
    //     return _repeat;
    // }

    void setCallback(EventScheduler::Callback loopCallback);
    EventScheduler::Callback getCallback();
    void setPriority(EventScheduler::Priority_t priority = EventScheduler::PRIO_LOW);

    // cancels timer and updates options
    void rearm(int64_t delay, EventScheduler::RepeatUpdateType repeat = EventScheduler::RepeatUpdateType());

    // stops the timer
    void detach();

private:
    friend EventScheduler;
    friend EventScheduler::Timer;

    // void _setScheduleCallback(bool enable);
    void _invokeCallback();
    void _remove();

    EventScheduler::Callback _loopCallback;
    int64_t _delay;
    int64_t _remainingDelay;
    EventScheduler::Priority_t _priority;
    EventScheduler::RepeatType _repeat;
    bool _callbackScheduled;
    bool _disarmed;

    void _loop();
    std::thread _thread;
};

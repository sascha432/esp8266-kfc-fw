/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>

#ifndef DEBUG_EVENT_SCHEDULER
#define DEBUG_EVENT_SCHEDULER                   0
#endif

class EventTimer;
class EventScheduler;

extern EventScheduler Scheduler;

class EventScheduler {
public:
    class RepeatType {
    public:
        static const int UNLIMITED = -1;
        static const int YES = 1;
        static const int NO = 0;

        // true for UNLIMITED or false for no repeat
        RepeatType(bool repeat = false) : _counter(0), _maxRepeat(repeat ? UNLIMITED : NO) {
        }
        // number of repeats, YES for calling it once, UNLIMITED or 0
        RepeatType(int repeat) : _counter(0), _maxRepeat(repeat) {
        }

    protected:
        friend EventScheduler;
        friend EventTimer;

        inline bool hasRepeat() const {
            return (_maxRepeat == UNLIMITED) || (_counter <= _maxRepeat);
        }

        int _counter;
        int _maxRepeat;
    };

    class RepeatUpdateType : public RepeatType {
    public:
        using RepeatType::RepeatType;

        static const int NO_CHANGE = -2;

        // keep repeat
        RepeatUpdateType() : RepeatType(NO_CHANGE) {
        }

    private:
        friend EventTimer;

        inline bool isUpdateRequired() {
            return _maxRepeat != NO_CHANGE;
        }
    };

    typedef enum {
        PRIO_NONE =         -1,
        PRIO_LOW =          0,
        PRIO_NORMAL,                // below HIGH, the timer callback is executed in the main loop and might be considerably delayed
        PRIO_HIGH,                  // HIGH runs the timer callback directly inside the timer interrupt/task without any delay
    } Priority_t;

    typedef std::shared_ptr<EventTimer> TimerPtr;
    typedef std::function<void(TimerPtr timer)> Callback;
    typedef std::function<void(EventTimer *)> DeleterCallback;
    typedef std::vector<TimerPtr> TimerVector;
    typedef std::vector<TimerPtr>::iterator TimerIterator;

public:
    class Timer {
    public:
        Timer();
        ~Timer();

        Timer(const Timer &timer) = delete;
        Timer &operator=(const Timer &timer) = delete;

        Timer(Timer &&timer);
        Timer &operator=(Timer &&timer);

        void add(int64_t delayMillis, RepeatType repeat, Callback callback, Priority_t priority = PRIO_LOW, DeleterCallback deleter = nullptr);
        bool remove();

        inline bool active() const {
            return _timer != nullptr;
        }

        EventTimer *operator->() const;

    private:
        EventTimer *_timer;
    };

public:
    EventScheduler() : _runtimeLimit(250) {
    }

    void begin();
    void end();

    EventTimer *addTimer(int64_t delayMillis, RepeatType repeat, Callback callback, Priority_t priority = PRIO_LOW, DeleterCallback deleter = nullptr);

    // checks if "timer" exists and is running
    bool hasTimer(EventTimer *timer) const;

    // remove timer and return true if it has been removed
    bool removeTimer(EventTimer *timer);

    // returns number of scheduled timers
    size_t size() const {
        return _timers.size();
    }

public:
    static void loop();
    static void _timerCallback(void *arg);

private:
#if DEBUG_EVENT_SCHEDULER
    void _list();
#endif
    friend EventTimer;
    friend void at_mode_list_ets_timers(Print &output);

    void _removeTimer(EventTimer *timer);
    void _loop();

private:
    TimerVector _timers;
    int _runtimeLimit; // if a callback takes longer than this amount of time, the next callback will be delayed until the next loop function call
};

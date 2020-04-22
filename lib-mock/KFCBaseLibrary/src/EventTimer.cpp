/**
  Author: sascha_lammers@gmx.de
*/

#include <LoopFunctions.h>
#include "EventTimer.h"

EventTimer::EventTimer(EventScheduler::Callback loopCallback, int64_t delay, EventScheduler::RepeatType repeat, EventScheduler::Priority_t priority) : _disarmed(false), _thread([this]() { _loop(); })
{
    _loopCallback = loopCallback;
    _callbackScheduled = false;
    _delay = delay;
    _remainingDelay = 0;
    _repeat = repeat;
    _priority = priority;
    _disarmed = true;
}

EventTimer::~EventTimer()
{
    _disarmed = true;
    _thread.join();
}

void EventTimer::_remove()
{
    _disarmed = true;
}

void EventTimer::detach()
{
    _disarmed = true;
}

static void _no_deleter(void *) {
}

void EventTimer::_loop()
{
    EventScheduler::TimerPtr timerPtr(this, _no_deleter);
    while (_disarmed) {
        Sleep(1);
    }
    for (;;) {
        auto num = _delay / 10;
        while (num--) {
            Sleep(10);
            if (_disarmed) {
                break;
            }
        }
        if (_disarmed) {
            break;
        }
        if (_priority == EventScheduler::PRIO_HIGH) {
            _loopCallback(timerPtr);
        }
        else {
            LoopFunctions::callOnce([this, timerPtr]() {
                _loopCallback(timerPtr);
            });
        }
        if (_disarmed) {
            break;
        }
        _repeat._counter++;
        if (!_repeat.hasRepeat()) {
            break;
        }
    }
    _disarmed = true;
}

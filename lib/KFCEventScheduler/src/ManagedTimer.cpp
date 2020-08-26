/**
  Author: sascha_lammers@gmx.de
*/

#include "Event.h"
#include "Timer.h"
#include "ManagedTimer.h"
#include "Scheduler.h"

#if DEBUG_EVENT_SCHEDULER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

using namespace Event;

ManangedCallbackTimer::ManangedCallbackTimer() : _callbackTimer(nullptr)
{
}

ManangedCallbackTimer::ManangedCallbackTimer(CallbackTimerPtr callbackTimer) : _callbackTimer(callbackTimer)
{
}

ManangedCallbackTimer::ManangedCallbackTimer(CallbackTimerPtr callbackTimer, TimerPtr timer) : _callbackTimer(callbackTimer)
{
    __LDBG_assert(callbackTimer != nullptr);
    __LDBG_printf("callback_timer=%p timer=%p _timer=%p", callbackTimer, timer, callbackTimer->_timer);
    if (callbackTimer) {
        if (callbackTimer->_timer) {
            __LDBG_printf("removing managed timer=%p", callbackTimer->_timer);
            callbackTimer->_timer->_managedTimer.clear();
        }
        _callbackTimer->_timer = timer;
    }
}

ManangedCallbackTimer &ManangedCallbackTimer::operator=(ManangedCallbackTimer &&move) noexcept
{
    __LDBG_assert(move._callbackTimer != nullptr);
    if (_callbackTimer) {
        if (_callbackTimer->_timer) {
            __LDBG_printf("removing managed timer=%p", _callbackTimer->_timer);
            _callbackTimer->_timer->_managedTimer.clear();
        }
    }
    __LDBG_assert(move._callbackTimer != nullptr);
    _callbackTimer = std::exchange(move._callbackTimer, nullptr);
    return *this;
}

ManangedCallbackTimer::~ManangedCallbackTimer()
{
    __LDBG_printf("this=%p callback_timer=%p timer=%p", this, _callbackTimer, _callbackTimer ? _callbackTimer->_timer : nullptr);

    if (_callbackTimer) {
        if (_callbackTimer->_timer) {
            _callbackTimer->_timer->_managedTimer._callbackTimer = nullptr;
        }
        __LDBG_printf("remove callback_timer=%p", _callbackTimer);
        __Scheduler.remove(_callbackTimer);
    }
}

ManangedCallbackTimer::operator bool() const
{
    return _callbackTimer != nullptr;
}

CallbackTimerPtr ManangedCallbackTimer::operator->() const noexcept
{
    return _callbackTimer;
}

CallbackTimerPtr ManangedCallbackTimer::get() const noexcept
{
    return _callbackTimer;
}

void ManangedCallbackTimer::clear()
{
    if (_callbackTimer) {
        if (_callbackTimer->_timer) {
            __LDBG_printf("this=%p timer=%p managed=%p", this, _callbackTimer, _callbackTimer->_timer->_managedTimer._callbackTimer);
            _callbackTimer->_timer = nullptr;
        }
        _callbackTimer = nullptr;
    }
}


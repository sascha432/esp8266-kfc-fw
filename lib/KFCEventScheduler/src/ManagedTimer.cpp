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
    EVENT_SCHEDULER_ASSERT(callbackTimer != nullptr);
    __LDBG_printf("callback_timer=%p timer=%p _timer=%p", callbackTimer, timer, callbackTimer->_timer);
    if (callbackTimer) {
        callbackTimer->_releaseManagerTimer();
        _callbackTimer->_timer = timer;
    }
}

ManangedCallbackTimer &ManangedCallbackTimer::operator=(ManangedCallbackTimer &&move) noexcept
{
    EVENT_SCHEDULER_ASSERT(move._callbackTimer != nullptr);
    if (_callbackTimer) {
        _callbackTimer->_releaseManagerTimer();
    }
    EVENT_SCHEDULER_ASSERT(move._callbackTimer != nullptr);
    _callbackTimer = std::exchange(move._callbackTimer, nullptr);
    return *this;
}

ManangedCallbackTimer::~ManangedCallbackTimer()
{
    __LDBG_printf("this=%p callback_timer=%p timer=%p", this, _callbackTimer, _callbackTimer ? _callbackTimer->_timer : nullptr);
    remove();
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

bool ManangedCallbackTimer::remove()
{
    __LDBG_printf("_callbackTimer=%p _timer=%p _managedTimer=%d _managedTimer._callbackTimer=%p",
        _callbackTimer,
        _callbackTimer ? _callbackTimer->_timer : nullptr,
        _callbackTimer && _callbackTimer->_timer ? (int)(bool)_callbackTimer->_timer->_managedTimer : -1,
        _callbackTimer && _callbackTimer->_timer ? _callbackTimer->_timer->_managedTimer._callbackTimer : nullptr
    );
    if (!_callbackTimer || !_callbackTimer->_timer) {
        return false;
    }
    auto result = __Scheduler._removeTimer(_callbackTimer);
    // remove timer after calling _removeTimer
    // the managed timer has been cleared during removal
    _callbackTimer = nullptr;
    return result;
}

void ManangedCallbackTimer::clear()
{
    __LDBG_printf("_callbackTimer=%p _timer=%p _managedTimer=%p this=%p _managedTimer._callbackTimer=%p",
        _callbackTimer,
        _callbackTimer ? _callbackTimer->_timer : nullptr,
        _callbackTimer && _callbackTimer->_timer ? &_callbackTimer->_timer->_managedTimer : nullptr,
        this,
        _callbackTimer && _callbackTimer->_timer ? _callbackTimer->_timer->_managedTimer._callbackTimer : nullptr
    );
    if (_callbackTimer && _callbackTimer->_timer) {
        __DBG_assert_printf(_callbackTimer->_timer->_managedTimer._callbackTimer == nullptr || _callbackTimer->_timer->_managedTimer._callbackTimer == _callbackTimer, "_managedTimer._callbackTimer=%p points to a different _callbackTimer=%p", _callbackTimer->_timer->_managedTimer._callbackTimer, _callbackTimer);
    }
    if (_callbackTimer) {
        _callbackTimer->_timer = nullptr;
        _callbackTimer = nullptr;
    }
}

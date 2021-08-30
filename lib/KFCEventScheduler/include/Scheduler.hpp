/**
  Author: sascha_lammers@gmx.de
*/

#include "Scheduler.h"
#include "Timer.h"

#if DEBUG_EVENT_SCHEDULER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

namespace Event {

    inline Scheduler::Scheduler() :
        _size(0),
        _hasEvent(PriorityType::NONE),
        _addedFlag(false),
        _removedFlag(false),
        _checkTimers(false)
        #if DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR == 0
            , _runtimeLimit(kMaxRuntimeLimit)
        #endif
    {
    }

    inline Scheduler::~Scheduler()
    {
        end();
    }

    #if !DEBUG_EVENT_SCHEDULER

        inline void Scheduler::__list(bool debug)
        {
        }

    #endif


    inline void Timer::add(const char *name, int64_t intervalMillis, RepeatType repeat, Callback callback, PriorityType priority)
    {
        if (*this) {
            _managedTimer->rearm(intervalMillis, repeat, callback);
        }
        else {
            auto callbackTimer = __Scheduler._add(name, intervalMillis, repeat, callback, priority);
            _managedTimer = ManangedCallbackTimer(callbackTimer, this);
        }
    }

}

namespace Event {

    inline void CallbackTimer::_releaseManagerTimer()
    {
        __LDBG_printf("_timer=%p", _timer);
        if (_timer) {
            _timer->_managedTimer.clear();
        }
    }

    inline void CallbackTimer::_invokeCallback(CallbackTimerPtr timer)
    {
        __Scheduler._invokeCallback(timer, _runtimeLimit(timer->_priority));
    }

    inline uint32_t CallbackTimer::_runtimeLimit(PriorityType priority) const
    {
        if (priority == PriorityType::TIMER) {
            return kMaxRuntimePrioTimer;
        }
        else if (priority > PriorityType::NORMAL) {
            return kMaxRuntimePrioAboveNormal;
        }
        return 0;
    }

}

namespace Event {

    inline void Scheduler::_cleanup()
    {
        _timers.erase(std::remove(_timers.begin(), _timers.end(), nullptr), _timers.end());
        _timers.shrink_to_fit();
        _size = static_cast<int16_t>(_timers.size());
        _removedFlag = false;
    }

    inline bool Scheduler::_hasTimer(CallbackTimerPtr timer) const
    {
        return (timer == nullptr) ? false : (std::find(_timers.begin(), _timers.end(), timer) != _timers.end());
    }

    inline void Scheduler::run(PriorityType runAbovePriority)
    {
        __Scheduler._run(runAbovePriority);
    }

    inline void Scheduler::run()
    {
        __Scheduler._run();
    }

    inline void Scheduler::_run(PriorityType runAbovePriority)
    {
        if (_hasEvent > runAbovePriority) {
            for(int i = 0; i < _size; i++) {
                auto timer = _timers[i];
                if (timer) {
                    if (timer->_priority > runAbovePriority && timer->_callbackScheduled) {
                        timer->_callbackScheduled = false;
                        timer->_invokeCallback(timer);
                    }
                }
            }
        }
    }

}

namespace Event {

    inline void ManangedCallbackTimer::clear()
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

    inline bool ManangedCallbackTimer::remove()
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

}

namespace Event {

    inline RepeatType::RepeatType(uint32_t repeat) : _repeat(repeat)
    {
        EVENT_SCHEDULER_ASSERT(_repeat != kPreset);
    }

}

#if DEBUG_EVENT_SCHEDULER
#    include <debug_helper_disable.h>
#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

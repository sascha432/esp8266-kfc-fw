/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include "Event.h"
#include "CallbackTimer.h"

namespace Event {

    class Timer;

    class ManangedCallbackTimer {
    public:
        using TimerPtr = Timer *;

        ManangedCallbackTimer(const ManangedCallbackTimer &) = delete;
        ManangedCallbackTimer &operator=(const ManangedCallbackTimer &) = delete;

        ManangedCallbackTimer();
        ManangedCallbackTimer(CallbackTimerPtr callbackTimer, TimerPtr timer);

        ManangedCallbackTimer &operator=(ManangedCallbackTimer &&move) noexcept;

        ManangedCallbackTimer(CallbackTimerPtr callbackTimer);

        ~ManangedCallbackTimer();

        operator bool() const;

        CallbackTimerPtr operator->() const noexcept;
        CallbackTimerPtr get() const noexcept;
        void clear();

    private:
        // friend Timer;

        // ManangedCallbackTimer(ManangedCallbackTimer &&move) noexcept : _callbackTimer(move._callbackTimer) {}

        CallbackTimerPtr _callbackTimer;
    };

}

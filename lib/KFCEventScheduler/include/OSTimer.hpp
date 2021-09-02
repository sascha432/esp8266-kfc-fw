/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#if ESP32

#include "OSTimer_esp32.hpp"

#elif ESP8266 || _MSC_VER

#include "OSTimer_esp8266.hpp"

#endif


inline __attribute__((__always_inline__)) OSTimer::
#if DEBUG_OSTIMER
    OSTimer(const char *name) : _etsTimer(name)
#else
    OSTimer()
#endif
{
}

inline OSTimer::~OSTimer()
{
    MUTEX_LOCK_BLOCK(_lock) {
        _etsTimer.done();
    }
}

inline __attribute__((__always_inline__)) bool OSTimer::isRunning() const
{
    return _etsTimer.isRunning();
}

inline __attribute__((__always_inline__)) OSTimer::operator bool() const
{
    return _etsTimer.isRunning();
}

inline void OSTimer::startTimer(Event::OSTimerDelayType delay, bool repeat, bool isMillis)
{
    #if DEBUG_OSTIMER
        __DBG_printf("start timer name=%s delay=%u repeat=%u millis=%u", _etsTimer._name, delay, repeat, isMillis);
    #endif
    MUTEX_LOCK_BLOCK(_lock) {
        _etsTimer.create(OSTimer::_OSTimerCallback, this);
        delay = std::clamp<Event::OSTimerDelayType>(delay, Event::kMinDelay, Event::kMaxDelay);
        _etsTimer.arm(delay, repeat, isMillis);
    }
}

inline void OSTimer::detach()
{
    MUTEX_LOCK_BLOCK(_lock) {
        if (_etsTimer.isRunning()) {
            _etsTimer.disarm();
        }
    }
}

inline bool OSTimer::lock()
{
    #if DEBUG_OSTIMER_FIND
        if (!_etsTimer.find()) {
            return false;
        }
    #endif
    if (_etsTimer.isLocked()) {
        return false;
    }
    _etsTimer.lock();
    return true;
}

inline void OSTimer::unlock(OSTimer &timer, uint32_t timeoutMicros)
{
    #if DEBUG_OSTIMER_FIND
        if (!ETSTimerEx::find(timer)) {
            return;
        }
    #endif
    if (!timer._etsTimer.isLocked()) {
        return;
    }

    // wait until the lock has been released
    uint32_t start = micros();
    uint32_t timeout;
    while(isLocked(timer) && (timeout = (micros() - start)) < timeoutMicros) {
        optimistic_yield(timeoutMicros - timeout);
    }

    #if DEBUG_OSTIMER_FIND
        if (!ETSTimerEx::find(timer)) {
            #if DEBUG_OSTIMER
                ::printf(PSTR("%p:unlock() timer vanished\n"), &timer);
            #endif
            return;
        }
    #endif
    if (!timer._etsTimer.isLocked()) {
        return;
    }
    timer._etsTimer.unlock();
}

inline void OSTimer::unlock(OSTimer &timer)
{
    #if DEBUG_OSTIMER_FIND
        if (!ETSTimerEx::find(timer)) {
            return;
        }
    #endif
    timer._etsTimer.unlock();
}

inline bool OSTimer::isLocked(OSTimer &timer)
{
    return
        #if DEBUG_OSTIMER_FIND
            ETSTimerEx::find(&timer._etsTimer) &&
        #endif
        timer._etsTimer.isLocked();
}

inline SemaphoreMutex &OSTimer::getLock()
{
    return _lock;
}


#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

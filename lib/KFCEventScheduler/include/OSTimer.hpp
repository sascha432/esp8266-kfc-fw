/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#ifndef OSTIMER_INLINE
#   define OSTIMER_INLINE inline
#endif

#if ESP32

#include "OSTimer_esp32.hpp"

#elif ESP8266 || _MSC_VER

#include "OSTimer_esp8266.hpp"

#endif

OSTIMER_INLINE OSTimer::
#if DEBUG_OSTIMER
    OSTimer(const char *name) : _etsTimer(name)
#else
    OSTimer()
#endif
{
}

OSTIMER_INLINE OSTimer::~OSTimer()
{
    PORT_MUX_LOCK_ISR_BLOCK(_mux) {
        _etsTimer.done();
    }
}

OSTIMER_INLINE bool OSTimer::isRunning() const
{
    return _etsTimer.isRunning();
}

OSTIMER_INLINE OSTimer::operator bool() const
{
    return _etsTimer.isRunning();
}

OSTIMER_INLINE void OSTimer::startTimer(Event::OSTimerDelayType delay, bool repeat, bool isMillis)
{
    #if DEBUG_OSTIMER
        __DBG_printf("start timer name=%s delay=%u repeat=%u millis=%u", _etsTimer._name, delay, repeat, isMillis);
    #endif
    PORT_MUX_LOCK_ISR_BLOCK(_mux) {
        delay = std::clamp<Event::OSTimerDelayType>(delay, Event::kMinDelay, Event::kMaxDelay);
        _etsTimer.arm(delay, repeat, isMillis);
    }
}

OSTIMER_INLINE void OSTimer::detach()
{
    PORT_MUX_LOCK_ISR_BLOCK(_mux) {
        if (_etsTimer.isRunning()) {
            _etsTimer.disarm();
        }
    }
}

OSTIMER_INLINE bool OSTimer::lock()
{
    PORT_MUX_LOCK_ISR_BLOCK(_mux) {
        #if DEBUG_OSTIMER_FIND
            if (!_etsTimer.find()) {
                return false;
            }
        #endif
        if (_etsTimer.isLocked()) {
            return false;
        }
        _etsTimer.lock();
    }
    return true;
}

OSTIMER_INLINE void OSTimer::unlock(OSTimer &timer, uint32_t timeoutMicros)
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

    PORT_MUX_LOCK_ISR_BLOCK(timer.getMux()) {
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
}

OSTIMER_INLINE void OSTimer::unlock(OSTimer &timer)
{
    PORT_MUX_LOCK_ISR_BLOCK(timer.getMux()) {
        #if DEBUG_OSTIMER_FIND
            if (!ETSTimerEx::find(timer)) {
                return;
            }
        #endif
        timer._etsTimer.unlock();
    }
}

OSTIMER_INLINE bool OSTimer::isLocked(OSTimer &timer)
{
    portMuxLockISR mLock(timer.getMux());
    return
        #if DEBUG_OSTIMER_FIND
            ETSTimerEx::find(&timer._etsTimer) &&
        #endif
        timer._etsTimer.isLocked();
}

OSTIMER_INLINE portMuxType &OSTimer::getMux()
{
    return _mux;
}


#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

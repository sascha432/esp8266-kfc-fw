/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

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
    detach();
    _etsTimer.done();
}

OSTIMER_INLINE bool OSTimer::isRunning() const
{
    return _etsTimer.isRunning();
}

OSTIMER_INLINE OSTimer::operator bool() const
{
    return _etsTimer.isRunning();
}

OSTIMER_INLINE void OSTimer::startTimer(int32_t delay, bool repeat, bool isMillis)
{
    #if DEBUG_OSTIMER
        __DBG_printf("start timer name=%s delay=%u repeat=%u millis=%u", _etsTimer._name, delay, repeat, isMillis);
    #endif
    #if ESP32
        delay = std::clamp<int32_t>(delay, 1, std::numeric_limits<decltype(delay)>::max());
        if (_etsTimer._timer) {
            _etsTimer.disarm();
            _etsTimer.done();
        }
        _etsTimer.create(&_EtsTimerCallback, this);
        _etsTimer.arm(delay, repeat, isMillis);
    #else
        delay = std::clamp<int32_t>(delay, Event::kMinDelay, Event::kMaxDelay);
        _etsTimer.create(&_EtsTimerCallback, this);
        ets_timer_arm_new(&_etsTimer, delay, repeat, isMillis);
    #endif
}

OSTIMER_INLINE void OSTimer::detach()
{
    portMuxLock mLock(_mux);
    if (_etsTimer.isRunning()) {
        _etsTimer.disarm();
    }
}

OSTIMER_INLINE bool OSTimer::lock()
{
    InterruptLock intrLock;
    portMuxLock mLock(_mux);
    if (!_etsTimer.find()) {
        return false;
    }
    if (_etsTimer.isLocked()) {
        return false;
    }
    _etsTimer.lock();
    return true;
}

OSTIMER_INLINE void OSTimer::unlock(OSTimer &timer, uint32_t timeoutMicros)
{
    if (!ETSTimerEx::find(timer)) {
        return;
    }
    if (!timer._etsTimer.isLocked()) {
        return;
    }

    // wait until the lock has been released
    uint32_t start = micros();
    uint32_t timeout;
    while(isLocked(timer) && (timeout = (micros() - start)) < timeoutMicros) {
        optimistic_yield(timeoutMicros - timeout);
    }

    InterruptLock intrLock;
    portMuxLock mLock(_mux);
    if (!ETSTimerEx::find(timer)) {
        #if DEBUG_OSTIMER
            ::printf(PSTR("%p:unlock() timer vanished\n"), &timer);
        #endif
        return;
    }
    if (!timer._etsTimer.isLocked()) {
        return;
    }
    timer._etsTimer.unlock();
}

OSTIMER_INLINE void OSTimer::unlock(OSTimer &timer)
{
    InterruptLock intrLock;
    portMuxLock mLock(_mux);
    if (!ETSTimerEx::find(timer)) {
        return;
    }
    timer._etsTimer.unlock();
}

OSTIMER_INLINE bool OSTimer::isLocked(OSTimer &timer)
{
    InterruptLock intrLock;
    portMuxLock mLock(_mux);
    return ETSTimerEx::find(&timer._etsTimer) && timer._etsTimer.isLocked();
}

/**
  Author: sascha_lammers@gmx.de
*/

#include "OSTimer.h"
#include "Event.h"

#ifndef OSTIMER_INLINE
#define OSTIMER_INLINE inline
#endif

#if DEBUG_OSTIMER

static void __DBG_printEtsTimer(ETSTimer &timer) {
    ::printf(PSTR("timer=%p func=%p arg=%p period=%u next=%p\n"), &timer, timer.timer_func, timer.timer_arg, timer.timer_period, timer.timer_next);
}

#endif


#if DEBUG_OSTIMER
    OSTIMER_INLINE ETSTimerEx::ETSTimerEx(const char *name) : _magic(kMagic), _name(name)
#else
    OSTIMER_INLINE ETSTimerEx::ETSTimerEx()
#endif
{
    clear();
}

OSTIMER_INLINE ETSTimerEx::~ETSTimerEx()
{
    #if DEBUG_OSTIMER
        if (_magic != kMagic) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::~ETSTimerEx(): name=%s _magic=%08x<>%08x", __S(_name), _magic, kMagic);
        }
        if (isRunning()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::~ETSTimerEx(): name=%s isRunning()", __S(_name));
        }
        if (!isDone()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::~ETSTimerEx(). name=%s !isDone()", __S(_name));
        }
        _magic = 0;
    #endif
    clear();
}

OSTIMER_INLINE bool ETSTimerEx::isRunning() const
{
    return timer_period != 0;
}

OSTIMER_INLINE bool ETSTimerEx::isDone() const
{
    return timer_period == 0 && timer_func == nullptr && timer_arg == nullptr && (uint32_t)timer_next == 0xffffffffU;
}

OSTIMER_INLINE bool ETSTimerEx::isLocked() const
{
    return timer_func == reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback);
}

OSTIMER_INLINE void ETSTimerEx::lock()
{
    #if DEBUG_OSTIMER
        if (!isRunning()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::lock() name=%s", __S(_name));
        }
    #endif
    timer_func = reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback);
}

OSTIMER_INLINE void ETSTimerEx::unlock()
{
    #if DEBUG_OSTIMER
        if (!isLocked()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::unlock() name=%s", __S(_name));
        }
    #endif
    timer_func = reinterpret_cast<ETSTimerFunc *>(OSTimer::_EtsTimerCallback);
}

OSTIMER_INLINE void ETSTimerEx::disarm()
{
    #if DEBUG_OSTIMER
        if (!find()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::disarm() name=%s", __S(_name));
        }
    #endif
    ets_timer_disarm(this);
}

OSTIMER_INLINE void ETSTimerEx::done()
{
    #if DEBUG_OSTIMER
        if (isRunning()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::done() name=%s", __S(_name));
        }
    #endif
    ets_timer_done(this);
}

OSTIMER_INLINE void ETSTimerEx::clear()
{
    timer_func = nullptr;
    timer_arg = nullptr;
    timer_period = 0;
    timer_next = (ETSTimer *)0xfffffffffU;
}

OSTIMER_INLINE ETSTimer *ETSTimerEx::find()
{
    return find(this);
}

OSTIMER_INLINE ETSTimer *ETSTimerEx::find(ETSTimer *timer)
{
    ETSTimer *cur = timer_list;
    while(cur) {
        if (cur == timer) {
            #if DEBUG_OSTIMER
                auto &t = *reinterpret_cast<ETSTimerEx *>(timer);
                if (t._magic != kMagic) {
                    __DBG_printEtsTimer(t);
                    __DBG_panic("ETSTimerEx::~ETSTimerEx(): name=%s _magic=%08x<>%08x", __S(t._name), t._magic, kMagic);
                }
            #endif
            return cur;
        }
        cur = cur->timer_next;
    }
    return nullptr;
}

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

OSTIMER_INLINE void OSTimer::startTimer(int32_t delay, bool repeat, bool millis)
{
    delay = std::clamp<int32_t>(delay, Event::kMinDelay, Event::kMaxDelay);
    ets_timer_disarm(&_etsTimer);
    ets_timer_setfn(&_etsTimer, &_EtsTimerCallback, this);
    ets_timer_arm_new(&_etsTimer, delay, repeat, millis);
}

OSTIMER_INLINE void OSTimer::detach()
{
    if (_etsTimer.isRunning()) {
        _etsTimer.disarm();
    }
}

OSTIMER_INLINE bool OSTimer::lock()
{
    InterruptLock intrLock;
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
    if (!ETSTimerEx::find(timer)) {
        return;
    }
    timer._etsTimer.unlock();
}

OSTIMER_INLINE bool OSTimer::isLocked(OSTimer &timer)
{
    InterruptLock intrLock;
    return ETSTimerEx::find(&timer._etsTimer) && timer._etsTimer.isLocked();
}

/**
  Author: sascha_lammers@gmx.de
*/

#include "OSTimer.h"
#include "Event.h"

#ifndef OSTIMER_INLINE
#define OSTIMER_INLINE inline
#endif

#if DEBUG_OSTIMER

void __DBG_printEtsTimer(ETSTimer &timer) {
    ::printf(PSTR("func=%p arg=%p period=%u next=%p\n"), timer.timer_func, timer.timer_arg, timer.timer_period, timer.timer_next);
}

#else

#define __DBG_printEtsTimer(...)

#endif

OSTIMER_INLINE ETSTimerEx::ETSTimerEx() : _magic(kMagic)
{
    clear();
}

OSTIMER_INLINE ETSTimerEx::~ETSTimerEx()
{
    #if DEBUG
        if (_magic != kMagic) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::~ETSTimerEx(): _magic=%08x<>%08x", _magic, kMagic);
        }
        if (isRunning()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::~ETSTimerEx(): isRunning()");
        }
        if (!isDone()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::~ETSTimerEx(). !isDone()");
        }
    #endif
    clear();
    _magic = 0;
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
    return timer_func == OSTIMER_LOCKED_PTR;
}

OSTIMER_INLINE void ETSTimerEx::lock()
{
    #if DEBUG
        if (!isRunning()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::lock()");
        }
    #endif
    timer_func = OSTIMER_LOCKED_PTR;
}

OSTIMER_INLINE void ETSTimerEx::unlock() {
    #if DEBUG
        if (!isLocked()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::unlock()");
        }
    #endif
    timer_func = reinterpret_cast<ETSTimerFunc *>(OSTimer::_EtsTimerCallback);
}

OSTIMER_INLINE void ETSTimerEx::disarm()
{
    #if DEBUG
        if (!find()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::disarm()");
        }
    #endif
    ets_timer_disarm(this);
}

OSTIMER_INLINE void ETSTimerEx::done()
{
    #if DEBUG
        if (isRunning()) {
            __DBG_printEtsTimer(*this);
            __DBG_panic("ETSTimerEx::done()");
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
            return cur;
        }
        cur = cur->timer_next;
    }
    return nullptr;
}

#if OSTIMER_USE_MAGIC
    #define OSTIMER_VALIDATE_MAGIC(timer, ...) \
        if ((timer)._magic != kMagic) { \
            ::printf(PSTR("%p: invalid magic\n"), &(timer)); \
        } else { __VA_ARGS__; }
#else
    define OSTIMER_VALIDATE_MAGIC(...)
#endif

OSTIMER_INLINE OSTimer::
#if DEBUG_OSTIMER
    OSTimer(const char *name) :
#else
    OSTimer() :
#endif
        #if OSTIMER_USE_MAGIC
            _magic(kMagic), _locked(false),
        #endif
        #if DEBUG_OSTIMER
            _name(name)
        #endif
{
}

OSTIMER_INLINE OSTimer::~OSTimer()
{
    detach();
    _etsTimer.done();
    #if OSTIMER_USE_MAGIC
        _magic = 0;
    #endif
}

OSTIMER_INLINE bool OSTimer::isRunning() const
{
    return _etsTimer.isRunning();
    // return (_etsTimer.timer_arg == this);
}

OSTIMER_INLINE OSTimer::operator bool() const
{
    return _etsTimer.isRunning();
}

OSTIMER_INLINE void OSTimer::startTimer(int32_t delay, bool repeat, bool millis)
{
    delay = std::clamp<int32_t>(delay, Event::kMinDelay, Event::kMaxDelay);
    #if DEBUG_OSTIMER
        _delay = delay;
        if (millis) {
            _delay *= 1000;
        }
    #endif
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

    OSTIMER_VALIDATE_MAGIC(*this, (!_locked ? (_locked = true) : false));
    return true;
}

OSTIMER_INLINE void OSTimer::unlock(OSTimer &timer, uint32_t timeoutMicros)
{
    if (!ETSTimerEx::find(timer)) {
        return;
    }
    OSTIMER_VALIDATE_MAGIC(timer);

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
    OSTIMER_VALIDATE_MAGIC(timer, timer._locked = false);

    if (!timer._etsTimer.isLocked()) {
        return;
    }
    timer._etsTimer.unlock();
}

OSTIMER_INLINE void OSTimer::unlock(OSTimer &timer)
{
    InterruptLock intrLock;
    OSTIMER_VALIDATE_MAGIC(timer, timer._locked = false);

    if (!ETSTimerEx::find(timer)) {
        return;
    }
    timer._etsTimer.unlock();
}

OSTIMER_INLINE bool OSTimer::isLocked(OSTimer &timer)
{
    InterruptLock intrLock;
    OSTIMER_VALIDATE_MAGIC(timer);
    return ETSTimerEx::find(&timer._etsTimer) && timer._etsTimer.isLocked();
}

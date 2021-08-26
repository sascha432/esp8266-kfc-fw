/**
  Author: sascha_lammers@gmx.de
*/

// timer implementation for ESP8266

#pragma once

#if ESP8266 || _MSC_VER

#include "OSTimer.h"
#include "Event.h"

#if DEBUG_OSTIMER

static OSTIMER_INLINE void __DBG_printEtsTimer(ETSTimer &timer, const char *msg = "") {
    __DBG_printf("%stimer=%p func=%p arg=%p period=%u next=%p", msg, &timer, timer.timer_func, timer.timer_arg, timer.timer_period, timer.timer_next);
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
            disarm();
            __DBG_printf_E("ETSTimerEx::~ETSTimerEx(): name=%s isRunning()", __S(_name));
        }
        if (!isDone()) {
            __DBG_printf_E("ETSTimerEx::~ETSTimerEx(). name=%s !isDone()", __S(_name));
            done();
        }
        _magic = 0;
    #endif
    clear();
}

OSTIMER_INLINE void ETSTimerEx::create(ETSTimerFunc *callback, void *arg)
{
    ets_timer_disarm(this);
    ets_timer_setfn(this, callback, arg);
}

OSTIMER_INLINE void ETSTimerEx::arm(int32_t delay, bool repeat, bool millis)
{
    ets_timer_arm_new(this, delay, repeat, millis);
}

OSTIMER_INLINE bool ETSTimerEx::isRunning() const
{
    return timer_period != 0;
}

OSTIMER_INLINE bool ETSTimerEx::isDone() const
{
    return timer_period == 0 && timer_func == nullptr && timer_arg == nullptr && reinterpret_cast<uint32_t>(timer_next) == 0xffffffffU;
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
    if (isRunning()) {
        ets_timer_disarm(this);
    }
    ets_timer_done(this);
}

OSTIMER_INLINE void ETSTimerEx::clear()
{
    timer_func = nullptr;
    timer_arg = nullptr;
    timer_period = 0;
    timer_next = reinterpret_cast<ETSTimer *>(0xfffffffffU);
}

OSTIMER_INLINE ETSTimerEx *ETSTimerEx::find()
{
    return find(this);
}

OSTIMER_INLINE ETSTimerEx *ETSTimerEx::find(ETSTimerEx *timer)
{
    ETSTimer *cur = timer_list;
    while(cur) {
        if (cur == timer) {
            #if DEBUG_OSTIMER
                auto &t = *timer;
                if (t._magic != kMagic) {
                    __DBG_printEtsTimer(t);
                    __DBG_panic("ETSTimerEx::~ETSTimerEx(): name=%s _magic=%08x<>%08x", __S(t._name), t._magic, kMagic);
                }
            #endif
            return reinterpret_cast<ETSTimerEx *>(cur);
        }
        cur = cur->timer_next;
    }
    return nullptr;
}

OSTIMER_INLINE void ETSTimerEx::end()
{
    ETSTimer *cur = timer_list;
    while(cur) {
        auto next = cur->timer_next;
        if (cur->timer_func == reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback) || cur->timer_func == reinterpret_cast<ETSTimerFunc *>(OSTimer::_EtsTimerCallback)) {
            ets_timer_disarm(cur);
            ets_timer_done(cur);
        }
        cur = next;
    }
}

OSTIMER_INLINE void ICACHE_FLASH_ATTR ETSTimerEx::_EtsTimerLockedCallback(void *arg)
{
}

#endif

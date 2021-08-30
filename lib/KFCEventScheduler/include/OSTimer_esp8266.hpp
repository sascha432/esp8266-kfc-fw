/**
  Author: sascha_lammers@gmx.de
*/

// timer implementation for ESP8266

#pragma once

#include <Arduino_compat.h>

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#if ESP8266 || _MSC_VER

#include "OSTimer.h"

#if DEBUG_OSTIMER
    OSTIMER_INLINE ETSTimerEx::ETSTimerEx(const char *name) : _magic(kMagic), _name(name)
#else
    OSTIMER_INLINE ETSTimerEx::ETSTimerEx()
#endif
{
    clear();
    #if DEBUG_OSTIMER
        __DBG_printEtsTimer(this, PSTR("ctor"));
    #endif
}

OSTIMER_INLINE ETSTimerEx::~ETSTimerEx()
{
    #if DEBUG_OSTIMER
        __DBG_printEtsTimer(this, PSTR("dtor"));
        if (_magic != kMagic) {
            __DBG_printEtsTimer(this, PrintString(F("dtor(): _magic=%08x<>%08x"), _magic, kMagic));
        }
        if (isRunning()) {
            __DBG_printEtsTimer_E(this, PSTR("dtor(): isRunning()==TRUE"));
        }
        if (isDone()) {
            __DBG_printEtsTimer_E(this, PSTR("dtor(): isDone()==TRUE"));
        }
        if (isLocked()) {
            __DBG_printEtsTimer_E(this, PSTR("dtor(): isLocked()==TRUE"));
        }
        if (!isDone()) {
            // calls disarm if running
            done();
        }
        _magic = 0;
    #else
        if (!isDone()) {
            // calls disarm if running
            done();
        }
    #endif
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
    return (timer_period == 0 && timer_func == nullptr && timer_arg == nullptr);// || reinterpret_cast<uint32_t>(timer_next) == 0xffffffffU;
}

OSTIMER_INLINE bool ETSTimerEx::isLocked() const
{
    return timer_func == reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback);
}

OSTIMER_INLINE void ETSTimerEx::lock()
{
    #if DEBUG_OSTIMER
        if (!find()) {
            __DBG_printEtsTimer_E(this, PSTR("lock(): find()==FALSE"));
        }
        if (!isRunning()) {
            __DBG_printEtsTimer_E(this, PSTR("lock(): isRunning()==FALSE"));
        }
        if (isLocked()) {
            __DBG_printEtsTimer_E(this, PSTR("lock(): isLocked()==TRUE"));
        }
    #endif
    timer_func = reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback);
}

OSTIMER_INLINE void ETSTimerEx::unlock()
{
    #if DEBUG_OSTIMER
        if (!find()) {
            __DBG_printEtsTimer_E(this, PSTR("unlock(): find()==FALSE"));
        }
        if (!isRunning()) {
            __DBG_printEtsTimer_E(this, PSTR("unlock(): isRunning()==FALSE"));
        }
        if (!isLocked()) {
            __DBG_printEtsTimer_E(this, PSTR("unlock(): isLocked()==FALSE"));
        }
    #endif
    timer_func = reinterpret_cast<ETSTimerFunc *>(OSTimer::_EtsTimerCallback);
}

OSTIMER_INLINE void ETSTimerEx::disarm()
{
    #if DEBUG_OSTIMER
        if (!find()) {
            __DBG_printEtsTimer_E(this, PSTR("disarm(): find()==FALSE"));
        }
    #endif
    ets_timer_disarm(this);
}

OSTIMER_INLINE void ETSTimerEx::done()
{
    #if DEBUG_OSTIMER
        if (!find()) {
            __DBG_printEtsTimer_E(this, PSTR("done(): find()==FALSE"));
        }
        if (isRunning()) {
            __DBG_printEtsTimer_E(this, PSTR("done(): isRunning()==TRUE"));
        }
        if (isDone()) {
            __DBG_printEtsTimer_E(this, PSTR("done(): isDone()==TRUE"));
        }
        if (isLocked()) {
            __DBG_printEtsTimer_E(this, PSTR("done(): isLocked()==TRUE"));
        }
    #endif
    if (isRunning()) {
        ets_timer_disarm(this);
    }
    ets_timer_done(this);
    // clear();
}

OSTIMER_INLINE void ETSTimerEx::clear()
{
    timer_func = nullptr;
    timer_arg = nullptr;
    timer_period = 0;
    // timer_next = reinterpret_cast<ETSTimer *>(0xfffffffffU);
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
                    __DBG_printEtsTimer_E(timer, PrintString(F("find(): _magic=%08x<>%08x"), t._magic, kMagic));
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
            //ets_timer_done(cur);
        }
        cur = next;
    }
}

OSTIMER_INLINE void ICACHE_FLASH_ATTR ETSTimerEx::_EtsTimerLockedCallback(void *arg)
{
}

#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

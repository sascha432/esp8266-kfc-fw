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
    inline ETSTimerEx::ETSTimerEx(const char *name) :
        _name(is_HEAP_P(name) ? strdup_P(name) : const_cast<char *>(name)),
        _called(0),
        _calledWhileLocked(0),
        _magic(kMagic)
#else
    inline ETSTimerEx::ETSTimerEx()
#endif
{
    clear();
    timer_next = nullptr;
    #if DEBUG_OSTIMER
        __DBG_printEtsTimer(*this, PSTR("ctor"));
    #endif
}

inline ETSTimerEx::~ETSTimerEx()
{
    #if DEBUG_OSTIMER
        __DBG_printEtsTimer(*this, PSTR("dtor"));
        if (_magic != kMagic) {
            __DBG_printEtsTimer(*this, PrintString(F("dtor(): _magic=%08x<>%08x"), _magic, kMagic));
        }
        if (isRunning()) {
            __DBG_printEtsTimer_E(*this, PSTR("dtor(): isRunning()==TRUE"));
        }
        if (isDone()) {
            __DBG_printEtsTimer_E(*this, PSTR("dtor(): isDone()==TRUE"));
        }
        if (isLocked()) {
            __DBG_printEtsTimer_E(*this, PSTR("dtor(): isLocked()==TRUE"));
        }
        if (!isDone()) {
            // calls disarm if running
            done();
        }
        _magic = 0;
        if (is_HEAP_P(_name)) {
            free(_name);
        }
    #else
        if (!isDone()) {
            // calls disarm if running
            done();
        }
    #endif
}

inline void ETSTimerEx::create(ETSTimerFunc *callback, void *arg)
{
    if (!isNew()) {
        ets_timer_disarm(this);
    }
    ets_timer_setfn(this, callback, arg);
}

inline void ETSTimerEx::arm(int32_t delay, bool repeat, bool millis)
{
    ets_timer_arm_new(this, delay, repeat, millis);
}

inline bool ETSTimerEx::isNew() const
{
    return (timer_next != reinterpret_cast<void *>(0xffffffffU)) && (timer_arg == reinterpret_cast<void *>(kUnusedMagic));
}

inline bool ETSTimerEx::isRunning() const
{
    return timer_period != 0;
}

inline bool ETSTimerEx::isDone() const
{
    return (timer_next != reinterpret_cast<void *>(0xffffffffU)) && ((timer_arg == reinterpret_cast<void *>(kUnusedMagic)) || (timer_period == 0 && timer_func == nullptr && timer_arg == nullptr));
}

inline bool ETSTimerEx::isLocked() const
{
    return timer_func == reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback);
}

inline void ETSTimerEx::lock()
{
    #if DEBUG_OSTIMER_FIND
        if (!find()) {
            __DBG_printEtsTimer_E(*this, PSTR("lock(): find()==FALSE"));
        }
    #endif
    #if DEBUG_OSTIMER
        if (!isRunning()) {
            __DBG_printEtsTimer_E(*this, PSTR("lock(): isRunning()==FALSE"));
        }
        if (isLocked()) {
            __DBG_printEtsTimer_E(*this, PSTR("lock(): isLocked()==TRUE"));
        }
        if (isDone()) {
            __DBG_printEtsTimer_E(*this, PSTR("lock(): isDone()==TRUE"));
        }
        else if (isNew()) {
            __DBG_printEtsTimer_E(*this, PSTR("lock(): isNew()==TRUE"));
        }
    #endif
    timer_func = reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback);
}

inline void ETSTimerEx::unlock()
{
    #if DEBUG_OSTIMER_FIND
        if (!find()) {
            __DBG_printEtsTimer_E(*this, PSTR("unlock(): find()==FALSE"));
        }
    #endif
    #if DEBUG_OSTIMER
        if (!isRunning()) {
            __DBG_printEtsTimer_E(*this, PSTR("unlock(): isRunning()==FALSE"));
        }
        if (!isLocked()) {
            __DBG_printEtsTimer_E(*this, PSTR("unlock(): isLocked()==FALSE"));
        }
        if (isDone()) {
            __DBG_printEtsTimer_E(*this, PSTR("unlock(): isDone()==TRUE"));
        }
        else if (isNew()) {
            __DBG_printEtsTimer_E(*this, PSTR("unlock(): isNew()==TRUE"));
        }
    #endif
    timer_func = reinterpret_cast<ETSTimerFunc *>(OSTimer::_OSTimerCallback);
}

inline void ETSTimerEx::disarm()
{
    #if DEBUG_OSTIMER_FIND
        if (!find()) {
            __DBG_printEtsTimer_E(*this, PSTR("disarm(): find()==FALSE"));
        }
    #endif
    #if DEBUG_OSTIMER
        if (isDone()) {
            __DBG_printEtsTimer_E(*this, PSTR("disarm(): isDone()==TRUE"));
        }
        else if (isNew()) {
            __DBG_printEtsTimer_E(*this, PSTR("disarm(): isNew()==TRUE"));
        }
    #endif
    ets_timer_disarm(this);
}

inline void ETSTimerEx::done()
{
    if (isNew()) {
        return;
    }
    #if DEBUG_OSTIMER_FIND
        if (!find()) {
            __DBG_printEtsTimer_E(*this, PSTR("done(): find()==FALSE"));
        }
    #endif
    #if DEBUG_OSTIMER
        if (isRunning()) {
            __DBG_printEtsTimer_E(*this, PSTR("done(): isRunning()==TRUE"));
        }
        if (isDone()) {
            __DBG_printEtsTimer_E(*this, PSTR("done(): isDone()==TRUE"));
        }
        if (isLocked()) {
            __DBG_printEtsTimer_E(*this, PSTR("done(): isLocked()==TRUE"));
        }
    #endif
    ets_timer_disarm(this);
    ets_timer_done(this);
    clear();
}

inline void ETSTimerEx::clear()
{
    timer_func = nullptr;
    timer_arg = reinterpret_cast<ETSTimer *>(kUnusedMagic);
    timer_period = 0;
    timer_next = nullptr;
}

#if DEBUG_OSTIMER_FIND

inline ETSTimerEx *ETSTimerEx::find()
{
    return find(this);
}

inline ETSTimerEx *ETSTimerEx::find(ETSTimerEx *timer)
{
    ETSTimer *cur = timer_list;
    while(cur) {
        if (cur == timer) {
            #if DEBUG_OSTIMER
                auto &t = *timer;
                if (t._magic != kMagic) {
                    __DBG_printEtsTimer_E(*timer, PrintString(F("find(): _magic=%08x<>%08x"), t._magic, kMagic));
                }
            #endif
            return reinterpret_cast<ETSTimerEx *>(cur);
        }
        cur = cur->timer_next;
    }
    if (is_HEAP_P(timer) && ((timer->timer_next == reinterpret_cast<void *>(0xffffffffU)) || (timer->timer_arg == reinterpret_cast<void *>(kUnusedMagic)))) {
        return timer;
    }
    return nullptr;
}

#endif

inline void ETSTimerEx::end()
{
    ETSTimer *cur = timer_list;
    while(cur) {
        auto next = cur->timer_next;
        if (cur->timer_func == reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback) || cur->timer_func == reinterpret_cast<ETSTimerFunc *>(OSTimer::_OSTimerCallback)) {
            ets_timer_disarm(cur);
            //ets_timer_done(cur);
        }
        cur = next;
    }
}

inline void ICACHE_FLASH_ATTR ETSTimerEx::_EtsTimerLockedCallback(OSTimer *timer)
{
    timer->_etsTimer._calledWhileLocked++;
}

#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif


/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "EventTimer.h"
#include "OSTimer.h"

#if DEBUG_OSTIMER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

OSTimer::OSTimer() : _etsTimer()
{
}

OSTimer::~OSTimer()
{
    debug_printf_P(PSTR("_etsTimer.timer_arg=%p\n"), _etsTimer.timer_arg);
    detach();
}

static void ICACHE_RAM_ATTR _callback(void *arg)
{
    reinterpret_cast<OSTimer *>(arg)->run();
}

void OSTimer::startTimer(uint32_t delay, bool repeat)
{
    debug_printf_P(PSTR("delay=%u repeat=%u\n"), delay, repeat);
    if (delay < EventTimer::MIN_DELAY) {
        __debugbreak_and_panic_printf_P(PSTR("delay %u < %u is not supported\n"), delay, EventTimer::MIN_DELAY);
        delay = EventTimer::MIN_DELAY;
    }
    else if (delay > EventTimer::MAX_DELAY) {
        __debugbreak_and_panic_printf_P(PSTR("delay %u > %u is not supported\n"), delay, EventTimer::MAX_DELAY);
        delay = EventTimer::MAX_DELAY;
    }
    ets_timer_disarm(&_etsTimer);
    ets_timer_setfn(&_etsTimer, reinterpret_cast<ETSTimerFunc *>(_callback), reinterpret_cast<void *>(this));
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void OSTimer::detach()
{
    debug_printf_P(PSTR("_etsTimer.timer_arg=%p\n"), _etsTimer.timer_arg);
    if (_etsTimer.timer_arg == this) {
        ets_timer_disarm(&_etsTimer);
        ets_timer_done(&_etsTimer);
        _etsTimer.timer_arg = nullptr;
    }
}


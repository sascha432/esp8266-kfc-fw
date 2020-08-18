
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

OSTimer::OSTimer() : _etsTimer({})
{
}

OSTimer::~OSTimer()
{
    __SLDBG_printf("_etsTimer.timer_arg=%p", _etsTimer.timer_arg);
    detach();
}

static void ICACHE_RAM_ATTR _callback(void *arg)
{
    reinterpret_cast<OSTimer *>(arg)->run();
}

void OSTimer::startTimer(uint32_t delay, bool repeat)
{
    __SLDBG_printf("delay=%u repeat=%u", delay, repeat);
    if (delay < EventTimer::kMinDelay) {
        __SLDBG_panic("delay %u < %u is not supported", delay, EventTimer::kMinDelay);
        delay = EventTimer::kMinDelay;
    }
    else if (delay > EventTimer::kMaxDelay) {
        __SLDBG_panic("delay %u > %u is not supported", delay, EventTimer::kMaxDelay);
        delay = EventTimer::kMaxDelay;
    }
    ets_timer_disarm(&_etsTimer);
    ets_timer_setfn(&_etsTimer, reinterpret_cast<ETSTimerFunc *>(_callback), reinterpret_cast<void *>(this));
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void OSTimer::detach()
{
    __SLDBG_printf("_etsTimer.timer_arg=%p", _etsTimer.timer_arg);
    if (_etsTimer.timer_arg == this) {
        ets_timer_disarm(&_etsTimer);
        ets_timer_done(&_etsTimer);
        _etsTimer.timer_arg = nullptr;
    }
}


/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "Event.h"
#include "OSTimer.h"

#if DEBUG_OSTIMER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern "C" void ICACHE_RAM_ATTR _ostimer_callback(void *arg)
{
    reinterpret_cast<OSTimer *>(arg)->run();
}

extern "C" void ICACHE_RAM_ATTR _ostimer_detach(ETSTimer *etsTimer, void *timerArg)
{
    if (etsTimer->timer_arg == timerArg) {
        ets_timer_disarm(etsTimer);
        ets_timer_done(etsTimer);
        etsTimer->timer_arg = nullptr;
    }
}

void OSTimer::startTimer(int32_t delay, bool repeat)
{
    delay = std::max(1, std::min(Event::kMaxDelay, delay));
    ets_timer_disarm(&_etsTimer);
    ets_timer_setfn(&_etsTimer, reinterpret_cast<ETSTimerFunc *>(_ostimer_callback), reinterpret_cast<void *>(this));
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void OSTimer::detach()
{
    _ostimer_detach(&_etsTimer, this);
}

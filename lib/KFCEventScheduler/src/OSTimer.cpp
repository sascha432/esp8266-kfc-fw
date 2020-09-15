
/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include <stl_ext/algorithm.h>
#include "Event.h"
#include "OSTimer.h"

#if DEBUG_OSTIMER
#include <debug_helper_enable.h>
#else
#include <debug_helper_disable.h>
#endif

extern "C" void ICACHE_FLASH_ATTR _etstimer_callback(void *arg)
{
    reinterpret_cast<OSTimer *>(arg)->run();
}

void OSTimer::startTimer(int32_t delay, bool repeat)
{
    delay = std::clamp_signed(delay, Event::kMinDelay, Event::kMaxDelay);
    ets_timer_disarm(&_etsTimer);
    ets_timer_setfn(&_etsTimer, reinterpret_cast<ETSTimerFunc *>(_etstimer_callback), this);
    ets_timer_arm_new(&_etsTimer, delay, repeat, true);
}

void OSTimer::detach()
{
    if (_etsTimer.timer_arg == this) {
        ets_timer_disarm(&_etsTimer);
        ets_timer_done(&_etsTimer);
        _etsTimer.timer_arg = nullptr;
    }
}

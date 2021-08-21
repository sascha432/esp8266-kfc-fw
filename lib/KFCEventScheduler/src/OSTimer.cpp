
/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "OSTimer.h"

#if ESP32
std::list<ETSTimerEx *> ETSTimerEx::_timers;
#endif

portMuxType OSTimer::_mux;

void ICACHE_FLASH_ATTR OSTimer::_EtsTimerCallback(void *arg)
{
    auto &timer = *reinterpret_cast<OSTimer *>(arg);
    // #if DEBUG_OSTIMER
    //     __DBG_printf("timer run name=%s running=%u locked=%u", timer._etsTimer._name, timer.isRunning(), timer.isLocked(timer));
    // #endif
    {
        portMuxLockISR lock(_mux);
        if (!timer.lock()) {
            return;
        }
    }
    timer.run();
    portMuxLockISR lock(_mux);
    OSTimer::unlock(timer);
}

#if DEBUG_OSTIMER
#   define OSTIMER_INLINE
#   include "OSTimer.hpp"
#endif

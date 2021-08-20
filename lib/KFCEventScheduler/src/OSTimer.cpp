
/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "OSTimer.h"

void ICACHE_FLASH_ATTR OSTimer::_EtsTimerCallback(void *arg)
{
    auto &timer = *reinterpret_cast<OSTimer *>(arg);
    // #if DEBUG_OSTIMER
    //     __DBG_printf("timer run name=%s running=%u locked=%u", timer._etsTimer._name, timer.isRunning(), timer.isLocked(timer));
    // #endif
    if (!timer.lock()) {
        return;
    }
    timer.run();
    OSTimer::unlock(timer);
}

#if DEBUG_OSTIMER
#   define OSTIMER_INLINE
#   include "OSTimer.hpp"
#endif

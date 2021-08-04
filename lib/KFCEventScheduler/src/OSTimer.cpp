
/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "OSTimer.h"

void ICACHE_FLASH_ATTR _EtsTimerLockedCallback(void *arg) {
}

void ICACHE_FLASH_ATTR OSTimer::_EtsTimerCallback(void *arg)
{
    auto &timer = *reinterpret_cast<OSTimer *>(arg);
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

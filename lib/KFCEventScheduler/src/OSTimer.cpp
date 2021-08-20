
/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "OSTimer.h"

#if ESP32
#undef __DBG_panic
#define __DBG_panic __DBG_printf
#endif

void ICACHE_FLASH_ATTR OSTimer::_EtsTimerCallback(void *arg)
{
    auto &timer = *reinterpret_cast<OSTimer *>(arg);
    if (!timer.lock()) {
        return;
    }
    __DBG_printf("timer run name=%s", timer._etsTimer._name);
    timer.run();
    OSTimer::unlock(timer);
}

#if DEBUG_OSTIMER
#   define OSTIMER_INLINE
#   include "OSTimer.hpp"
#endif

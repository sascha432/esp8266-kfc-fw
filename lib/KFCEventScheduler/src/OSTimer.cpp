
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
    PORT_MUX_LOCK_ISR_BLOCK(_mux) {
        if (!timer.lock()) {
            return;
        }
    }
    timer.run();
    PORT_MUX_LOCK_ISR_BLOCK(_mux) {
        OSTimer::unlock(timer);
    }
}

void __DBG_printEtsTimer(ETSTimerEx *timerPtr, const char *msg, bool error)
{
    #if DEBUG_OSTIMER
        if (msg == nullptr) {
            msg = emptyString.c_str();
        }
        String prepend;
        if (timerPtr->_magic != ETSTimerEx::kMagic) {
            prepend = PrintString(F("name=invalid ptr=%p "), timerPtr);
        }
        else {
            prepend = PrintString(F("name=%s "), __S(timerPtr->name()));
        }
        if (pgm_read_byte(msg)) {
            prepend += FPSTR(msg);
        }
        if (!prepend.endEquals(F(" "))) {
            prepend += ' ';
        }
        if (error) {
            __DBG_printEtsTimer_E(timerPtr, prepend);
        }
        else {
            __DBG_printEtsTimer(timerPtr, prepend);
        }
    #else
        if (error) {
            __DBG_printEtsTimer_E(*reinterpret_cast<ETSTimer *>(timerPtr), msg);
        }
        else {
            __DBG_printEtsTimer(*reinterpret_cast<ETSTimer *>(timerPtr), msg);
        }

    #endif
}


#if DEBUG_OSTIMER
#   define OSTIMER_INLINE
#   include "OSTimer.hpp"
#endif

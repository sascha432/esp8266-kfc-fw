
/**
  Author: sascha_lammers@gmx.de
*/

#include <Arduino_compat.h>
#include "OSTimer.h"

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#if ESP32
std::list<ETSTimerEx *> ETSTimerEx::_timers;
#endif

void ICACHE_FLASH_ATTR OSTimer::_EtsTimerCallback(void *arg)
{
    auto &timer = *reinterpret_cast<OSTimer *>(arg);
    // #if DEBUG_OSTIMER
    //     __DBG_printf("timer run name=%s running=%u locked=%u", timer._etsTimer._name, timer.isRunning(), timer.isLocked(timer));
    // #endif
    PORT_MUX_LOCK_ISR_BLOCK(timer.getMux()) {
        if (!timer.lock()) {
            return;
        }
        __muxLock.exit();
        timer.run();
        __muxLock.enter();
        OSTimer::unlock(timer);
    }
}

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

#if ESP8266

static void ___DBG_printEtsTimerRaw(ETSTimerEx &timer, const char *msg)
{
    #if DEBUG_OSTIMER
        if (timer._magic != ETSTimerEx::kMagic) {
            DEBUG_OUTPUT.printf_P(PSTR("name=invalid ptr=%p "), &timer);
        }
        else {
            DEBUG_OUTPUT.printf_P(PSTR("name=%s "), __S(timer.name()));
        }
    #endif

    if (msg) {
        DEBUG_OUTPUT.print(FPSTR(msg));
        // append space if it does not a trailing space
        if (pgm_read_byte(msg) && !str_endswith_P(msg, ' ')) {
            DEBUG_OUTPUT.print(' ');
        }
    }
    DEBUG_OUTPUT.printf_P(PSTR("timer=%p func=%p arg=%p period=%u next=%p"), &timer, timer.timer_func, timer.timer_arg, timer.timer_period, timer.timer_next);
}

void ___DBG_printEtsTimer(ETSTimerEx &timer, const char *msg)
{
    ___DBG_printEtsTimerRaw(timer, msg);
    DEBUG_OUTPUT.print(F(__DBG_newline));
    DEBUG_OUTPUT.flush();
}

void ___DBG_printEtsTimer_E(ETSTimerEx &timer, const char *msg)
{
    DEBUG_OUTPUT.printf_P(PSTR(_VT100(bold_red)));
    ___DBG_printEtsTimerRaw(timer, msg);
    DEBUG_OUTPUT.print(F(_VT100(reset) __DBG_newline));
    DEBUG_OUTPUT.flush();
}

#endif

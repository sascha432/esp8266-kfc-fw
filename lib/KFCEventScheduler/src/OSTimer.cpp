
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
    // MUTEX_LOCK_BLOCK(timer.getLock())
    {
        if (!timer.lock()) {
            return;
        }
        // __lock.unlock();
        timer.run();
        // __lock.lock();
        OSTimer::unlock(timer);
    }
}

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

static void ___DBG_printEtsTimerRaw(const ETSTimerEx &timer, const char *msg)
{
    #if DEBUG_OSTIMER
        #if ESP8266
            if (timer._magic != ETSTimerEx::kMagic) {
                DEBUG_OUTPUT.printf_P(PSTR("name=invalid ptr=%p "), &timer);
            }
            else
        #endif
        {
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
    #if ESP8266
        DEBUG_OUTPUT.printf_P(PSTR("timer=%p func=%p arg=%p period=%u next=%p"), &timer, timer.timer_func, timer.timer_arg, timer.timer_period, timer.timer_next);
    #elif ESP32
        if (timer._timer) {
            DEBUG_OUTPUT.printf_P(PSTR("timer=%p alarm=%llu func=%p arg=%p period=%llu prev %p next=%p"), &timer, timer._timer->alarm, timer._timer->callback, timer._timer->arg, timer._timer->period, timer._timer->list_entry.le_prev, timer._timer->list_entry.le_next);
        }
        else {
            DEBUG_OUTPUT.printf_P(PSTR("timer=%p _timer=nullptr"), &timer);
        }
    #else
        DEBUG_OUTPUT.printf_P(PSTR("timer=%p"), &timer);
    #endif
}

void ___DBG_printEtsTimer(const ETSTimerEx &timer, const char *msg)
{
    ___DBG_printEtsTimerRaw(timer, msg);
    DEBUG_OUTPUT.print(F(__DBG_newline));
    DEBUG_OUTPUT.flush();
}

void ___DBG_printEtsTimer_E(const ETSTimerEx &timer, const char *msg)
{
    DEBUG_OUTPUT.printf_P(PSTR(_VT100(bold_red)));
    ___DBG_printEtsTimerRaw(timer, msg);
    DEBUG_OUTPUT.print(F(_VT100(reset) __DBG_newline));
    DEBUG_OUTPUT.flush();
}

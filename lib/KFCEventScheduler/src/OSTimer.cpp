
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

void ICACHE_FLASH_ATTR OSTimer::_OSTimerCallback(void *arg)
{
    auto &timer = *reinterpret_cast<OSTimer *>(arg);
    MUTEX_LOCK_BLOCK(timer.getLock()) {
        if (!timer.lock()) {
            return;
        }
        #if DEBUG_OSTIMER
            timer._etsTimer._called++;
        #endif
        __lock.unlock();
        timer.run();
        __lock.lock();
        OSTimer::unlock(timer);
    }
}

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

#include "Scheduler.h"

#if ESP8266 || _MSC_VER

void dumpTimers(Print &output)
{
    ETSTimer *cur = timer_list;
    while(cur) {
        void *callback = nullptr;
        for(const auto timer: __Scheduler.__getTimers()) {
            if (reinterpret_cast<ETSTimer *>(&timer->_etsTimer) == cur) {
                callback = lambda_target(timer->_callback);
                break;
            }
        }
        float period_in_s = NAN;
        auto timeUnit = emptyString.c_str();
        if (cur->timer_period) {
            timeUnit = PSTR("s");
            period_in_s = cur->timer_period / 312500.0;
            if (period_in_s < 1) {
                period_in_s /= 1000.0;
                timeUnit = PSTR("ms");
            }
        }
        output.printf_P(PSTR("ETSTimer=%p func=%p arg=%p period=%u (%.3f%s) exp=%u callback=%p"),
            cur,
            cur->timer_func,
            cur->timer_arg,
            cur->timer_period,
            period_in_s,
            timeUnit,
            cur->timer_expire,
            callback
        );

        #if DEBUG_OSTIMER
            if (cur->timer_func == reinterpret_cast<ETSTimerFunc *>(ETSTimerEx::_EtsTimerLockedCallback) || cur->timer_func == reinterpret_cast<ETSTimerFunc *>(OSTimer::_OSTimerCallback)) {
                auto timer = reinterpret_cast<ETSTimerEx *>(cur);
                output.printf_P(PSTR(" ETSTimerEx=%p running=%p locked=%p callback=%p called=%u called_lock=%u name=%s"),
                    timer,
                    timer->isRunning(),
                    timer->isLocked(),
                    callback,
                    timer->getStatsCalled(),
                    timer->getStatsCalledWhileLocked(),
                    timer->name()
                );
            }
        #endif

        output.println();

        cur = cur->timer_next;
    }
    #if DEBUG_EVENT_SCHEDULER
        output.println(F("Event::Scheduler"));
        __Scheduler.__list(false);
    #endif
}

#elif ESP32

#include <sys/reent.h>

void dumpTimers(Print &output)
{
    for(const auto timer: ETSTimerEx::_timers) {
        void *callback = nullptr;
        for(const auto timer: __Scheduler.__getTimers()) {
            if (&timer->_etsTimer == reinterpret_cast<void *>(timer)) {
                callback = lambda_target(timer->_callback);
                break;
            }
        }
        output.printf_P(PSTR("ETSTimerEx=%p running=%u locked=%u callback=%p"),
            timer,
            timer->isRunning(),
            timer->isLocked(),
            callback
        );
        #if DEBUG_OSTIMER
            output.printf_P(PSTR(" called=%u called_lock=%u name=%s"),
                timer->getStatsCalled(),
                timer->getStatsCalledWhileLocked(),
                timer->name()
            );
        #endif
        output.println();
    }
    #if DEBUG_EVENT_SCHEDULER
        output.println(F("Event::Scheduler"));
        __Scheduler.__list(false);
    #endif
    esp_timer_dump(reinterpret_cast<FILE *>(const_cast<__sFILE_fake *>(&__sf_fake_stdout)));
}

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

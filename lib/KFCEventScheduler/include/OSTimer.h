/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_OSTIMER
#    define DEBUG_OSTIMER (1 || defined(DEBUG_ALL) || _MSC_VER)
#endif

#if DEBUG_OSTIMER
#    define OSTIMER_NAME(name) PSTR(name)
#    define OSTIMER_NAME_VAR(var) var
#    define OSTIMER_NAME_ARG(name) OSTIMER_NAME(name),
#else
#    define OSTIMER_NAME(name)
#    define OSTIMER_NAME_VAR(varname)
#    define OSTIMER_NAME_ARG(name)
#endif

#include <Arduino_compat.h>
#include <PrintString.h>
#include "Event.h"

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

struct ETSTimerEx;

inline void __DBG_printEtsTimer(ETSTimer &timer, const char *msg = nullptr);
inline void __DBG_printEtsTimer(ETSTimerEx *timerPtr, const String &msg);
extern void __DBG_printEtsTimer(ETSTimerEx *timerPtr, const char *msg = nullptr, bool error = false);
inline void __DBG_printEtsTimer_E(ETSTimer &timer, const char *msg = nullptr);
inline void __DBG_printEtsTimer_E(ETSTimerEx *timerPtr, const String &msg);
inline void __DBG_printEtsTimer_E(ETSTimerEx *timerPtr, const char *msg = nullptr);

#if ESP32

    #include <esp_timer.h>
    #include <list>

    struct ETSTimerEx {

        #if DEBUG_OSTIMER
            ETSTimerEx(const char *name);
        #else
            ETSTimerEx();
        #endif
        ~ETSTimerEx();

        void create(esp_timer_cb_t callback, void *arg);
        void arm(int32_t delay, bool repeat, bool millis);
        bool isRunning() const;
        bool isDone() const;
        bool isLocked() const;
        void lock();
        void unlock();
        void disarm();
        void done();
        void clear();
        ETSTimerEx *find();
        static ETSTimerEx *find(ETSTimerEx *timer);

        // terminate all OSTimer instances
        static void end();

        #if DEBUG_OSTIMER
            const char *_name;
        #endif
        esp_timer_handle_t _timer;
        bool _running;
        bool _locked;

        static std::list<ETSTimerEx *> _timers;
    };

#elif ESP8266 || _MSC_VER

    #if ESP8266

        #include <osapi.h>

    #endif

    #ifdef __cplusplus
    extern "C" {
    #endif

        extern ETSTimer *timer_list;

    #ifdef __cplusplus
    }
    #endif

    // to avoid calling the timer's run() while it is still running, the callback is changed to
    // a dummy function and restored when run() has finished
    //
    // in debug mode the timers integrity is checked to protect against dangling pointers or invalid state
    struct ETSTimerEx : ETSTimer {
        #if DEBUG_OSTIMER
            static constexpr uint32_t kMagic = 0x73f281f1;
            ETSTimerEx(const char *name);
        #else
            ETSTimerEx();
        #endif
        ~ETSTimerEx();

        void create(ETSTimerFunc *callback, void *arg);
        void arm(int32_t delay, bool repeat, bool millis);
        bool isRunning() const;
        bool isDone() const;
        bool isLocked() const;
        void lock();
        void unlock();
        void disarm();
        void done();
        void clear();
        ETSTimerEx *find();
        static ETSTimerEx *find(ETSTimerEx *timer);

        // terminate all OSTimer instances
        static void end();

        static void ICACHE_FLASH_ATTR _EtsTimerLockedCallback(void *arg);

        #if DEBUG_OSTIMER
            uint32_t _magic;
            const char *_name;

            const char *name() const {
                return _name;
            }
        #else
            const char *name() const {
                return PSTR("OSTimer");
            }
        #endif
    };

#endif

class OSTimer {
public:
    #if DEBUG_OSTIMER
        OSTimer() : OSTimer(PSTR("OSTimer")) {
        }
        OSTimer(const char *name);
    #else
        OSTimer();
    #endif
    virtual ~OSTimer();

    virtual void run() = 0;

    void startTimer(Event::OSTimerDelayType delay, bool repeat, bool millis = true);
    virtual void detach();

    bool isRunning() const;
    operator bool() const;

    bool lock();
    static void unlock(OSTimer &timer, uint32_t timeoutMicros);
    static void unlock(OSTimer &timer);
    static bool isLocked(OSTimer &timer);

    static void ICACHE_FLASH_ATTR _EtsTimerCallback(void *arg);

    static portMuxType &getMux() {
        return _mux;
    }

protected:
    ETSTimerEx _etsTimer;
    static portMuxType _mux;

    operator ETSTimerEx *() const {
        return const_cast<ETSTimerEx *>(&_etsTimer);
    }
};

inline void __DBG_printEtsTimer(ETSTimer &timer, const char *msg)
{
    if (!msg) {
        msg = emptyString.c_str();
    }
    __DBG_printf("%stimer=%p func=%p arg=%p period=%u next=%p", msg, &timer, timer.timer_func, timer.timer_arg, timer.timer_period, timer.timer_next);
}

inline void __DBG_printEtsTimer_E(ETSTimer &timer, const char *msg)
{
    if (!msg) {
        msg = emptyString.c_str();
    }
    __DBG_printf_E("%stimer=%p func=%p arg=%p period=%u next=%p", msg, &timer, timer.timer_func, timer.timer_arg, timer.timer_period, timer.timer_next);
}

inline void __DBG_printEtsTimer(ETSTimerEx *timerPtr, const String &msg)
{
    __DBG_printEtsTimer(*reinterpret_cast<ETSTimer *>(timerPtr), msg.c_str());
}

inline void __DBG_printEtsTimer_E(ETSTimerEx *timerPtr, const String &msg)
{
    __DBG_printEtsTimer_E(*reinterpret_cast<ETSTimer *>(timerPtr), msg.c_str());
}

inline void __DBG_printEtsTimer_E(ETSTimerEx *timerPtr, const char *msg)
{
    __DBG_printEtsTimer(timerPtr, msg, true);
}

#if !DEBUG_OSTIMER
#   define OSTIMER_INLINE inline
#   include "OSTimer.hpp"
#endif

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

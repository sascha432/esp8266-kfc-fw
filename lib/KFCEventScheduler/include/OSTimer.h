/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <PrintString.h>
#include "Event.h"

#if DEBUG_OSTIMER
#    if ESP8266
#        define OSTIMER_NAME(name) PSTR(name)
#    else
#        define OSTIMER_NAME(name) strdup_P(PSTR(name))
#    endif
#    define OSTIMER_NAME_VAR(var)  var
#    define OSTIMER_NAME_ARG(name) OSTIMER_NAME(name),
#else
#    define OSTIMER_NAME(name)
#    define OSTIMER_NAME_VAR(varname)
#    define OSTIMER_NAME_ARG(name)
#endif

// keep a list of all timers to check it exists
// using the internal list of the ESP8266
#ifndef DEBUG_OSTIMER_FIND
#    if ESP32
#        define DEBUG_OSTIMER_FIND 0
#    else
#        define DEBUG_OSTIMER_FIND 1
#    endif
#endif

struct ETSTimerEx;

extern void ___DBG_printEtsTimer(const ETSTimerEx &timer, const char *msg);
extern void ___DBG_printEtsTimer_E(const ETSTimerEx &timer, const char *msg);

inline void ___DBG_printEtsTimer(const ETSTimerEx &timer, const String &msg)
{
    ___DBG_printEtsTimer(timer, msg.c_str());
}

inline void ___DBG_printEtsTimer_E(const ETSTimerEx &timer, const String &msg)
{
    ___DBG_printEtsTimer_E(timer, msg.c_str());
}

#define __DBG_printEtsTimer(timer, msg) \
    if (isDebugContextActive()) { \
        debug_prefix(); \
        ___DBG_printEtsTimer(timer, msg); \
    }

#define __DBG_printEtsTimer_E(timer, msg) \
    if (isDebugContextActive()) { \
        debug_prefix(); \
        ___DBG_printEtsTimer_E(timer, msg); \
    }

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

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
        bool isNew() const;
        bool isRunning() const;
        bool isDone() const;
        bool isLocked() const;
        void lock();
        void unlock();
        void disarm();
        void done();
        void clear();
        #if DEBUG_OSTIMER_FIND
            ETSTimerEx *find();
            static ETSTimerEx *find(ETSTimerEx *timer);
        #endif

        // terminate all OSTimer instances
        static void end();

        #if DEBUG_OSTIMER
            const char *_name;
            const char *name() const {
                return _name;
            }
            esp_err_t __debug_ostimer_validate_result(esp_err_t error, const char *func, PGM_P file, int line) const;
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
        static constexpr uint32_t kUnusedMagic = 0x12345678;
        #if DEBUG_OSTIMER
            static constexpr uint32_t kMagic = 0x73f281f1;
            ETSTimerEx(const char *name);
        #else
            ETSTimerEx();
        #endif
        ~ETSTimerEx();

        void create(ETSTimerFunc *callback, void *arg);
        void arm(int32_t delay, bool repeat, bool millis);
        bool isNew() const;
        bool isRunning() const;
        bool isDone() const;
        bool isLocked() const;
        void lock();
        void unlock();
        void disarm();
        void done();
        void clear();
        #if DEBUG_OSTIMER_FIND
            ETSTimerEx *find();
            static ETSTimerEx *find(ETSTimerEx *timer);
        #endif

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

    MutexSemaphore &getLock();

protected:
    ETSTimerEx _etsTimer;
    MutexSemaphore _lock;
};

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

#include "OSTimer.hpp"

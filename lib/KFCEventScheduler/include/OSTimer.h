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

void dumpTimers(Print &output);

struct ETSTimerEx;
class OSTimer;

#if DEBUG

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

#else

#define __DBG_printEtsTimer(...)        ;
#define __DBG_printEtsTimer_E(...)      ;

#endif

#ifndef _MSC_VER
#    pragma GCC push_options
#    pragma GCC optimize("O3")
#endif

#if ESP32

    #include <esp_timer.h>
    #include <list>

#elif ESP8266 || _MSC_VER

    #if ESP8266
        #include <osapi.h>
    #endif

    extern "C" ETSTimer *timer_list;

#endif

// ETSTimerEx is using ETSTimer on ESP8266 and esp_timer on ESP32 and adds
// debug functions to ensure the consistency of the timer states. interrupt
// locking and semaphores are not built-in, those are in OSTimer and the
// EventScheduler

struct ETSTimerEx
    #if ESP8266 || _MSC_VER
        : ETSTimer
    #endif
{

    static constexpr uint32_t kUnusedMagic = 0x12345678;

    #if DEBUG_OSTIMER
        ETSTimerEx(const char *name);
    #else
        ETSTimerEx();
    #endif
    ~ETSTimerEx();

    #if ESP32
        void create(esp_timer_cb_t callback, void *arg);
    #else
        void create(ETSTimerFunc *callback, void *arg);
    #endif
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
        char *_name;

        const char *name() const {
            return _name;
        }

        uint32_t _called;
        uint32_t _calledWhileLocked;

        uint32_t getStatsCalled() const {
            return _called;
        }

        uint32_t getStatsCalledWhileLocked() const {
            return _calledWhileLocked;
        }

        #if ESP32

            esp_err_t __debug_ostimer_validate_result(esp_err_t error, const char *func, PGM_P file, int line) const;

        #elif ESP8266

            static constexpr uint32_t kMagic = 0x73f281f1;
            uint32_t _magic;

        #endif

    #else

        const char *name() const {
            return PSTR("ETSTimerEx");
        }

        uint32_t getStatsCalled() const {
            return -1;
        }

        uint32_t getStatsCalledWhileLocked() const {
            return -1;
        }

    #endif

    #if ESP32

        using ETSTimerExCallback = esp_timer_cb_t;

        esp_timer_handle_t _timer;
        bool _running;
        bool _locked;

        static std::list<ETSTimerEx *> _timers;

    #elif ESP8266 || _MSC_VER

        using ETSTimerExCallback = ETSTimerFunc *;

        // locking is implemented by changing the timer callback to _EtsTimerLockedCallback to
        // avoid using extra memory for locking. this also avoid any extra overhead checking
        // locks/locking interrupts
        static void ICACHE_FLASH_ATTR _EtsTimerLockedCallback(OSTimer *arg);

    #endif

};


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

    static void ICACHE_FLASH_ATTR _OSTimerCallback(OSTimer *arg);

    SemaphoreMutex &getLock();

protected:
    friend ETSTimerEx;

    ETSTimerEx _etsTimer;
    SemaphoreMutex _lock;
};

#ifndef _MSC_VER
#    pragma GCC pop_options
#endif

#include "OSTimer.hpp"

/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_OSTIMER
#    define DEBUG_OSTIMER 0
#endif

#if DEBUG_OSTIMER
#    define OSTIMER_NAME(name) PSTR(name)
#else
#    define OSTIMER_NAME(name)
#endif

#include <Arduino_compat.h>
#if defined(ESP8266)
    #include <osapi.h>

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

        bool isRunning() const;
        bool isDone() const;
        bool isLocked() const;
        void lock();
        void unlock();
        void disarm();
        void done();
        void clear();
        ETSTimer *find();
        static ETSTimer *find(ETSTimer *timer);

        static void ICACHE_FLASH_ATTR _EtsTimerLockedCallback(void *arg);

        #if DEBUG_OSTIMER
            uint32_t _magic;
            const char *_name;
        #endif
    };

#endif

class OSTimer {
public:
    #if DEBUG_OSTIMER
        OSTimer(const char *name);
    #else
        OSTimer();
    #endif
    virtual ~OSTimer();

    virtual void run() = 0;

    void startTimer(int32_t delay, bool repeat, bool millis = true);
    virtual void detach();

    bool isRunning() const;
    operator bool() const;

    bool lock();
    static void unlock(OSTimer &timer, uint32_t timeoutMicros);
    static void unlock(OSTimer &timer);
    static bool isLocked(OSTimer &timer);

    static void ICACHE_FLASH_ATTR _EtsTimerCallback(void *arg);

protected:
    ETSTimerEx _etsTimer;

    operator ETSTimer *() const {
        return const_cast<ETSTimerEx *>(&_etsTimer);
    }
};

// #if !DEBUG_OSTIMER
// #   include "OSTimer.hpp"
// #endif

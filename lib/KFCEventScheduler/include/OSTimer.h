/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#ifndef DEBUG_OSTIMER
#    define DEBUG_OSTIMER 1
#endif

#define OSTIMER_USE_MAGIC DEBUG_OSTIMER
#define OSTIMER_USE_FIND_TIMER ESP8266

#if DEBUG_OSTIMER
#    define OSTIMER_NAME(name) PSTR(name)
#else
#    define OSTIMER_NAME(name)
#endif

struct TimerArg {
    void *_arg;
    TimerArg(const void *arg = nullptr) : _arg(const_cast<void *>(arg))
    {}
    operator const void *() const {
        return this;
    }
    operator void *() {
        return this;
    }
};

extern void ICACHE_FLASH_ATTR _EtsTimerLockedCallback(void *arg);

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

    #define OSTIMER_LOCKED_PTR reinterpret_cast<ETSTimerFunc *>(_EtsTimerLockedCallback)

    struct ETSTimerEx : ETSTimer {
        static constexpr uint32_t kMagic = 0x73f281f1;
        ETSTimerEx();
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
        uint32_t _magic;
    };

#endif

class OSTimer {
public:
    static constexpr uint32_t kMagic = 0x73f281f1 & 0x7fffffff;

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

    // ETSTimerEx *_getEtsTimer() const {
    //     return const_cast<ETSTimerEx *>(&_etsTimer);
    // }

    static void ICACHE_FLASH_ATTR _EtsTimerCallback(void *arg);

protected:
    ETSTimerEx _etsTimer;

    operator ETSTimer *() const {
        return const_cast<ETSTimerEx *>(&_etsTimer);
    }

    operator TimerArg() const {
        return TimerArg(reinterpret_cast<const void *>(this));
    }

    #if OSTIMER_USE_MAGIC
        volatile uint32_t _magic: 31;
        volatile uint32_t _locked: 1;
    #endif
    #if DEBUG_OSTIMER
        const char *_name;
        uint32_t _delay;
    #endif
};

// #if !DEBUG_OSTIMER
// #   include "OSTimer.hpp"
// #endif

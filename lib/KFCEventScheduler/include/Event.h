/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <chrono>

#ifndef DEBUG_EVENT_SCHEDULER
#define DEBUG_EVENT_SCHEDULER                           0
#endif

#ifndef DISABLE_GLOBAL_EVENT_SCHEDULER
#define DISABLE_GLOBAL_EVENT_SCHEDULER                  0
#endif

#ifndef DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR
#define DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR   1
#endif

#if DEBUG_EVENT_SCHEDULER
#define __DBG_Event_Timer_store_position()              __DBG_store_position()
#define _Scheduler                                      ((__DBG_store_position()) ? __Scheduler : __Scheduler)
#define _Timer(obj)                                     ((__DBG_store_position()) ? obj : obj)
#else
#define _Scheduler                                      __Scheduler
#define _Timer(obj)                                     obj
#endif

#pragma push_macro("HIGH")
#pragma push_macro("LOW")
#undef HIGH
#undef LOW

namespace Event {

    class Timer;
    class CallbackTimer;
    class Scheduler;

    static constexpr uint32_t kMinDelay = 5;
    static constexpr uint32_t kMaxDelay = 0x68D7A3;                                              // 6870947ms / 6870.947 seconds

    static constexpr uint64_t kMaxDelayMillis = (1ULL << 17) * (uint64_t)kMaxDelay + (kMaxDelay - 1);
    static constexpr uint64_t kMaxDelaySeconds = kMaxDelayMillis / 1000;
    static constexpr uint64_t kMaxDelayInDays  = kMaxDelaySeconds / 86400;

    // ESP8266 for "os_timer_arm"
    // - if system_timer_reinit has been called, the timer value allowed range from 100 to 0x689D0.
    // - if didn’t call system_timer_reinit has NOT been called, the timer value allowed range from 5 to 0x68D7A3.

    using CallbackTimerPtr = CallbackTimer *;
    using TimerVector = std::vector<CallbackTimerPtr>;
    using TimerVectorIterator = TimerVector::iterator;
    using Callback = std::function<void(CallbackTimerPtr timer)>;

    using milliseconds = std::chrono::duration<int64_t, std::ratio<1>>;

    template<typename T>
    constexpr milliseconds seconds(T seconds) {
        return milliseconds((milliseconds::rep)seconds * (milliseconds::rep)1000);
    }

    template<typename T>
    constexpr milliseconds minutes(T minutes) {
        return milliseconds((milliseconds::rep)minutes * (milliseconds::rep)60000);
    }

    // 6 bit -31...31
    enum class PriorityType : int8_t {
        NONE = -31,
        LOWEST = -24,
        LOWER = -16,
        LOW = -8,
        _LOW = LOW,
        NORMAL = 0,
        HIGH = 8,               // above normal is limited to 15ms runtime
        _HIGH = HIGH,
        HIGHER = 16,
        HIGHEST = 24,
        TIMER = 30,             // run as timer, limit to 5ms runtime. interval limited to 6870946 ms. RepeatType true or false only
        MAX
    };

    class RepeatType {
    public:
        static const uint32_t kUnlimited = ~0;
        static const uint32_t kPreset = 0;
        static const uint32_t kNoRepeat = 1;

        // use preset
        RepeatType();
        // repeat timer
        // false = do not repeat
        // true = repeat unlimited times
        RepeatType(bool repeat);
        // repeat timer "repeat" times
        // 0 = must not be used
        // 1 = do not repeat
        // 2 = repeat once
        // ...
        RepeatType(int repeat);
        RepeatType(uint32_t repeat);

        RepeatType(const RepeatType &repeat);

    protected:
        friend Scheduler;
        friend CallbackTimer;

        bool _doRepeat();
        bool _hasRepeat() const;

        uint32_t _repeat;
    };

}

#pragma pop_macro("HIGH")
#pragma pop_macro("LOW")

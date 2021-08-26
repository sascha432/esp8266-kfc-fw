/**
  Author: sascha_lammers@gmx.de
*/

#pragma once

#include <Arduino_compat.h>
#include <chrono>
#include <time.h>

#ifndef DEBUG_EVENT_SCHEDULER
#    define DEBUG_EVENT_SCHEDULER (0 || defined(DEBUG_ALL))
#endif

#ifndef DISABLE_GLOBAL_EVENT_SCHEDULER
#    define DISABLE_GLOBAL_EVENT_SCHEDULER 0
#endif

#ifndef DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR
#    define DEBUG_EVENT_SCHEDULER_RUNTIME_LIMIT_CONSTEXPR 1
#endif

#ifndef EVENT_SCHEDULER_ASSERT
// #define EVENT_SCHEDULER_ASSERT(cond)                    assert(cond)
#    define EVENT_SCHEDULER_ASSERT(cond) __DBG_assert(cond)
#endif

#if DEBUG_EVENT_SCHEDULER
#    define __DBG_Event_Timer_store_position() __DBG_store_position()
#    define _Scheduler                         ((__DBG_store_position()) ? __Scheduler : __Scheduler)
#    define _Timer(obj)                        ((__DBG_store_position()) ? obj : obj)
#else
#    define _Scheduler  __Scheduler
#    define _Timer(obj) obj
#endif

#pragma push_macro("HIGH")
#pragma push_macro("LOW")
#undef HIGH
#undef LOW

#if DEBUG_EVENT_SCHEDULER
#    include <debug_helper_enable.h>
#else
#    include <debug_helper_disable.h>
#endif

//
// Event::Timer is a self managing timer using the Scheduler
// It has an add() and remove() method and the destructor removes the timer
//
// Event::CallbackTimer is a system timer managed by the Scheduler. It can have a managed timer attached to it
// using disarm or Scheduler.remove(), removes the timer and clears the manager timer, if one is attached
// ! the object must not be deleted
// ! disarm() must be called inside the timer callback
//
// Event::ManangedCallbackTimer links to Event::CallbackTimer and is used internally by Event::Timer
// to keep the Scheduler in-sync
//
// Event::Scheduler is a singleton that manages Event::*Timer objects. It can be used to add()
// and remove() Event::CallbackTimer and Event::Timer. It is recommended to use Event::Timer instead of the
// Scheduler or Event::CallbackTimer


namespace Event {

    class Timer;
    class CallbackTimer;
    class Scheduler;
    class ManangedCallbackTimer;

    #if ESP8266
        static constexpr uint32_t kMinDelay = 5;
        static constexpr uint32_t kMaxDelay = 0x68D7A3;
    #else
        static constexpr uint32_t kMinDelay = 1;
        static constexpr uint32_t kMaxDelay = std::numeric_limits<int32_t>::max();
    #endif

    static constexpr uint64_t kMaxDelayMillis = (1ULL << 17) * (uint64_t)kMaxDelay + (kMaxDelay - 1);
    static constexpr uint64_t kMaxDelaySeconds = kMaxDelayMillis / 1000;
    static constexpr uint64_t kMaxDelayInDays  = kMaxDelaySeconds / 86400;

    // ESP8266 for "os_timer_arm"
    // - if system_timer_reinit has been called, the timer value allowed range from 100 to 0x689D0.
    // - if didn’t call system_timer_reinit has NOT been called, the timer value allowed range from 5 to 0x68D7A3.

    using CallbackTimerPtr = CallbackTimer *;
    using TimerVector = std::vector<CallbackTimerPtr>;
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

    template<typename T>
    constexpr milliseconds milliseconds_cast(T ms) {
        return milliseconds((milliseconds::rep)ms * (milliseconds::rep)1);
    }

    template<typename T>
    constexpr milliseconds hertz(T hertz) {
        return milliseconds((milliseconds::rep)(1000 / hertz));
    }

    enum class PriorityType : int8_t {
        NONE = -127,
        LOWEST = -64,
        LOWER = -32,
        LOW = -16,
        _LOW = LOW,
        NORMAL = 0,
        HIGH = 16,               // above normal is limited to 15ms runtime
        _HIGH = HIGH,
        HIGHER = 32,
        HIGHEST = 64,
        TIMER = 126,             // run as timer, limit to 5ms runtime
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

        uint32_t getRepeatsLeft() const;

    private:
        friend Scheduler;
        friend CallbackTimer;

        bool _doRepeat();
        bool _hasRepeat() const;

    private:
        uint32_t _repeat;
    };

    inline RepeatType::RepeatType() : _repeat(kPreset)
    {
    }

    inline RepeatType::RepeatType(bool repeat) : _repeat(repeat ? kUnlimited : kNoRepeat)
    {
    }

    inline RepeatType::RepeatType(int repeat) : RepeatType(static_cast<uint32_t>(repeat))
    {
    }

    inline bool RepeatType::_doRepeat()
    {
        static_assert(kUnlimited > kNoRepeat, "check kUnlimited");
        if (_repeat == kUnlimited) {
            return true;
        }
        else if (_repeat > kNoRepeat) {
            _repeat--;
            return true;
        }
        return false;
    }

    inline uint32_t RepeatType::getRepeatsLeft() const
    {
        return _repeat - kNoRepeat;
    }

    inline bool RepeatType::_hasRepeat() const
    {
        static_assert(kNoRepeat + 1 > kNoRepeat && kNoRepeat + 1 != kPreset, "check kNoRepeat and kPreset");
        return _repeat > kNoRepeat; // && (_repeat != kPreset);
    }

}

#include <debug_helper_disable.h>

#pragma pop_macro("HIGH")
#pragma pop_macro("LOW")
